"""
AI Audio Research - Procedural Sound Generator (Baseline)
WebSocket server that generates basic waveform sounds for latency comparison.

Port: 8765
Author: Igor Szuniewicz
"""

import asyncio
import websockets
import json
import numpy as np
import struct
import time

# Audio settings
SAMPLE_RATE = 44100
BIT_DEPTH = 16


def generate_procedural_sound(prompt: str, duration: float = 0.5) -> bytes:
    """
    Generate simple procedural sounds based on prompt.
    Returns raw PCM data (16-bit signed, mono).
    """
    num_samples = int(SAMPLE_RATE * duration)
    t = np.linspace(0, duration, num_samples, dtype=np.float32)
    
    prompt_lower = prompt.lower()
    
    if "footstep" in prompt_lower:
        # Footstep: short noise burst with decay
        noise = np.random.uniform(-1, 1, num_samples).astype(np.float32)
        envelope = np.exp(-t * 20)  # Fast decay
        audio = noise * envelope * 0.7
        
    elif "gunshot" in prompt_lower:
        # Gunshot: loud transient + decay
        noise = np.random.uniform(-1, 1, num_samples).astype(np.float32)
        envelope = np.exp(-t * 15)
        # Add some low frequency "boom"
        boom = np.sin(2 * np.pi * 60 * t) * np.exp(-t * 10)
        audio = (noise * 0.6 + boom * 0.4) * envelope
        
    elif "impact" in prompt_lower:
        # Impact: metallic ring
        freq = 800 if "metal" in prompt_lower else 400
        audio = np.sin(2 * np.pi * freq * t) * np.exp(-t * 8)
        # Add some noise
        audio += np.random.uniform(-0.1, 0.1, num_samples).astype(np.float32) * np.exp(-t * 15)
        
    elif "test" in prompt_lower or "tone" in prompt_lower:
        # Simple test tone
        audio = np.sin(2 * np.pi * 440 * t) * 0.5
        
    else:
        # Default: white noise burst
        audio = np.random.uniform(-0.5, 0.5, num_samples).astype(np.float32) * np.exp(-t * 5)
    
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
                prompt = data.get("prompt", "test_tone")
                category = data.get("category", "sfx")
                client_timestamp = data.get("timestamp", 0)
                
                print(f"[{time.strftime('%H:%M:%S')}] Received request: '{prompt}' (category: {category})")
                
                # Generate audio
                audio_data = generate_procedural_sound(prompt)
                
                generation_time = (time.time() - start_time) * 1000
                print(f"[{time.strftime('%H:%M:%S')}] Generated {len(audio_data)} bytes in {generation_time:.2f}ms")
                
                # Send binary audio data
                await websocket.send(audio_data)
                
            except json.JSONDecodeError:
                # If not JSON, treat as plain text prompt
                audio_data = generate_procedural_sound(message)
                await websocket.send(audio_data)
                
            except Exception as e:
                error_msg = json.dumps({"error": str(e)})
                await websocket.send(error_msg)
                print(f"[{time.strftime('%H:%M:%S')}] Error: {e}")
                
    except websockets.exceptions.ConnectionClosed:
        print(f"[{time.strftime('%H:%M:%S')}] Client disconnected: {client_addr}")


async def main():
    """Start the WebSocket server."""
    host = "localhost"
    port = 8765
    
    print("=" * 50)
    print("AI Audio Research - Procedural Sound Server")
    print("=" * 50)
    print(f"Server starting on ws://{host}:{port}")
    print(f"Sample Rate: {SAMPLE_RATE} Hz")
    print(f"Bit Depth: {BIT_DEPTH}-bit")
    print("=" * 50)
    print("Supported prompts:")
    print("  - footstep_wood, footstep_concrete, footstep_*")
    print("  - gunshot")
    print("  - impact_metal, impact_*")
    print("  - test_tone")
    print("=" * 50)
    print("Waiting for connections...\n")
    
    async with websockets.serve(handle_client, host, port):
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nServer stopped.")
