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
| **Speedup** | 240x faster with cache hits |

---

## The Problem

Modern game audio relies on pre-recorded sound files. AI generation could offer:
- Infinite variation of sounds
- No storage requirements
- On-demand content creation

**But can it be fast enough for real-time gameplay?**

Target: **< 100ms** (imperceptible delay to player)

---

## Research Results

### Latency Comparison (5th-95th percentile)

```
MODEL                  LATENCY       QUALITY (1-5)
──────────────────────────────────────────────────
Procedural DSP         ~100ms        2.4 +- 0.8
MMAudio Small          1650-2650ms   3.5 +- 0.7
MMAudio Large          2450-3000ms   3.9 +- 0.6
AudioGen               2000-3000ms   3.6 +- 0.7
ElevenLabs (cloud)     1800-8300ms*  4.3 +- 0.5
Cached Playback        35-90ms       varies

* High variance due to network jitter
```

**Conclusion:** Only procedural DSP meets interactive SFX budget. All AI models exceed the 100ms target on cache miss.

---

## Quality Assessment

Perceptual quality scores from blind evaluation (60 samples, 1-5 scale):

| Method | Mean | Std Dev | 95% CI |
|--------|------|---------|--------|
| ElevenLabs | 4.3 | 0.5 | [4.0, 4.6] |
| MMAudio Large | 3.9 | 0.6 | [3.6, 4.2] |
| MMAudio Small | 3.5 | 0.7 | [3.1, 3.9] |
| AudioGen | 3.6 | 0.7 | [3.2, 4.0] |
| Procedural DSP | 2.4 | 0.8 | [2.0, 2.9] |

ElevenLabs showed best clarity and transients. Procedural DSP was noted as "synthetic but consistent."

---

## Failure Mode Analysis (500 attempts per method)

| Method | Silent | Timeout | Wrong Duration | Total |
|--------|--------|---------|----------------|-------|
| Procedural DSP | 0% | 0% | 0% | **0%** |
| ElevenLabs | 0.4% | 2.8% | 1.6% | **4.8%** |
| MMAudio Small | 1.8% | 1.2% | 3.8% | **6.8%** |
| MMAudio Large | 3.6% | 3.0% | 6.2% | **12.8%** |
| AudioGen | 2.4% | 1.6% | 4.6% | **8.6%** |

Local AI models occasionally produced silent outputs (2-4% rate).

---

## The Solution: Caching

### Cache Impact (AudioGen model)

| Scenario | Cache Miss | Cache Hit | Pre-Generated | Perceived |
|----------|------------|-----------|---------------|-----------|
| Footstep (first) | 2400ms | - | - | Unacceptable |
| Footstep (repeat) | - | 45ms | <10ms | Perfect |
| Footstep (predicted) | - | - | <10ms | Perfect |
| Ambience enter | 3200ms | 120ms | <20ms | Good |
| UI click | 1800ms | 35ms | <10ms | Excellent |

**Key finding:** Pre-generation reduces perceived latency from "unacceptable" to "perfect."

### Cache Performance Summary

| Metric | Value |
|--------|-------|
| Cache hit latency | 35-90ms |
| Pre-gen playback | <10ms |
| Speedup vs miss | 27x - 320x |
| Memory per sound | ~90 KB (2s @ 44.1kHz 16-bit mono) |

---

## Recommended Methods by Category

Based on quality + practical constraints:

| Category | Recommended | Score | Rationale |
|----------|-------------|-------|-----------|
| **Footsteps** | ElevenLabs | 4.4 | Best transients and realism |
| **Impacts** | MMAudio Large | 4.1 | Good texture, acceptable latency |
| **UI Sounds** | Procedural DSP | 3.1 | Latency critical; AI struggled with <200ms clips |
| **Ambience Loops** | MMAudio Small | 3.9 | Good quality, fastest local model |
| **Music** | MMAudio Large | 3.7 | Best temporal coherence |

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
│  │  - procedural   (mathematical synthesis, ~100ms)           │ │
│  │  - elevenlabs   (cloud API, 1.8-8.3s, highest quality)     │ │
│  │  - audiogen     (Meta, 2.0-3.0s)                           │ │
│  │  - mmaudio      (small: 1.6-2.6s, large: 2.4-3.0s)        │ │
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

## Usage Examples

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

1. **AI models are not fast enough** for direct real-time audio generation
   - Fastest AI model (MMAudio Small): 1650-2650ms
   - Target: < 100ms
   - All AI models exceed budget by 16-80x

2. **Caching enables practical use**
   - Cache hit: 35-90ms (within acceptable range)
   - Pre-generated: <10ms (instant)
   - Speedup: 27x - 320x faster than cache miss

3. **Quality vs Latency trade-off**
   - ElevenLabs: Highest quality (4.3/5), high network variance
   - Local models: Consistent latency, 2-4% silent output rate
   - Procedural: Lowest quality (2.4/5), most reliable

4. **Recommended approach**
   - Use ElevenLabs for critical SFX (footsteps, impacts) with pre-generation
   - Use MMAudio for ambience and music
   - Use procedural DSP for UI sounds where latency is critical
   - Always pre-generate during loading screens

---

## Future Work

- [ ] Streaming audio for very long sounds
- [ ] Context-aware prompt generation based on game state
- [ ] Domain-specific model fine-tuning for game audio
- [ ] Automated quality filtering for generated outputs
- [ ] Multiplayer synchronization considerations

---

## License

MIT License - free for commercial and personal use.

---

*Graduation work for Digital Arts and Entertainment (DAE), Howest, Belgium.*
