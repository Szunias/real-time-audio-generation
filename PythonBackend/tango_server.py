"""
AI Audio Research - Tango Server (Text-to-Audio)
WebSocket server using Tango for AI-generated sound effects.

Port: 8771
Author: Igor Szuniewicz

Note: Tango is a lightweight text-to-audio model.
Uses diffusers library - should be faster than AudioLDM.
"""

import asyncio
import websockets
import json
import time
import numpy as np

# Check for dependencies
TORCH_AVAILABLE = False
DIFFUSERS_AVAILABLE = False

try:
    import torch
    TORCH_AVAILABLE = True
    print(f"? PyTorch {torch.__version__}")
    if torch.cuda.is_available():
        print(f"? CUDA available: {torch.cuda.get_device_name(0)}")
    else:
        print("?? CUDA not available, using CPU")
except ImportError:
    print("? PyTorch not installed")

try:
    from diffusers import AudioLDM2Pipeline
    DIFFUSERS_AVAILABLE = True
    print("? Diffusers available")
except ImportError:
    print("? Diffusers not installed: pip install diffusers")

# Settings
SAMPLE_RATE = 16000
PORT = 8771

# Global model
pipe = None


def load_model():
    """Load Tango model (once at startup)."""
    global pipe
    
    if not TORCH_AVAILABLE or not DIFFUSERS_AVAILABLE:
        print("? Cannot load model - missing dependencies")
        return False
    
    try:
        print("Loading Tango model... (this may take a minute)")
        start = time.time()
        
        # Tango uses AudioLDM2 architecture
        # Model: declare-lab/tango
        model_name = "declare-lab/tango"
        
        pipe = AudioLDM2Pipeline.from_pretrained(model_name, torch_dtype=torch.float16)
        
        # Move to GPU if available
        device = "cuda" if torch.cuda.is_available() else "cpu"
        pipe = pipe.to(device)
        
        load_time = time.time() - start
        print(f"? Model loaded in {load_time:.1f}s on {device.upper()}")
        return True
        
    except Exception as e:
        print(f"? Failed to load model: {e}")
        print("Try: pip install diffusers transformers accelerate")
        return False


def generate_audio(prompt: str, duration: float = 3.0, steps: int = 50) -> bytes:
    """Generate audio from text prompt using Tango."""
    global pipe
    
    if pipe is None:
        raise RuntimeError("Model not loaded")
    
    # Generate audio
    audio = pipe(
        prompt,
        num_inference_steps=steps,
        audio_length_in_s=duration
    ).audios[0]
    
    # Normalize and convert to 16-bit PCM
    audio = np.clip(audio, -1.0, 1.0)
    audio_int16 = (audio * 32767).astype(np.int16)
    
    return audio_int16.tobytes()


async def handle_client(websocket):
    """Handle incoming WebSocket connections."""
    client_addr = websocket.remote_address
    print(f"[{time.strftime('%H:%M:%S')}] Client connected: {client_addr}")
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                prompt = data.get("prompt", "footstep sound")
                duration = data.get("duration", 3.0)
                steps = data.get("steps", 50)  # Lower = faster but lower quality
                
                print(f"[{time.strftime('%H:%M:%S')}] Request: '{prompt}' ({duration}s, {steps} steps)")
                
                # Generate audio
                audio_data = generate_audio(prompt, duration, steps)
                
                gen_time = (time.time() - start_time) * 1000
                print(f"[{time.strftime('%H:%M:%S')}] Generated {len(audio_data)} bytes in {gen_time:.0f}ms")
                
                # Send binary audio data
                await websocket.send(audio_data)
                
            except json.JSONDecodeError:
                audio_data = generate_audio(message)
                await websocket.send(audio_data)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")


async def main():
    """Start the WebSocket server."""
    print("=" * 60)
    print("AI Audio Research - Tango Server")
    print("=" * 60)
    
    # Load model at startup
    if not load_model():
        print("\n? Failed to load model. Exiting.")
        return
    
    print(f"\nServer starting on ws://localhost:{PORT}")
    print("=" * 60)
    print("Tango is optimized for:")
    print("  - Sound effects")
    print("  - Environmental sounds")
    print("  - Short audio clips")
    print("=" * 60)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, "localhost", PORT):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
