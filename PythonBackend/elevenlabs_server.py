"""
AI Audio Research - ElevenLabs Server (with Credit Tracking)
WebSocket server that uses ElevenLabs API for high-quality SFX generation.

Port: 8766
Author: Igor Szuniewicz

Features:
- MP3 → PCM conversion for Unreal compatibility
- Credit usage tracking (40 credits/second for SFX)
"""

import asyncio
import websockets
import json
import time
import os
import io

# Check if dependencies are installed
try:
    from elevenlabs import ElevenLabs
    ELEVENLABS_AVAILABLE = True
except ImportError:
    ELEVENLABS_AVAILABLE = False
    print("WARNING: ElevenLabs not installed. Install with: pip install elevenlabs")

try:
    from pydub import AudioSegment
    PYDUB_AVAILABLE = True
except ImportError:
    PYDUB_AVAILABLE = False
    print("WARNING: pydub not installed. Install with: pip install pydub")


# Audio settings for output (matching procedural server)
OUTPUT_SAMPLE_RATE = 44100
OUTPUT_CHANNELS = 1
OUTPUT_BIT_DEPTH = 16

# ElevenLabs SFX pricing
CREDITS_PER_SECOND = 40  # As per ElevenLabs documentation


class CreditTracker:
    """Tracks ElevenLabs credit usage for the session."""
    
    def __init__(self):
        self.total_credits_used = 0
        self.total_requests = 0
        self.request_log = []
    
    def log_request(self, prompt: str, duration_seconds: float, generation_time_ms: float):
        credits_used = int(duration_seconds * CREDITS_PER_SECOND)
        self.total_credits_used += credits_used
        self.total_requests += 1
        
        entry = {
            "timestamp": time.strftime("%H:%M:%S"),
            "prompt": prompt,
            "duration_s": duration_seconds,
            "credits": credits_used,
            "latency_ms": generation_time_ms
        }
        self.request_log.append(entry)
        
        return credits_used
    
    def get_summary(self):
        return {
            "total_requests": self.total_requests,
            "total_credits": self.total_credits_used,
            "estimated_cost_usd": self.total_credits_used * 0.00015  # ~$0.15 per 1000 credits approx
        }


credit_tracker = CreditTracker()


