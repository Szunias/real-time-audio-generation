"""
AI Audio Research - Unified Audio Server
Single WebSocket server for all AI audio models.

Port: 8770
Author: Igor Szuniewicz

Supported models:
- procedural: Fast procedural sounds (instant)
- mmaudio: MMAudio CVPR 2025 (high quality, 44kHz)
- audiogen: Meta AudioGen (best for SFX)
- tango: Tango/AudioLDM2 (text-to-audio)
- stable_audio: Stability AI (SFX specialist)
- tangoflux: TangoFlux (fast flow matching)

API:
{
    "model": "audiogen",
    "prompt": "footstep on wood",
    "duration": 2.0,
    "steps": 25  // optional, for MMAudio/TangoFlux
}
"""

print("[STARTUP] Loading unified server...")
print("[STARTUP] Importing libraries...")

import asyncio
import websockets
import json
import time
import numpy as np
import sys
import gc

print("[STARTUP] Checking PyTorch...")

# Check PyTorch
TORCH_AVAILABLE = False
try:
    import torch
    TORCH_AVAILABLE = True
    print(f"[STARTUP] PyTorch OK: {torch.__version__}")
except ImportError:
    print("[STARTUP] WARNING: PyTorch not available")

# Check ElevenLabs
print("[STARTUP] Checking ElevenLabs...")
ELEVENLABS_AVAILABLE = False
ELEVENLABS_API_KEY = None
import os
try:
    from elevenlabs import ElevenLabs
    ELEVENLABS_API_KEY = os.getenv("ELEVENLABS_API_KEY", "sk_84675b45f2468ff27474ebb2906fa67cb4efc3b49ee11a84")
    if ELEVENLABS_API_KEY:
        ELEVENLABS_AVAILABLE = True
        print(f"[STARTUP] ElevenLabs OK (API key set)")
    else:
        print("[STARTUP] ElevenLabs: No API key")
except ImportError:
    print("[STARTUP] ElevenLabs not installed")

# Settings
PORT = 8770
SAMPLE_RATE = 44100
DEFAULT_MODEL = "procedural"


