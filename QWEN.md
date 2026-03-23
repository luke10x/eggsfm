# eggsfm — FM Synthesis Chip Reference

## Project Overview
**eggsfm** is a real-time FM synthesis library for games, emulating Yamaha FM chips (OPN, OPL, OPM) using the [ymfm](https://github.com/aaronsgiles/ymfm) library.

---

## Chip Specifications

### 🎹 OPN (YM2612/YM3438) — Sega Genesis/Mega Drive

#### Architecture
- **4 operators** per channel
- **6 channels** (voices)
- **8 algorithms** (operator connection patterns)

#### Global Parameters (per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| ALG | 0–7 | Algorithm (operator routing) |
| FB | 0–7 | Feedback on operator 1 |
| AMS | 0–3 | Amplitude Modulation Sensitivity (LFO → volume) |
| FMS | 0–7 | Frequency Modulation Sensitivity (LFO → pitch) |

#### Operator Parameters (4 per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| DT | -3–+3 | Fine detune |
| MUL | 0–15 | Frequency multiplier (harmonic series) |
| TL | 0–127 | Total Level (0 = loudest, 127 = silent) |
| RS | 0–3 | Rate Scale (key scaling for envelope speed) |
| AR | 0–31 | Attack Rate |
| AM | 0–1 | Amplitude Modulation enable (LFO affects this operator) |
| DR | 0–31 | Decay Rate |
| SR | 0–31 | Sustain Rate |
| SL | 0–15 | Sustain Level (0–15 fraction of max) |
| RR | 0–15 | Release Rate |
| SSG | 0–8 | SSG-EG envelope mode |

#### LFO (Global per chip)
- **Toggle**: Enable/disable
- **Frequency**: 8 preset rates
  - 3.82 Hz, 5.33 Hz, 5.77 Hz, 6.11 Hz, 6.60 Hz, 9.23 Hz, 46.1 Hz, 69.2 Hz
- **AMS**: Affects only operators with AM=1
- **FMS**: Affects pitch of entire channel

#### SSG-EG Envelope Modes
| Value | Mode | Behavior |
|-------|------|----------|
| 0 | Off | Normal EG |
| 1 | Repeat | Loop attack→decay→sustain when EG hits 0 |
| 2 | Hold | EG runs once, holds at 0 or TL |
| 3 | Alternate | Loop forward/backward |
| 4 | Inverted | Start inverted |
| 5–8 | Variants | Combinations of above |

**Note**: SSG-EG doubles EG rate (except attack). Release enters after key-off.

#### Algorithm Diagrams
```
ALG 0: OP1 → OP2 → OP3 → OP4 → OUT
ALG 1: OP1 ─┬→ OP3 → OP4 → OUT
            └→ OP2 ─┘
ALG 2: OP1 → OP3 ─┬→ OP4 → OUT
            OP2 → OP3 ─┘
ALG 3: OP1 → OP2 → OP4 → OUT
            OP3 ─┘
ALG 4: OP1 → OP2 → OUT
            OP3 → OP4 → OUT
ALG 5: OP1 → OP2 → OUT
            OP3 ─┘
            OP4 → OUT
ALG 6: OP1 → OP2 → OUT
            OP3 → OUT
            OP4 → OUT
ALG 7: OP1 →┬─ OUT
            OP2 →┤
            OP3 →┤→ OUT
            OP4 →┘
```

---

### 🎹 OPL3 (YMF262) — Sound Blaster 16

#### Architecture
- **2 operators** per channel (standard mode)
- **4-operator mode**: Pair 2 channels (OPL3 only)
- **18 channels** (or 6× 4-op + 6× 2-op)
- **2 algorithms** (2-op mode)

#### Global Parameters (per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| ALG | 0–1 | Algorithm (0: OP1→OP2→OUT, 1: OP1+OP2→OUT) |
| FB | 0–7 | Feedback on operator 1 |
| waveform | 0–7 | Global waveform select (OPL3: 8 waveforms) |

#### Operator Parameters (2 per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| AM | 0–1 | Tremolo enable (LFO affects amplitude) |
| VIB | 0–1 | Vibrato enable (LFO affects pitch) |
| EG | 0–1 | Envelope Generator type |
| KSR | 0–1 | Key Scale Rate (envelope speed vs pitch) |
| MUL | 0–15 | Frequency multiplier |
| KSL | 0–3 | Key Scale Level (attenuation vs pitch) |
| TL | 0–63 | Total Level (0 = loudest) |
| AR | 0–15 | Attack Rate |
| DR | 0–15 | Decay Rate |
| SL | 0–15 | Sustain Level |
| RR | 0–15 | Release Rate |
| wave | 0–7 | **Per-operator waveform** (OPL3) |

#### Waveforms (OPL3)
| ID | Name | Description |
|----|------|-------------|
| 0 | Sine | Pure sine wave |
| 1 | Half-sine | Sine with flat bottom |
| 2 | Square | Square-like (clipped) |
| 3 | Triangle | Triangle wave |
| 4 | Sawtooth | Saw wave |
| 5 | Pulse | Pulse wave |
| 6 | Double-saw | Dual saw |
| 7 | Sine+noise | Sine with noise |

#### LFO (Global, fixed)
- **Waveform**: Triangle (fixed, not selectable)
- **Speed**: Fixed by chip
- **Depth** (register 0xBD):
  - Tremolo: 1dB (shallow) / 4.8dB (deep)
  - Vibrato: Normal / Double
- **Per-operator**: AM/VIB enable bits

#### 4-Operator Mode (OPL3)
- Pair channels: CH(n) + CH(n+3)
- Operators: 4 total
- Additional registers for OP3/OP4 levels
- Algorithms similar to OPN but limited

---

### 🎹 OPM (YM2151) — Arcade Synths

#### Architecture
- **4 operators** per channel
- **8 channels** (voices)
- **8 algorithms**
- **Stereo output** with panning

#### Global Parameters (per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| ALG | 0–7 | Algorithm |
| FB | 0–7 | Feedback on operator 1 |
| PAN | 0–3 | Panning (0=left, 1=right, 2=center, 3=both) |
| lfo_freq | 0–7 | LFO frequency |
| lfo_wave | 0–3 | LFO waveform |

#### Operator Parameters (4 per patch)
| Parameter | Range | Description |
|-----------|-------|-------------|
| DT1 | 0–3 | Coarse detune (tens of cents) |
| DT2 | 0–3 | Fine detune |
| MUL | 0–15 | Frequency multiplier |
| TL | 0–127 | Total Level |
| KS | 0–3 | Key Scale (envelope speed vs pitch) |
| AR | 0–31 | Attack Rate |
| DR | 0–31 | Decay Rate |
| SR | 0–31 | Sustain Rate |
| SL | 0–15 | Sustain Level |
| RR | 0–15 | Release Rate |
| SSG | 0–7 | SSG-EG (simplified vs OPN) |

#### LFO (Global per chip)
| Parameter | Range | Description |
|-----------|-------|-------------|
| Waveform | 0–3 | Triangle, Saw down, Square, Noise |
| Frequency | 0–7 | 8 preset rates |

#### AMS/FMS (Per Channel, not per operator)
| Parameter | Range | Description |
|-----------|-------|-------------|
| AMS | 0–3 | Amplitude Modulation Sensitivity (LFO → volume) |
| FMS | 0–7 | Frequency Modulation Sensitivity (LFO → pitch) |

**Key difference from OPL**: OPM has **dimmer switches** (0–3/0–7) instead of binary on/off.

#### LFO Waveforms
| ID | Waveform | Description |
|----|----------|-------------|
| 0 | Triangle | Smooth up/down |
| 1 | Saw down | Ramp down |
| 2 | Square | High/low |
| 3 | Noise | Random |

---

## API Reference (eggsfm)

### Module Creation
```cpp
xfm_module* xfm_module_create(int sample_rate, int buffer_frames, xfm_chip_type chip);
void xfm_module_destroy(xfm_module* m);
```

### Patch Management
```cpp
void xfm_patch_set(xfm_module* m, int patch_id, const void* patch_data, 
                   int patch_size, xfm_chip_type patch_type);
```

### Note Control
```cpp
xfm_voice_id xfm_note_on(xfm_module* m, int midi_note, int patch_id, int velocity);
void xfm_note_off(xfm_module* m, xfm_voice_id voice);
```

### Audio Output
```cpp
void xfm_mix(xfm_module* m, int16_t* stream, int frames);
void xfm_mix_song(xfm_module* m, int16_t* stream, int frames);  // Song only
void xfm_mix_sfx(xfm_module* m, int16_t* stream, int frames);   // SFX only
```

### Volume & LFO
```cpp
void xfm_module_set_volume(xfm_module* m, float volume);  // 0.0–1.0
void xfm_module_set_lfo(xfm_module* m, bool enable, int freq);
```

---

## Implementation Notes

### Current State (xfm_api.h)
- ✅ `xfm_patch_opn` — Full OPN support (ALG, FB, AMS, FMS, 4 operators)
- ✅ `xfm_patch_opl` — OPL support (alg, fb, waveform, 2 operators)
- ✅ `xfm_patch_opm` — OPM support (alg, fb, pan, lfo_freq, lfo_wave, 4 operators with DT1/DT2)

### Known Limitations
1. **All chips use YM2612 internally** — OPL/OPM patches are mapped to OPN registers
2. **No true OPL/OPM emulation yet** — Requires separate chip backends in xfm_impl.cpp
3. **LFO is chip-global** — Cannot set per-channel LFO in current API

### Future Work
- [ ] Implement OPL3 backend (ymfm_opl.cpp exists, needs integration)
- [ ] Implement OPM backend (ymfm_opm.cpp exists, needs integration)
- [ ] Add per-channel AMS/FMS for OPM
- [ ] Add 4-operator mode for OPL3
- [ ] Add waveform-per-operator for OPL3

---

## File Structure
```
ingamefm/
├── xfm_api.h              # C99 API header
├── xfm_impl.cpp           # Implementation (currently OPN-only)
├── xfm_synth_editors.h    # ImGui editors for OPN/OPL/OPM
├── synths/                # Multi-chip test bed app
│   ├── synths.cpp         # Main application
│   └── build.sh           # Emscripten build script
└── demo/                  # Full-featured web demo
    ├── demo.cpp           # Main application
    └── build.sh           # Emscripten build script
```

---

## Key Design Decisions

1. **TL is attenuation** — 0 = loudest, 127 = silent (matches hardware)
2. **AR=31 = instant attack** — No fade-in
3. **Voice pooling** — 6 voices per module with priority-based stealing
4. **Dynamic gap timing** — Gap before keyon scales with distance to next note
5. **Sample rate scaling** — Bresenham accumulator maintains correct pitch

---

## References
- [ymfm source](https://github.com/aaronsgiles/ymfm) — Aaron Giles' FM emulator
- [Furnace tracker docs](https://github.com/tildearrow/furnace) — FM patch format reference
- [YM2612 datasheet](https://www.smspower.org/Development/Documents/Chips/YM2612) — Original Yamaha docs