class ElevenLabsGenerator:
    """Wrapper for ElevenLabs API with MP3→PCM conversion."""
    
    def __init__(self):
        self.api_key = os.getenv("ELEVENLABS_API_KEY")
        self.client = None
        
        if not self.api_key:
            print("WARNING: ELEVENLABS_API_KEY not set in environment")
            print("Set it with: set ELEVENLABS_API_KEY=your_api_key")
        elif ELEVENLABS_AVAILABLE:
            self.client = ElevenLabs(api_key=self.api_key)
            print("ElevenLabs client initialized")
    
    def generate(self, prompt: str, duration_seconds: float = 2.0) -> tuple[bytes, float, dict]:
        """Generate sound effect using ElevenLabs API and convert to PCM.
        Returns: (pcm_data, actual_duration, timing_info)
        """
        if not self.client:
            raise RuntimeError("ElevenLabs client not initialized")
        
        if not PYDUB_AVAILABLE:
            raise RuntimeError("pydub not installed - cannot convert audio")
        
        timing = {}
        
        try:
            # === STEP 1: Call ElevenLabs API ===
            api_start = time.time()
            print(f"  Calling ElevenLabs API...")
            result = self.client.text_to_sound_effects.convert(
                text=prompt,
                duration_seconds=duration_seconds,
                prompt_influence=0.3  # More AI creativity
            )
            
            # Collect all audio chunks (MP3 data)
            mp3_data = b""
            for chunk in result:
                mp3_data += chunk
            
            timing['api_ms'] = (time.time() - api_start) * 1000
            print(f"  ⏱️ API call: {timing['api_ms']:.2f}ms | Received {len(mp3_data)} bytes MP3")
            
            # === STEP 2: Convert MP3 to PCM ===
            conv_start = time.time()
            audio = AudioSegment.from_mp3(io.BytesIO(mp3_data))
            
            # Get actual duration before resampling
            actual_duration = len(audio) / 1000.0  # pydub uses milliseconds
            
            # Resample to match our output format
            audio = audio.set_frame_rate(OUTPUT_SAMPLE_RATE)
            audio = audio.set_channels(OUTPUT_CHANNELS)
            audio = audio.set_sample_width(OUTPUT_BIT_DEPTH // 8)  # 2 bytes for 16-bit
            
            # Get raw PCM data
            pcm_data = audio.raw_data
            
            timing['convert_ms'] = (time.time() - conv_start) * 1000
            print(f"  ⏱️ MP3→PCM: {timing['convert_ms']:.2f}ms | {len(pcm_data)} bytes ({OUTPUT_SAMPLE_RATE}Hz, {OUTPUT_BIT_DEPTH}-bit)")
            
            return pcm_data, actual_duration, timing
            
        except Exception as e:
            print(f"ElevenLabs API error: {e}")
            raise


generator = ElevenLabsGenerator() if ELEVENLABS_AVAILABLE else None


async def handle_client(websocket):
    """Handle incoming WebSocket connections."""
    client_addr = websocket.remote_address
    print(f"\n[{time.strftime('%H:%M:%S')}] Client connected: {client_addr}")
    
    try:
        async for message in websocket:
            start_time = time.time()
            
            try:
                data = json.loads(message)
                prompt = data.get("prompt", "footstep on wooden floor")
                duration = data.get("duration", 2.0)
                
                print(f"[{time.strftime('%H:%M:%S')}] Generating: '{prompt}'")
                
                if not generator or not generator.client:
                    error_msg = json.dumps({
                        "error": "ElevenLabs not configured. Set ELEVENLABS_API_KEY environment variable."
                    })
                    await websocket.send(error_msg)
                    continue
                
                # Generate audio via ElevenLabs (returns PCM + timing)
                audio_data, actual_duration, timing = generator.generate(prompt, duration)
                
                generation_time = (time.time() - start_time) * 1000
                
                # Track credits
                credits_used = credit_tracker.log_request(prompt, actual_duration, generation_time)
                summary = credit_tracker.get_summary()
                
                print(f"[{time.strftime('%H:%M:%S')}] Total: {generation_time:.2f}ms (API: {timing['api_ms']:.0f}ms + Convert: {timing['convert_ms']:.0f}ms)")
                print(f"  💰 Credits: {credits_used} (this request) | {summary['total_credits']} total | ~${summary['estimated_cost_usd']:.4f}")
                
                # Send binary PCM data
                await websocket.send(audio_data)
                
            except json.JSONDecodeError:
                audio_data, _, _ = generator.generate(message) if generator else (b"", 0, {})
                await websocket.send(audio_data)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")
        
        # Print session summary on disconnect
        summary = credit_tracker.get_summary()
        print(f"\n{'='*50}")
        print(f"SESSION SUMMARY")
        print(f"{'='*50}")
        print(f"Total Requests: {summary['total_requests']}")
        print(f"Total Credits Used: {summary['total_credits']}")
        print(f"Estimated Cost: ${summary['estimated_cost_usd']:.4f}")
        print(f"{'='*50}\n")


async def main():
    """Start the WebSocket server."""
    host = "localhost"
    port = 8766
    
    print("=" * 50)
    print("AI Audio Research - ElevenLabs Server")
    print("=" * 50)
    print(f"Server starting on ws://{host}:{port}")
    print(f"ElevenLabs Available: {ELEVENLABS_AVAILABLE}")
    print(f"Pydub Available: {PYDUB_AVAILABLE}")
    print(f"API Key Set: {'Yes' if os.getenv('ELEVENLABS_API_KEY') else 'No'}")
    print(f"Output: {OUTPUT_SAMPLE_RATE}Hz, {OUTPUT_BIT_DEPTH}-bit, mono PCM")
    print(f"Cost: {CREDITS_PER_SECOND} credits/second")
    print("=" * 50)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        # Print final summary
        summary = credit_tracker.get_summary()
        print(f"\n{'='*50}")
        print(f"FINAL SESSION SUMMARY")
        print(f"{'='*50}")
        print(f"Total Requests: {summary['total_requests']}")
        print(f"Total Credits Used: {summary['total_credits']}")
        print(f"Estimated Cost: ${summary['estimated_cost_usd']:.4f}")
        print(f"{'='*50}")
        print("Server stopped.")
