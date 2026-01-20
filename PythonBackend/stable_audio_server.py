"""
AI Audio Research - Stable Audio Open Server (2024)
WebSocket server using Stable Audio Open 1.0 for AI-generated sound effects on GPU.

Port: 8769
Author: Igor Szuniewicz

Note: Stable Audio Open is the NEWEST open-source model from Stability AI (Nov 2024)
- Optimized for sound effects and audio
- Uses diffusers library 
- Runs on GPU with CUDA
"""

import asyncio
import websockets
import json
import time
import numpy as np

# Check for dependencies
try:
    import torch
    TORCH_AVAILABLE = True
except ImportError:
    TORCH_AVAILABLE = False
    print("ERROR: PyTorch not installed")

try:
    from diffusers import StableAudioPipeline
    DIFFUSERS_AVAILABLE = True
except ImportError:
    DIFFUSERS_AVAILABLE = False
    print("WARNING: diffusers not installed or outdated. Install with:")
    print("  pip install --upgrade diffusers")

# Settings
MODEL_NAME = "stabilityai/stable-audio-open-1.0"
OUTPUT_SAMPLE_RATE = 44100  # Stable Audio uses 44.1kHz

# Generation settings
DEFAULT_DURATION = 2.0  # seconds
DEFAULT_STEPS = 50  # inference steps (lower = faster, 50-100 recommended)


class StableAudioGenerator:
    """Stable Audio Open model wrapper using diffusers."""
    
    def __init__(self):
        self.pipe = None
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        self.dtype = torch.float16 if self.device == "cuda" else torch.float32
        
        print(f"Device: {self.device}")
        if torch.cuda.is_available():
            print(f"GPU: {torch.cuda.get_device_name(0)}")
            print(f"VRAM: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")
    
    def load_model(self):
        """Load the Stable Audio model (lazy loading)."""
        if self.pipe is None:
            print(f"\nLoading Stable Audio Open: {MODEL_NAME}...")
            print("(First run downloads ~2GB model - please wait)")
            
            self.pipe = StableAudioPipeline.from_pretrained(
                MODEL_NAME, 
                torch_dtype=self.dtype
            )
            self.pipe = self.pipe.to(self.device)
            
            # Enable memory optimizations for GPU
            if self.device == "cuda":
                try:
                    self.pipe.enable_model_cpu_offload()
                    print("Memory optimization: CPU offload enabled")
                except Exception as e:
                    print(f"CPU offload not available: {e}")
            
            print("Model loaded successfully!\n")
    
    def generate(self, prompt: str, duration: float = DEFAULT_DURATION, 
                 steps: int = DEFAULT_STEPS, negative_prompt: str = "") -> tuple[bytes, dict]:
        """Generate audio from text prompt.
        Returns: (pcm_bytes, timing_dict)
        """
        self.load_model()
        
        timing = {}
        
        # Clamp duration (Stable Audio max ~47 seconds)
        audio_length_in_s = min(max(duration, 0.5), 30.0)
        
        # Generate audio
        gen_start = time.time()
        
        generator = torch.Generator(self.device).manual_seed(int(time.time()) % 10000)
        
        with torch.no_grad():
            audio = self.pipe(
                prompt=prompt,
                negative_prompt=negative_prompt if negative_prompt else None,
                num_inference_steps=steps,
                audio_end_in_s=audio_length_in_s,
                generator=generator,
            ).audios[0]
        
        timing['generation_ms'] = (time.time() - gen_start) * 1000
        
        # Convert to 16-bit PCM
        conv_start = time.time()
        
        # audio is a torch tensor or numpy array [samples] or [channels, samples]
        if isinstance(audio, torch.Tensor):
            audio = audio.cpu().numpy()
        
        # Handle stereo/mono
        if audio.ndim > 1:
            audio = audio.mean(axis=0)  # Convert to mono
        
        # Normalize and convert to 16-bit
        audio = np.clip(audio, -1.0, 1.0)
        audio_int16 = (audio * 32767).astype(np.int16)
        pcm_data = audio_int16.tobytes()
        
        timing['conversion_ms'] = (time.time() - conv_start) * 1000
        timing['sample_rate'] = OUTPUT_SAMPLE_RATE
        timing['duration_s'] = len(audio_int16) / OUTPUT_SAMPLE_RATE
        
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
        generator = StableAudioGenerator()
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                prompt = data.get("prompt", "footstep on wooden floor")
                duration = data.get("duration", DEFAULT_DURATION)
                steps = data.get("steps", DEFAULT_STEPS)
                negative_prompt = data.get("negative_prompt", "")
                
                print(f"[{time.strftime('%H:%M:%S')}] Generating: '{prompt}'")
                print(f"  Duration: {duration}s | Steps: {steps}")
                
                if not DIFFUSERS_AVAILABLE:
                    await websocket.send(json.dumps({
                        "error": "diffusers not installed. Run: pip install --upgrade diffusers"
                    }))
                    continue
                
                if generator is None:
                    await websocket.send(json.dumps({
                        "error": "Model not loaded"
                    }))
                    continue
                
                # Generate audio
                audio_data, timing = generator.generate(prompt, duration, steps, negative_prompt)
                
                total_time = (time.time() - start_time) * 1000
                
                print(f"[{time.strftime('%H:%M:%S')}] ✅ Done: {len(audio_data)} bytes ({timing['duration_s']:.2f}s audio)")
                print(f"  ⏱️ Generate: {timing['generation_ms']:.0f}ms | Convert: {timing['conversion_ms']:.0f}ms | Total: {total_time:.0f}ms")
                
                # Send binary PCM data
                await websocket.send(audio_data)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] ❌ Error: {e}")
                import traceback
                traceback.print_exc()
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")


async def main():
    """Start the WebSocket server."""
    host = "localhost"
    port = 8769
    
    print("=" * 60)
    print("AI Audio Research - Stable Audio Open Server (2024)")
    print("=" * 60)
    print(f"Server: ws://{host}:{port}")
    print(f"Model: {MODEL_NAME}")
    print(f"PyTorch: {TORCH_AVAILABLE}")
    print(f"Diffusers: {DIFFUSERS_AVAILABLE}")
    
    if TORCH_AVAILABLE:
        print(f"CUDA: {torch.cuda.is_available()}")
        if torch.cuda.is_available():
            print(f"GPU: {torch.cuda.get_device_name(0)}")
    
    print(f"Output: {OUTPUT_SAMPLE_RATE}Hz, 16-bit, mono PCM")
    print(f"Default steps: {DEFAULT_STEPS} (lower = faster)")
    print("=" * 60)
    print("\n🚀 Model will be downloaded on first request (~2GB)")
    print("📡 Waiting for connections...\n")
    
    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\nServer stopped.")
