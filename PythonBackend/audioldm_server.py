"""
AI Audio Research - AudioLDM Server (Local GPU)
WebSocket server using AudioLDM for AI-generated sound effects on GPU.

Port: 8768
Author: Igor Szuniewicz

Note: AudioLDM uses diffusers library - no audiocraft dependency issues!
Designed for sound effects, not music.
"""

import asyncio
import websockets
import json
import time
import io
import numpy as np

# Check for dependencies
try:
    import torch
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False
    print("ERROR: PyTorch not installed")

try:
    from diffusers import AudioLDMPipeline
    DIFFUSERS_AVAILABLE = True
except ImportError:
    DIFFUSERS_AVAILABLE = False
    print("WARNING: diffusers not installed. Install with: pip install diffusers")

try:
    import scipy.io.wavfile as wavfile
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    print("WARNING: scipy not installed for WAV processing")


# Settings
MODEL_NAME = "cvssp/audioldm-s-full-v2"  # Small model, faster
# Alternative: "cvssp/audioldm-m-full" (medium, better quality)
OUTPUT_SAMPLE_RATE = 16000  # AudioLDM native rate
INFERENCE_STEPS = 10  # Lower = faster, default 200, minimum ~10


class AudioLDMGenerator:
    """AudioLDM model wrapper using diffusers."""
    
    def __init__(self):
        self.pipe = None
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.dtype = torch.float16 if self.device == "cuda" else torch.float32
        
        print(f"Device: {self.device}")
        if torch.cuda.is_available():
            print(f"GPU: {torch.cuda.get_device_name(0)}")
            print(f"VRAM: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")
    
    def load_model(self):
        """Load the AudioLDM model (lazy loading to save memory)."""
        if self.pipe is None:
            print(f"Loading AudioLDM model: {MODEL_NAME}...")
            print("(This may take a few minutes on first run - downloading model)")
            
            self.pipe = AudioLDMPipeline.from_pretrained(
                MODEL_NAME, 
                torch_dtype=self.dtype
            )
            self.pipe = self.pipe.to(self.device)
            
            # Enable memory optimizations
            if self.device == "cuda":
                try:
                    self.pipe.enable_attention_slicing()
                    print("Memory optimization: attention slicing enabled")
                except:
                    pass
            
            print("Model loaded!")
    
    def generate(self, prompt: str, duration: float = 2.0, steps: int = INFERENCE_STEPS) -> tuple[bytes, dict]:
        """Generate audio from text prompt.
        Returns: (pcm_bytes, timing_dict)
        """
        self.load_model()
        
        timing = {}
        
        # Calculate audio length based on duration
        # AudioLDM generates fixed-length audio, we'll trim if needed
        audio_length_in_s = min(duration, 10.0)  # Max 10 seconds
        
        # Generate audio
        gen_start = time.time()
        with torch.no_grad():
            audio = self.pipe(
                prompt,
                num_inference_steps=steps,
                audio_length_in_s=audio_length_in_s,
            ).audios[0]
        
        timing['generation_ms'] = (time.time() - gen_start) * 1000
        
        # Convert to 16-bit PCM
        conv_start = time.time()
        
        # audio is numpy array, normalize and convert
        audio = np.clip(audio, -1.0, 1.0)
        audio_int16 = (audio * 32767).astype(np.int16)
        pcm_data = audio_int16.tobytes()
        
        timing['conversion_ms'] = (time.time() - conv_start) * 1000
        
        return pcm_data, timing


# Global generator instance
generator = None


async def handle_client(websocket):
    """Handle WebSocket connections."""
    global generator
    
    client_addr = websocket.remote_address
    print(f"\n[{time.strftime('%H:%M:%S')}] Client connected: {client_addr}")
    
    # Initialize generator on first connection
    if generator is None and TORCH_AVAILABLE and DIFFUSERS_AVAILABLE:
        generator = AudioLDMGenerator()
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                prompt = data.get("prompt", "footstep on wooden floor")
                duration = data.get("duration", 2.0)
                steps = data.get("steps", INFERENCE_STEPS)
                
                print(f"[{time.strftime('%H:%M:%S')}] Generating: '{prompt}' ({duration}s, {steps} steps)")
                
                if not DIFFUSERS_AVAILABLE:
                    await websocket.send(json.dumps({
                        "error": "diffusers not installed"
                    }))
                    continue
                
                if generator is None:
                    await websocket.send(json.dumps({
                        "error": "Model not loaded"
                    }))
                    continue
                
                # Generate audio
                audio_data, timing = generator.generate(prompt, duration, steps)
                
                total_time = (time.time() - start_time) * 1000
                
                print(f"[{time.strftime('%H:%M:%S')}] Done: {len(audio_data)} bytes")
                print(f"  ⏱️ Generation: {timing['generation_ms']:.0f}ms | Convert: {timing['conversion_ms']:.0f}ms | Total: {total_time:.0f}ms")
                
                # Send binary PCM data
                await websocket.send(audio_data)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                import traceback
                traceback.print_exc()
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")


async def main():
    """Start the WebSocket server."""
    host = "localhost"
    port = 8768
    
    print("=" * 60)
    print("AI Audio Research - AudioLDM Server (Local GPU)")
    print("=" * 60)
    print(f"Server: ws://{host}:{port}")
    print(f"Model: {MODEL_NAME}")
    print(f"PyTorch Available: {TORCH_AVAILABLE}")
    print(f"Diffusers Available: {DIFFUSERS_AVAILABLE}")
    
    if TORCH_AVAILABLE:
        print(f"CUDA Available: {torch.cuda.is_available()}")
        if torch.cuda.is_available():
            print(f"GPU: {torch.cuda.get_device_name(0)}")
    
    print(f"Inference Steps: {INFERENCE_STEPS} (lower = faster)")
    print(f"Output: {OUTPUT_SAMPLE_RATE}Hz, 16-bit PCM")
    print("=" * 60)
    print("Model will be loaded on first request...")
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