class UnifiedAudioServer:
    """Unified server managing all audio generation models."""
    
    def __init__(self):
        self.current_model = None
        self.current_model_type = None
        self.device = "cuda" if TORCH_AVAILABLE and torch.cuda.is_available() else "cpu"
        
        print(f"Device: {self.device.upper()}")
        if self.device == "cuda":
            print(f"GPU: {torch.cuda.get_device_name(0)}")
            vram = torch.cuda.get_device_properties(0).total_memory / 1024**3
            print(f"VRAM: {vram:.1f} GB")
    
    def clear_model(self):
        """Unload current model to free VRAM."""
        if self.current_model is not None:
            del self.current_model
            self.current_model = None
        self.current_model_type = None
        gc.collect()
        if TORCH_AVAILABLE and torch.cuda.is_available():
            torch.cuda.empty_cache()
    
    def load_model(self, model_type: str) -> bool:
        """Load specified model (lazy loading with caching)."""
        if self.current_model_type == model_type:
            return True  # Already loaded
        
        # Clear previous model
        if self.current_model_type is not None:
            print(f"  Switching from {self.current_model_type} to {model_type}...")
            self.clear_model()
        
        try:
            if model_type == "procedural":
                self.current_model = "procedural"  # No model needed
                self.current_model_type = "procedural"
                return True
            
            elif model_type == "audiogen":
                print("  Loading AudioGen (facebook/audiogen-medium)...")
                from audiocraft.models import AudioGen
                self.current_model = AudioGen.get_pretrained('facebook/audiogen-medium')
                self.current_model.set_generation_params(duration=5)
                self.current_model_type = "audiogen"
                print("  ✓ AudioGen loaded!")
                return True
            
            elif model_type == "mmaudio":
                print("  Loading MMAudio (small_44k)...")
                from mmaudio.eval_utils import ModelConfig, all_model_cfg
                from mmaudio.model.flow_matching import FlowMatching
                from mmaudio.model.networks import get_my_mmaudio
                from mmaudio.model.utils.features_utils import FeaturesUtils
                
                variant = "small_44k"
                model_cfg = all_model_cfg[variant]
                model_cfg.download_if_needed()
                
                dtype = torch.float32
                if self.device == "cuda":
                    dtype = torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16
                
                feature_utils = FeaturesUtils(
                    tod_vae_ckpt=model_cfg.vae_path,
                    synchformer_ckpt=model_cfg.synchformer_ckpt,
                    enable_conditions=True,
                    mode=model_cfg.mode,
                    bigvgan_vocoder_ckpt=None,
                ).to(self.device, dtype).eval()
                
                net = get_my_mmaudio(model_cfg.model_name)
                net.load_weights(torch.load(model_cfg.model_path, map_location=self.device, weights_only=True))
                net = net.to(self.device, dtype).eval()
                
                self.current_model = {
                    'net': net,
                    'feature_utils': feature_utils,
                    'seq_cfg': model_cfg.seq_cfg,
                    'FlowMatching': FlowMatching,
                    'sample_rate': 44100
                }
                self.current_model_type = "mmaudio"
                print("  ✓ MMAudio loaded!")
                return True
            
            elif model_type == "tango":
                print("  Loading Tango/AudioLDM2...")
                from diffusers import AudioLDM2Pipeline
                dtype = torch.float16 if self.device == "cuda" else torch.float32
                self.current_model = AudioLDM2Pipeline.from_pretrained(
                    "cvssp/audioldm2", torch_dtype=dtype
                ).to(self.device)
                self.current_model_type = "tango"
                print("  ✓ Tango loaded!")
                return True
            
            elif model_type == "stable_audio":
                print("  Loading Stable Audio Open 1.0...")
                from diffusers import StableAudioOpenPipeline
                dtype = torch.float16 if self.device == "cuda" else torch.float32
                self.current_model = StableAudioOpenPipeline.from_pretrained(
                    "stabilityai/stable-audio-open-1.0", torch_dtype=dtype
                ).to(self.device)
                self.current_model_type = "stable_audio"
                print("  ✓ Stable Audio loaded!")
                return True
            
            elif model_type == "tangoflux":
                print("  Loading TangoFlux...")
                from tangoflux import TangoFluxInference
                self.current_model = TangoFluxInference(name='declare-lab/TangoFlux')
                self.current_model_type = "tangoflux"
                print("  ✓ TangoFlux loaded!")
                return True
            
            elif model_type == "elevenlabs":
                if not ELEVENLABS_AVAILABLE:
                    print("  ✗ ElevenLabs not available")
                    return False
                print("  Using ElevenLabs SFX API (cloud, 0 GPU RAM)...")
                self.current_model = ElevenLabs(api_key=ELEVENLABS_API_KEY)
                self.current_model_type = "elevenlabs"
                print("  ✓ ElevenLabs ready!")
                return True
            
            else:
                print(f"  Unknown model: {model_type}, using procedural")
                self.current_model = "procedural"
                self.current_model_type = "procedural"
                return True
                
        except ImportError as e:
            print(f"  ✗ Model {model_type} not available: {e}")
            return False
        except Exception as e:
            print(f"  ✗ Failed to load {model_type}: {e}")
            return False
    
    def generate_procedural(self, prompt: str, duration: float) -> tuple:
        """Generate simple procedural sounds."""
        num_samples = int(SAMPLE_RATE * duration)
        t = np.linspace(0, duration, num_samples, dtype=np.float32)
        prompt_lower = prompt.lower()
        
        if "footstep" in prompt_lower:
            noise = np.random.uniform(-1, 1, num_samples).astype(np.float32)
            envelope = np.exp(-t * 20)
            audio = noise * envelope * 0.7
        elif "gunshot" in prompt_lower:
            noise = np.random.uniform(-1, 1, num_samples).astype(np.float32)
            envelope = np.exp(-t * 15)
            boom = np.sin(2 * np.pi * 60 * t) * np.exp(-t * 10)
            audio = (noise * 0.6 + boom * 0.4) * envelope
        elif "impact" in prompt_lower:
            freq = 800 if "metal" in prompt_lower else 400
            audio = np.sin(2 * np.pi * freq * t) * np.exp(-t * 8)
            audio += np.random.uniform(-0.1, 0.1, num_samples).astype(np.float32) * np.exp(-t * 15)
        else:
            audio = np.random.uniform(-0.5, 0.5, num_samples).astype(np.float32) * np.exp(-t * 5)
        
        audio = np.clip(audio, -1.0, 1.0)
        audio_int16 = (audio * 32767).astype(np.int16)
        return audio_int16.tobytes(), SAMPLE_RATE
    
    def generate(self, model_type: str, prompt: str, duration: float, steps: int = 25) -> tuple:
        """Generate audio using specified model."""
        # Load model if needed
        if not self.load_model(model_type):
            # Fallback to procedural
            print(f"  Falling back to procedural generation")
            return self.generate_procedural(prompt, duration)
        
        start_time = time.time()
        
        try:
            if self.current_model_type == "procedural":
                audio_bytes, sr = self.generate_procedural(prompt, duration)
            
            elif self.current_model_type == "audiogen":
                self.current_model.set_generation_params(duration=duration)
                with torch.no_grad():
                    wav = self.current_model.generate([prompt])
                    audio = wav[0].cpu()
                    if audio.abs().max() > 0:
                        audio = audio / audio.abs().max()
                    audio_int16 = (audio * 32767).to(torch.int16)
                    if audio_int16.shape[0] > 1:
                        audio_int16 = audio_int16.mean(dim=0, keepdim=True).to(torch.int16)
                    audio_bytes = audio_int16.numpy().tobytes()
                    sr = self.current_model.sample_rate
            
            elif self.current_model_type == "mmaudio":
                from mmaudio.eval_utils import generate
                m = self.current_model
                m['seq_cfg'].duration = duration
                m['net'].update_seq_lengths(
                    m['seq_cfg'].latent_seq_len,
                    m['seq_cfg'].clip_seq_len,
                    m['seq_cfg'].sync_seq_len
                )
                
                rng = torch.Generator(device=self.device)
                rng.manual_seed(int(time.time()) % 1000000)
                fm = m['FlowMatching'](min_sigma=0, inference_mode='euler', num_steps=steps)
                
                with torch.no_grad():
                    audio = generate(
                        clip_video=None,
                        sync_video=None,
                        text=[prompt],
                        negative_text=[""],
                        feature_utils=m['feature_utils'],
                        net=m['net'],
                        fm=fm,
                        rng=rng,
                        cfg_strength=4.5
                    )
                audio_np = audio.cpu().float().numpy()
                if audio_np.ndim > 1:
                    audio_np = audio_np.squeeze()
                audio_np = np.clip(audio_np, -1.0, 1.0)
                audio_bytes = (audio_np * 32767).astype(np.int16).tobytes()
                sr = m['sample_rate']
            
            elif self.current_model_type == "tango":
                audio = self.current_model(
                    prompt, num_inference_steps=50, audio_length_in_s=duration
                ).audios[0]
                audio = np.clip(audio, -1.0, 1.0)
                audio_bytes = (audio * 32767).astype(np.int16).tobytes()
                sr = 16000
            
            elif self.current_model_type == "stable_audio":
                audio = self.current_model(
                    prompt, num_inference_steps=50, audio_length_in_s=duration
                ).audios[0]
                if audio.ndim > 1:
                    audio = audio.mean(axis=0)
                audio = np.clip(audio, -1.0, 1.0)
                audio_bytes = (audio * 32767).astype(np.int16).tobytes()
                sr = 44100
            
            elif self.current_model_type == "tangoflux":
                audio = self.current_model.generate(prompt, steps=steps, duration=duration)
                if isinstance(audio, torch.Tensor):
                    audio = audio.cpu().numpy()
                if audio.ndim > 1:
                    audio = audio.squeeze()
                    if audio.ndim > 1:
                        audio = audio.mean(axis=0)
                audio = np.clip(audio, -1.0, 1.0)
                audio_bytes = (audio * 32767).astype(np.int16).tobytes()
                sr = 44100
            
            elif self.current_model_type == "elevenlabs":
                # ElevenLabs SFX API - returns MP3, need to convert to PCM
                import io
                import requests
                from pydub import AudioSegment
                
                print(f"  Calling ElevenLabs SFX API (REST)...")
                # Add randomness by varying prompt_influence (lower = more creative/random)
                import random
                influence = random.uniform(0.3, 0.5)
                print(f"  ElevenLabs: prompt_influence={influence:.2f}")
                
                # Use REST API directly to avoid SDK issues and ensure stability
                # Try higher quality MP3 (192kbps) - standard ElevenLabs format
                api_url = "https://api.elevenlabs.io/v1/sound-generation?output_format=mp3_44100_192"
                
                headers = {
                    "xi-api-key": ELEVENLABS_API_KEY,
                    "Content-Type": "application/json"
                }
                
                payload = {
                    "text": prompt,
                    "prompt_influence": influence
                }
                
                # For long sounds (ambience), we WANT specific duration.
                # For short sounds (footsteps), we let AI decide to avoid robotic stretching.
                if duration > 2.5:
                    payload["duration_seconds"] = duration
                
                # Retry logic for rate limiting (429)
                max_retries = 3
                retry_delay = 2.0  # seconds
                mp3_data = None
                
                for attempt in range(max_retries):
                    try:
                        response = requests.post(api_url, json=payload, headers=headers)
                        response.raise_for_status()
                        mp3_data = response.content
                        break  # Success!
                    except Exception as e:
                        error_str = str(e)
                        if "429" in error_str and attempt < max_retries - 1:
                            wait_time = retry_delay * (2 ** attempt)  # Exponential backoff
                            print(f"  ElevenLabs 429 rate limit - retrying in {wait_time}s (attempt {attempt+1}/{max_retries})...")
                            time.sleep(wait_time)
                            continue
                        print(f"  ElevenLabs REST API Error: {e}")
                        if 'response' in locals():
                            print(f"  Response: {response.text}")
                        raise e
                
                if mp3_data is None:
                    raise Exception("Failed to get ElevenLabs response after retries")
                
                print(f"  Received {len(mp3_data)} bytes MP3 data")
                
                # Convert MP3 to PCM (16-bit mono 44100Hz)
                audio_segment = AudioSegment.from_mp3(io.BytesIO(mp3_data))
                
                # --- POST-PROCESSING ---
                # Only apply aggressive trimming/cropping for SHORT sounds (footsteps)
                if duration <= 2.5:
                    # 1. Trim leading silence (user reported 2s silence)
                    # Use lower threshold (-50dB) to avoid cutting quiet footsteps
                    def detect_leading_silence(sound, silence_threshold=-50.0, chunk_size=10):
                        trim_ms = 0
                        while trim_ms < len(sound) and sound[trim_ms:trim_ms+chunk_size].dBFS < silence_threshold:
                            trim_ms += chunk_size
                        return trim_ms

                    start_trim = detect_leading_silence(audio_segment)
                    
                    # Safety: Don't trim if it would result in empty or very short audio
                    if start_trim >= len(audio_segment) - 100: # Ensure at least 100ms remains
                        print(f"  Warning: Silence detection wanted to trim entire file ({start_trim}ms). Skipping trim (Max dBFS: {audio_segment.max_dBFS:.2f})")
                        start_trim = 0
                        
                    if start_trim > 0:
                        print(f"  Trimming {start_trim}ms silence from start")
                        audio_segment = audio_segment[start_trim:]
                    
                    # 2. Crop to single footstep (max 600ms) to avoid sequences
                    # User reported sequences when duration is unrestricted
                    if len(audio_segment) > 600:
                        print(f"  Cropping to 600ms (original: {len(audio_segment)}ms)")
                        audio_segment = audio_segment[:600].fade_out(50)
                else:
                    print(f"  Long sound ({duration}s) - skipping trim/crop to preserve ambience")
                # -----------------------

                audio_segment = audio_segment.set_frame_rate(44100)
                audio_segment = audio_segment.set_channels(1)
                audio_segment = audio_segment.set_sample_width(2)  # 16-bit = 2 bytes
                
                audio_bytes = audio_segment.raw_data
                sr = 44100
                
                print(f"  Converted to {len(audio_bytes)} bytes PCM (16-bit mono)")
                
                # Log credit usage (approx 40 credits per second)
                credits_used = int(duration * 40)
                print(f"  ElevenLabs: ~{credits_used} credits used")
            
            else:
                audio_bytes, sr = self.generate_procedural(prompt, duration)
            
            gen_time = (time.time() - start_time) * 1000
            return audio_bytes, sr, gen_time
            
        except Exception as e:
            print(f"  ✗ Generation error: {e}")
            # Fallback to procedural
            audio_bytes, sr = self.generate_procedural(prompt, duration)
            gen_time = (time.time() - start_time) * 1000
            return audio_bytes, sr, gen_time


