"""
AI Audio Research - AudioGen Server (Meta AudioCraft)
WebSocket server using AudioGen for AI-generated sound effects.

Port: 8767
Author: Igor Szuniewicz

Note: AudioGen is specifically designed for SFX, unlike MusicGen.
"""

import asyncio
import websockets
import json
import time
import io
import torch

# Check for AudioCraft
try:
    from audiocraft.models import AudioGen
    from audiocraft.data.audio import audio_write
    import torchaudio
    AUDIOCRAFT_AVAILABLE = True
except ImportError:
    AUDIOCRAFT_AVAILABLE = False
    print("WARNING: AudioCraft not installed. Install with: pip install audiocraft")


# Settings
SAMPLE_RATE = 16000  # AudioGen uses 16kHz
MODEL_NAME = "facebook/audiogen-medium"  # Options: audiogen-medium


class AudioGenGenerator:
    """AudioGen model wrapper."""
    
    def __init__(self):
        self.model = None
        self.device = "cuda" if torch.cuda.is_available() else "cpu"
        print(f"Device: {self.device}")
        
        if torch.cuda.is_available():
            print(f"GPU: {torch.cuda.get_device_name(0)}")
            print(f"VRAM: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")
    
    def load_model(self):
        """Load the AudioGen model (lazy loading)."""
        if self.model is None and AUDIOCRAFT_AVAILABLE:
            print(f"Loading AudioGen model: {MODEL_NAME}...")
            self.model = AudioGen.get_pretrained(MODEL_NAME)
            self.model.set_generation_params(duration=3)  # Default 3 second generation
            print("Model loaded!")
    
    def generate(self, prompt: str, duration: float = 3.0) -> bytes:
        """Generate audio from text prompt."""
        self.load_model()
        
        if self.model is None:
            raise RuntimeError("AudioGen model not available")
        
        # Set duration
        self.model.set_generation_params(duration=duration)
        
        # Generate
        with torch.no_grad():
            wav = self.model.generate([prompt])
        
        # Convert to bytes (16-bit PCM)
        audio = wav[0].cpu()  # Shape: [channels, samples]
        
        # Normalize and convert
        audio = audio / audio.abs().max()  # Normalize to [-1, 1]
        audio_int16 = (audio * 32767).to(torch.int16)
        
        # Convert to mono if stereo
        if audio_int16.shape[0] > 1:
            audio_int16 = audio_int16.mean(dim=0, keepdim=True).to(torch.int16)
        
        return audio_int16.numpy().tobytes()


generator = AudioGenGenerator() if AUDIOCRAFT_AVAILABLE else None


async def handle_client(websocket):
    """Handle WebSocket connections."""
    client_addr = websocket.remote_address
    print(f"[{time.strftime('%H:%M:%S')}] Client connected: {client_addr}")
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                prompt = data.get("prompt", "footstep on wooden floor")
                duration = data.get("duration", 3.0)
                
                print(f"[{time.strftime('%H:%M:%S')}] Generating: '{prompt}' ({duration}s)")
                
                if not AUDIOCRAFT_AVAILABLE:
                    await websocket.send(json.dumps({
                        "error": "AudioCraft not installed"
                    }))
                    continue
                
                # Generate
                audio_data = generator.generate(prompt, duration)
                
                generation_time = (time.time() - start_time) * 1000
                print(f"[{time.strftime('%H:%M:%S')}] Done: {len(audio_data)} bytes in {generation_time:.2f}ms")
                
                await websocket.send(audio_data)
                
            except Exception as e:
                await websocket.send(json.dumps({"error": str(e)}))
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Disconnected: {client_addr}")


async def main():
    host = "localhost"
    port = 8767
    
    print("=" * 50)
    print("AI Audio Research - AudioGen Server")
    print("=" * 50)
    print(f"Server: ws://{host}:{port}")
    print(f"AudioCraft Available: {AUDIOCRAFT_AVAILABLE}")
    print(f"CUDA Available: {torch.cuda.is_available()}")
    if torch.cuda.is_available():
        print(f"GPU: {torch.cuda.get_device_name(0)}")
    print("=" * 50)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
