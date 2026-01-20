# Real-Time Audio Generation for Games

![Unreal Engine 5.5](https://img.shields.io/badge/Unreal-5.5-blue?logo=unrealengine)
![Python 3.12](https://img.shields.io/badge/Python-3.12-green?logo=python)
![License](https://img.shields.io/badge/license-MIT-blue)

**Author:** Igor Szuniewicz
**Institution:** Howest - Digital Arts and Entertainment (DAE)
**Academic Year:** 2025-2026

---

## Overview

This research investigates the feasibility of using machine learning models to generate sound effects in real-time for video games. The study measures latency from audio generation request to actual playback.

### Research Question

**Can machine learning models generate sound effects fast enough for real-time gameplay?**

Target latency: **< 100 milliseconds**

---

## Project Structure

```
GW2526_Szuniewicz_Igor_EN/
├── UnrealProject/           # Unreal Engine 5.5 project
│   ├── Source/             # C++ audio system
│   ├── Content/            # Game levels and assets
│   ├── Config/             # Engine configuration
│   └── AudioResearch.uproject
├── PythonBackend/          # Audio generation servers
│   ├── procedural_server.py
│   ├── elevenlabs_server.py
│   ├── audiogen_server.py
│   ├── audioldm_server.py
│   ├── stable_audio_server.py
│   ├── mmaudio_server.py
│   ├── tango_server.py
│   ├── unified_server.py
│   └── requirements.txt
├── Documentation/          # Research documentation
│   └── PROJECT_SUMMARY.md
├── Presentation.pptx       # Final presentation
├── Paper.pdf              # Research paper
└── README.md              # This file
```

---

## Installation

### Requirements
- **Unreal Engine 5.5**
- **Python 3.12+**
- **CUDA-capable GPU** (for local models)

### Python Setup

```bash
cd PythonBackend
pip install -r requirements.txt
```

For GPU support:
```bash
pip install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cu124
```

---

## Running the System

### 1. Start Python Server

```bash
cd PythonBackend

# Procedural generation (baseline - fastest)
python procedural_server.py

# ElevenLabs API (highest quality, requires API key)
set ELEVENLABS_API_KEY=your_key_here
python elevenlabs_server.py
```

### 2. Open Unreal Project

Open `UnrealProject/AudioResearch.uproject` in Unreal Engine 5.5

The project connects to `ws://localhost:8765` by default.

---

## Test Results

| Model | Latency | Quality | Real-Time? |
|-------|---------|---------|------------|
| Procedural | ~100ms | 3/10 | YES |
| ElevenLabs | ~4200ms | 9/10 | NO |
| AudioGen | ~3000ms | 7/10 | NO |
| Stable Audio | ~4000ms | 7/10 | NO |
| AudioLDM | ~11000ms | 5/10 | NO |
| **Cached** | <1ms | varies | YES |

### Key Findings

1. AI models are too slow for real-time generation (40-110x above target)
2. Caching enables practical use (80,000x - 400,000x speedup)
3. Pre-generation during loading + instant playback = viable solution

---

## System Architecture

```
Unreal Engine 5.5                    Python Backend
┌──────────────────┐                ┌─────────────────┐
│  AudioManager    │◄──WebSocket────►│  Model Servers  │
│  - Caching       │   JSON + PCM    │  - Procedural   │
│  - Latency Log   │                │  - ElevenLabs   │
│  - Pre-warming   │                │  - AudioGen     │
└──────────────────┘                │  - Stable Audio │
                                    └─────────────────┘
```

---

## Hardware Test Configuration

- **GPU:** NVIDIA RTX 5070 Laptop (8GB VRAM)
- **CPU:** Intel Core i7-12700H
- **RAM:** 32GB
- **OS:** Windows 11

---

## Conclusion

While current AI models cannot generate audio in real-time, the combination of AI generation with intelligent caching provides a practical solution for game development.

---

*Final submission package for graduation work defense.*
