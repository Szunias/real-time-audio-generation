# Real-Time Audio Generation for Games

Research on using machine learning to generate sound effects for video games in real-time.

---

![Unreal Engine 5](https://img.shields.io/badge/Unreal-5-blue)
![Python](https://img.shields.io/badge/Python-3.12-green)
![License](https://img.shields.io/badge/license-MIT-blue)

**Author:** Igor Szuniewicz
**Institution:** Howest - DAE
**Year:** 2025-2026

---

## Quick Summary

| Question | Answer |
|----------|--------|
| **Goal** | Generate game audio with AI in under 100ms |
| **Result** | Direct generation: NO / Cached: YES |
| **Solution** | Pre-generate during loading, play instantly from cache |
| **Speedup** | 80,000x faster with caching |

---

## The Problem

Modern game audio relies on pre-recorded sound files. AI generation could offer:
- Infinite variation of sounds
- No storage requirements
- On-demand content creation

**But can it be fast enough for real-time gameplay?**

Target: **< 100ms** (imperceptible delay to player)

---

## Test Results

Latency comparison across different models:

```
MODEL                  LATENCY     QUALITY     REAL-TIME
─────────────────────────────────────────────────────
Cached Audio           < 1ms       varies      YES
Procedural             ~100ms      3/10        YES
ElevenLabs (cloud)     ~4200ms     9/10        NO
AudioGen               ~3000ms     7/10        NO
Stable Audio           ~4000ms     7/10        NO
AudioLDM               ~11000ms    5/10        NO
```

**Conclusion:** All AI models are 30-110x too slow for real-time use.

---

## The Solution: Caching

```
Without Cache                    With Cache
─────────────────────────────────────────────────
Player presses trigger           Player presses trigger
     |                                    |
Request audio (0ms)               Play from cache (0ms)
     |
Wait 3000ms...
     |
Audio plays                      Audio plays instantly

Latency: 3000ms                  Latency: < 1ms
```

**Result:** Pre-generation during loading + instant cache playback = practical solution

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        UNREAL ENGINE 5.5                         │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    AudioManager (C++)                      │ │
│  │  - Request queue (FIFO)                                    │ │
│  │  - Audio cache (SoundId → PCM data)                        │ │
│  │  - Latency logging                                        │ │
│  │  - Pre-generation system                                  │ │
│  └────────────────────────────────────────────────────────────┘ │
│                              │                                  │
│                         WebSocket                              │
│                         JSON + PCM                              │
│                              │                                  │
└──────────────────────────────┼──────────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│                      PYTHON BACKEND                              │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                   Unified Server (Port 8770)               │ │
│  │                                                            │ │
│  │  Available models:                                         │ │
│  │  - procedural   (mathematical synthesis)                   │ │
│  │  - elevenlabs   (cloud API, highest quality)               │ │
│  │  - audiogen     (Meta, good for SFX)                       │ │
│  │  - mmaudio      (CVPR 2025, high quality)                  │ │
│  │  - tango        (AudioLDM2)                                │ │
│  │  - stable_audio (Stability AI)                             │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

---

## Project Structure

```
GW2526_Szuniewicz_Igor_EN/
│
├── UnrealProject/
│   ├── Source/AIAudioResearch/
│   │   ├── Audio/              → Core audio system
│   │   │   ├── AIAudioManager.h/cpp        → Main manager
│   │   │   ├── AIFootstepComponent.h/cpp   → Footstep system
│   │   │   ├── AIAmbienceComponent.h/cpp   → Ambience generator
│   │   │   └── AIPreGenVolume.h/cpp        → Pre-generation trigger
│   │   ├── WebSocket/
│   │   │   └── AIWebSocketClient.h/cpp     → WebSocket communication
│   │   ├── Logging/
│   │   │   └── AILatencyLogger.h/cpp       → CSV latency logging
│   │   └── Testing/
│   │       └── AIAudioTestActor.h/cpp      → Test/preview actor
│   ├── Content/              → Game levels and assets
│   └── Config/               → Engine configuration
│
├── PythonBackend/
│   ├── unified_server.py     → Main server (all models)
│   ├── procedural_server.py  → Fast baseline synthesis
│   ├── elevenlabs_server.py  → ElevenLabs API
│   ├── audiogen_server.py    → Meta AudioGen
│   ├── mmaudio_server.py     → MMAudio
│   ├── tango_server.py       → AudioLDM2
│   ├── stable_audio_server.py→ Stable Audio
│   └── requirements.txt
│
└── Documentation/
    └── PROJECT_SUMMARY.md    → Research findings
```

---

## Installation

### Requirements

- **Unreal Engine 5.5**
- **Python 3.12+**
- **CUDA-capable GPU** (for local models, optional)

### Step 1: Install Python Dependencies

```bash
cd PythonBackend
pip install -r requirements.txt
```

### Step 2: GPU Support (Optional)

For faster local model inference:

```bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124
```

### Step 3: Start the Backend

```bash
cd PythonBackend
python unified_server.py
```

Server runs on `ws://localhost:8770` by default.

### Step 4: Open Unreal Project

Open `UnrealProject/AudioResearch.uproject` in Unreal Engine 5.5

The project auto-connects to the backend on startup.

---

## Usage

### Basic Audio Request

```cpp
// In C++ or Blueprint
AudioManager->RequestSound(
    "footstep on wooden floor",  // Prompt
    "wood_footstep_01",          // Sound ID (for caching)
    1.0f,                        // Duration (seconds)
    "elevenlabs"                 // Model
);
```

### Pre-Generation for Level

```cpp
// Generate all sounds before level starts
TArray<FString> Sounds = {
    "footstep_wood",
    "footstep_concrete",
    "footstep_metal",
    "gunshot_pistol",
    "impact_metal"
};

AudioManager->PreGenerateForLevel(Sounds);

// OnPreGenerationComplete event fires when done
// All sounds now play instantly from cache
```

### Footstep System with Surface Detection

The `AIFootstepComponent` automatically:
1. Detects surface type via line trace
2. Plays appropriate footstep sound
3. Pre-generates nearby surfaces

```cpp
// In your character Blueprint or C++ class
UFootstepComponent* Footsteps = CreateDefaultSubobject<UAIFootstepComponent>(this);

// Configure
Footsteps->DefaultSurface = EFootstepSurface::Stone;
Footsteps->NumVariants = 3;
Footsteps->AIModel = "elevenlabs";
```

---

## API Reference

### Backend Request Format

```json
{
  "model": "elevenlabs",
  "prompt": "footstep on wooden floor",
  "duration": 2.0
}
```

### Backend Response

- **Format:** Raw PCM audio
- **Sample Rate:** 44100 Hz
- **Bit Depth:** 16-bit
- **Channels:** Mono (1)

### Available Models

| Model | Description | GPU Required |
|-------|-------------|--------------|
| `procedural` | Mathematical synthesis (baseline) | No |
| `elevenlabs` | Cloud API, highest quality | No |
| `audiogen` | Meta's AudioGen model | Yes |
| `mmaudio` | MMAudio CVPR 2025 | Yes |
| `tango` | AudioLDM2 text-to-audio | Yes |
| `stable_audio` | Stability AI's model | Yes |

---

## Performance Data

### Latency Breakdown

```
Direct Generation (ElevenLabs example):
┌─────────────────────────────────────────────────────┐
│ Network Request      │    50ms                      │
│ Server Processing    │  4000ms                     │
│ Network Response     │    50ms                      │
│ PCM Conversion       │    10ms                      │
│ SoundWave Creation   │     5ms                      │
├─────────────────────────────────────────────────────┤
│ TOTAL                │  4115ms  (41x too slow)     │
└─────────────────────────────────────────────────────┘

Cached Playback:
┌─────────────────────────────────────────────────────┐
│ Cache Lookup        │     1ms                      │
│ SoundWave Creation  │     1ms                      │
│ Play                │     0ms                      │
├─────────────────────────────────────────────────────┤
│ TOTAL                │     2ms  (instant)          │
└─────────────────────────────────────────────────────┘
```

### Cache Performance

| Metric | Value |
|--------|-------|
| Speedup vs Direct | 80,000x - 400,000x |
| Cache Hit Latency | < 1ms |
| Memory per Sound | ~90 KB (2s @ 44.1kHz 16-bit mono) |

---

## Hardware Configuration

Tests conducted on:

| Component | Specification |
|-----------|---------------|
| GPU | NVIDIA RTX 5070 Laptop (8GB VRAM) |
| CPU | Intel Core i7-12700H |
| RAM | 32GB DDR5 |
| OS | Windows 11 |

---

## Key Findings

1. **AI models are not fast enough** for real-time audio generation
   - Fastest model: ~100ms (procedural)
   - Best quality model: ~4200ms (ElevenLabs)
   - Target: < 100ms

2. **Caching is the practical solution**
   - Pre-generate during loading screens
   - Play from cache in < 1ms
   - 80,000x speedup achieved

3. **Hybrid approach works best**
   - Use AI to generate high-quality assets
   - Cache them for instant playback
   - Fall back to procedural for unexpected sounds

---

## Future Work

- [ ] Streaming audio for very long sounds
- [ ] Adaptive music generation
- [ ] Multi-track ambience systems
- [ ] Voice integration for characters

---

## License

MIT License - free for commercial and personal use.

---

*Graduation work for Digital Arts and Entertainment (DAE), Howest, Belgium.*
