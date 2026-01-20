"""
AI Audio Research - MMAudio Server (CVPR 2025)
WebSocket server using MMAudio for AI-generated sound effects.

Port: 8772
Author: Igor Szuniewicz

Note: MMAudio is from CVPR 2025 - newest model!
Can do both text-to-audio AND video-to-audio.
GitHub: https://github.com/hkchengrex/MMAudio

INSTALLATION:
    pip install torch torchvision torchaudio
    pip install git+https://github.com/hkchengrex/MMAudio.git
    
    OR clone and install:
    git clone https://github.com/hkchengrex/MMAudio.git
    cd MMAudio
    pip install -e .
"""

import asyncio
import websockets
import json
import time
import numpy as np

# Check for dependencies
TORCH_AVAILABLE = False
MMAUDIO_AVAILABLE = False

try:
    import torch
    TORCH_AVAILABLE = True
    print(f"? PyTorch {torch.__version__}")
    if torch.cuda.is_available():
        print(f"? CUDA available: {torch.cuda.get_device_name(0)}")
        print(f"   VRAM: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")
    else:
        print("?? CUDA not available, using CPU (will be SLOW)")
except ImportError:
    print("? PyTorch not installed")

try:
    from mmaudio.eval_utils import ModelConfig, all_model_cfg, generate
    from mmaudio.model.flow_matching import FlowMatching
    from mmaudio.model.networks import MMAudio
    from mmaudio.model.utils.features_utils import FeaturesUtils
    MMAUDIO_AVAILABLE = True
    print("? MMAudio available")
except ImportError:
    print("? MMAudio not installed")
    print("   Install: pip install git+https://github.com/hkchengrex/MMAudio.git")

# Settings
SAMPLE_RATE = 44100  # MMAudio supports 44.1kHz
PORT = 8772

# Global model
model = None
feature_utils = None
net = None


def load_model(model_size: str = "large_44k_v2"):
    """
    Load MMAudio model.
    
    Available models:
    - small_16k: Smallest, 16kHz
    - small_44k: Small, 44.1kHz  
    - medium_44k: Medium, 44.1kHz
    - large_44k: Large, 44.1kHz (best quality)
    - large_44k_v2: Large v2, 44.1kHz (newest, best)
    """
    global model, feature_utils, net
    
    if not TORCH_AVAILABLE or not MMAUDIO_AVAILABLE:
        print("? Cannot load model - missing dependencies")
        return False
    
    try:
        print(f"Loading MMAudio model ({model_size})... (this may take a minute)")
        start = time.time()
        
        device = "cuda" if torch.cuda.is_available() else "cpu"
        
        # Get model config
        model_cfg: ModelConfig = all_model_cfg[model_size]
        
        # Load model
        net = MMAudio.from_pretrained(model_cfg.model_name).to(device).eval()
        feature_utils = FeaturesUtils(
            tod_vae_ckpt=model_cfg.vae_path,
            synchformer_ckpt=model_cfg.synchformer_path,
            enable_conditions=True,
            mode=model_cfg.mode,
            bigvgan_vocoder_ckpt=model_cfg.bigvgan_16k_path if '16k' in model_size else None
        ).to(device).eval()
        
        model = FlowMatching(net)
        
        load_time = time.time() - start
        print(f"? Model loaded in {load_time:.1f}s on {device.upper()}")
        return True
        
    except Exception as e:
        print(f"? Failed to load model: {e}")
        import traceback
        traceback.print_exc()
        return False


def generate_audio(prompt: str, duration: float = 8.0, steps: int = 25) -> bytes:
    """Generate audio from text prompt using MMAudio."""
    global model, feature_utils, net
    
    if model is None:
        raise RuntimeError("Model not loaded")
    
    device = "cuda" if torch.cuda.is_available() else "cpu"
    
    # Generate using text-only mode
    audio = generate(
        model=model,
        feature_utils=feature_utils,
        net=net,
        prompt=prompt,
        negative_prompt="",  # Can add negative prompts
        duration=duration,
        cfg_strength=4.5,
        num_steps=steps,
        device=device
    )
    
    # Convert to numpy
    audio_np = audio.cpu().numpy()
    
    # Normalize and convert to 16-bit PCM
    audio_np = np.clip(audio_np, -1.0, 1.0)
    audio_int16 = (audio_np * 32767).astype(np.int16)
    
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
                prompt = data.get("prompt", "footstep on wooden floor")
                duration = min(data.get("duration", 8.0), 10.0)  # Max 10s
                steps = data.get("steps", 25)  # Lower = faster
                
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
    print("AI Audio Research - MMAudio Server (CVPR 2025)")
    print("=" * 60)
    
    # Load model at startup
    if not load_model():
        print("\n? Failed to load model. Exiting.")
        print("\nInstallation instructions:")
        print("  pip install git+https://github.com/hkchengrex/MMAudio.git")
        return
    
    print(f"\nServer starting on ws://localhost:{PORT}")
    print("=" * 60)
    print("MMAudio features:")
    print("  - Text-to-audio generation")
    print("  - Video-to-audio generation (sync)")
    print("  - 44.1kHz high quality output")
    print("  - CVPR 2025 state-of-the-art")
    print("=" * 60)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, "localhost", PORT):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