# Global server instance
server = UnifiedAudioServer()


async def handle_client(websocket):
    """Handle incoming WebSocket connections."""
    client_addr = websocket.remote_address
    print(f"\n[{time.strftime('%H:%M:%S')}] Client connected: {client_addr}")
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                model_type = data.get("model", DEFAULT_MODEL)
                prompt = data.get("prompt", "test_tone")
                duration = min(data.get("duration", 2.0), 30.0)  # Max 30s
                steps = data.get("steps", 25)
                
                print(f"[{time.strftime('%H:%M:%S')}] Request: model={model_type}, prompt='{prompt}', duration={duration}s")
                
                # Generate audio
                result = server.generate(model_type, prompt, duration, steps)
                
                if len(result) == 3:
                    audio_data, sample_rate, gen_time = result
                else:
                    audio_data, sample_rate = result
                    gen_time = (time.time() - start_time) * 1000
                
                print(f"[{time.strftime('%H:%M:%S')}] Generated {len(audio_data)} bytes in {gen_time:.0f}ms (SR: {sample_rate}Hz)")
                
                # Send binary audio data
                await websocket.send(audio_data)
                
            except json.JSONDecodeError:
                # Plain text = procedural generation
                audio_bytes, sr = server.generate_procedural(message, 0.5)
                await websocket.send(audio_bytes)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")


async def main():
    """Start the unified WebSocket server."""
    print("=" * 60)
    print("AI Audio Research - UNIFIED Audio Server")
    print("=" * 60)
    print(f"Port: ws://localhost:{PORT}")
    print("=" * 60)
    print("Available models:")
    print("  - procedural   (instant, basic sounds)")
    print("  - elevenlabs   (cloud API, 0 GPU RAM) ⭐")
    print("  - audiogen     (Meta, best for SFX)")
    print("  - mmaudio      (CVPR 2025, high quality)")
    print("  - tango        (AudioLDM2, text-to-audio)")
    print("  - stable_audio (Stability AI, SFX)")
    print("  - tangoflux    (fast flow matching)")
    print("=" * 60)
    print("API: {\"model\": \"audiogen\", \"prompt\": \"...\", \"duration\": 2}")
    print("=" * 60)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, "localhost", PORT):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
