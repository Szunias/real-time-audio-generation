# Audio Generation Research - Project Documentation

**Author:** Igor Szuniewicz
**Institution:** Howest - Digital Arts and Entertainment (DAE)
**Academic Year:** 2025-2026

---

## Research Question

**Can machine learning models generate sound effects fast enough for real-time video game gameplay?**

Target latency: **< 100 milliseconds** for responsive player feedback.

---

## Test Results Summary

| Model | Type | Latency | Quality | Real-Time? |
|-------|------|----------|---------|------------|
| Procedural | Mathematical synthesis | ~100ms | 3/10 | YES |
| ElevenLabs | Cloud API | ~4200ms | 9/10 | NO |
| AudioGen | Local GPU | ~3000ms | 7/10 | NO |
| Stable Audio | Local GPU | ~4000ms | 7/10 | NO |
| AudioLDM | Local GPU | ~11000ms | 5/10 | NO |
| **Cached** | Pre-generated | <1ms | varies | YES |

---

## Key Findings

1. **AI models are too slow for real-time generation** - all tested models are 40-110x above the 100ms target
2. **Caching provides a practical solution** - pre-generate during loading, play instantly during gameplay
3. **Speedup factor: 80,000x - 400,000x** when using cached audio

---

## System Architecture

```
Unreal Engine 5.5                    Python Backend
┌──────────────────┐                ┌─────────────────┐
│  AIAudioManager   │◄──WebSocket────►│  Model Servers  │
│  - Caching       │   JSON + PCM    │  - Procedural   │
│  - Latency Log   │                │  - ElevenLabs   │
│  - Pre-warming   │                │  - AudioGen     │
└──────────────────┘                └─────────────────┘
```

---

## Hardware Configuration

- **GPU:** NVIDIA RTX 5070 Laptop (8GB VRAM)
- **CPU:** Intel Core i7-12700H (12th Gen)
- **RAM:** 32GB
- **OS:** Windows 11

---

## Communication Protocol

**Request (JSON):**
```json
{
  "model": "procedural",
  "prompt": "footstep on wooden floor",
  "duration": 2.0
}
```

**Response:** Raw PCM audio (16-bit, mono, 44.1kHz)

---

## Python Servers

| File | Port | Model | Status |
|------|------|-------|--------|
| `procedural_server.py` | 8765 | Mathematical baseline | Working |
| `elevenlabs_server.py` | 8766 | ElevenLabs API | Working |
| `audiogen_server.py` | 8767 | Meta AudioGen | Working |
| `audioldm_server.py` | 8768 | AudioLDM | Working |
| `stable_audio_server.py` | 8769 | Stable Audio | Working |
| `mmaudio_server.py` | 8770 | MMAudio | Working |
| `tango_server.py` | 8771 | Tango | Working |

---

## Conclusion

While current AI models cannot generate audio in real-time, the combination of AI generation with intelligent caching provides a practical solution for game development. Pre-generation during loading screens followed by instant cache playback enables high-quality audio without perceptible latency.

---

*Documentation for graduation work submission.*
