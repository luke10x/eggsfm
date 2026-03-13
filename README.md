# ingamefm

A C++17 header-only library for real-time FM music and sound effects in games.  
Synthesises audio from patch data using the **ymfm** YM2612 emulator — no audio files needed.

---

## The idea

Traditional game audio loads `.ogg`, `.wav` or `.mp3` files.  ingamefm synthesises everything from FM patch parameters at runtime, the same way a Sega Genesis produced sound in 1990.  The result is:

- **Zero audio assets** — music and SFX are pure numbers in your source code.
- **Tiny footprint** — a full song is a few hundred bytes of text.
- **Fully procedural** — tempo, pitch and timbre can be changed at any time in code.

The workflow is: design patches and songs in [Furnace tracker](https://github.com/tildearrow/furnace) (free, open source), copy the pattern text directly into your C++ source, assign patches to instrument IDs, and call `player.start()`. There is also a companion VST/AUv3 plugin (ARM2612) you can use to design and export patches from inside your DAW.

---

## Dependencies

| Dependency | What for | Where to get |
|---|---|---|
| **ymfm** | YM2612 chip emulation | https://github.com/aaronsgiles/ymfm |
| **SDL 2** | Audio output, events | https://libsdl.org |
| C++17 | Language standard | — |

ymfm is also header-only for the interface; you compile its `.cpp` files alongside your own sources.

---

## Files

| File | Purpose |
|---|---|
| `ingamefm.h` | Single include — pulls in all modules |
| `ingamefm_patchlib.h` | `YM2612Patch` struct + `IngameFMChip` register wrapper |
| `ingamefm_player.h` | Furnace pattern parser + SDL audio playback engine + SFX system |
| `ingamefm_patches.h` | Ready-to-use patch catalogue (bass, lead, pad, percussion) |
| `ingamefm_patch_serializer.h` | Serialize / parse patches to and from human-readable C++ strings |

---

## Demos

| Demo | Platform | What it shows |
|---|---|---|
| `demo.cpp` | Native | Blocking playback of a slap-bass riff |
| `demo2.cpp` | Native | Looping song + live keyboard piano on channel 2 |
| `demo3.cpp` | Native **or** Emscripten (WebGL2) | Aurora shader background + FM guitar played with keyboard/mouse |
| `demo4.cpp` | Native **or** Emscripten (WebGL2) | Full ImGui patch editor with live YM2612 sound design |
| `demo5.cpp` | Native | 4-channel music + 19 game SFX with priority system + volume controls |

demo3 and demo4 use OpenGL ES 3 / WebGL2 and are designed to be built with Emscripten for the web, but also compile and run natively on macOS and Linux with a compatible SDL2 build.  demo1, demo2 and demo5 are pure SDL2 programs with no OpenGL dependency.

---

## Build

### Native — Linux / macOS

Replace the paths with wherever you have ymfm and SDL2.

```bash
YMFM=/path/to/ymfm/src
SDL_INC=/path/to/SDL2/include
SDL_LIB=/path/to/libSDL2.a   # or use $(sdl2-config --libs)

g++ -std=c++17 -O2 \
    -I$YMFM -I$SDL_INC \
    $YMFM/ymfm_misc.cpp $YMFM/ymfm_adpcm.cpp \
    $YMFM/ymfm_ssg.cpp  $YMFM/ymfm_opn.cpp \
    demo5.cpp \
    $SDL_LIB \
    -o demo5 && ./demo5
```

On macOS you also need the system frameworks:

```bash
    -framework Cocoa -framework IOKit -framework CoreVideo \
    -framework CoreAudio -framework AudioToolbox \
    -framework ForceFeedback -framework Carbon \
    -framework Metal -framework GameController -framework CoreHaptics \
    -lobjc
```

### Emscripten — Web (demo3 / demo4 only)

demo3 and demo4 include an OpenGL shader background and require WebGL2.  Build them with `em++`:

```bash
YMFM=/path/to/ymfm/src
IMGUI=/path/to/imgui   # demo4 only

em++ -std=c++17 -O2 \
    -I$YMFM \
    $YMFM/ymfm_misc.cpp $YMFM/ymfm_adpcm.cpp \
    $YMFM/ymfm_ssg.cpp  $YMFM/ymfm_opn.cpp \
    demo3.cpp \
    -s USE_SDL=2 \
    -s FULL_ES3=1 \
    -s MIN_WEBGL_VERSION=2 \
    -s MAX_WEBGL_VERSION=2 \
    -s ALLOW_MEMORY_GROWTH=1 \
    -s ASYNCIFY \
    --shell-file shell.html \
    -o demo3.html

# Serve and open
python3 -m http.server
# → http://localhost:8000/demo3.html
```

For demo4, add the ImGui sources and `-I$IMGUI` and use `shell4.html`.  See the build block at the top of each file for the exact flags.

---

## Quick start — music only

The simplest use: copy a pattern from Furnace, play it, block until done.

```cpp
#include "ingamefm.h"

// Patch — copy values from Furnace's FM editor or ARM2612 plugin
static constexpr YM2612Patch MY_BASS = {
    .ALG = 4, .FB = 5, .AMS = 0, .FMS = 0,
    .op = {
        { .DT=3,.MUL=1,.TL=34,.RS=0,.AR=31,.AM=0,.DR=10,.SR=6,.SL=4,.RR=7,.SSG=0 },
        { .DT=0,.MUL=2,.TL=18,.RS=1,.AR=25,.AM=0,.DR=12,.SR=5,.SL=5,.RR=6,.SSG=0 },
        { .DT=0,.MUL=1,.TL= 0,.RS=0,.AR=31,.AM=0,.DR= 6,.SR=3,.SL=6,.RR=5,.SSG=0 },
        { .DT=0,.MUL=1,.TL= 0,.RS=0,.AR=31,.AM=0,.DR= 7,.SR=2,.SL=5,.RR=5,.SSG=0 },
    }
};

// Pattern — paste directly from Furnace: Edit → Copy Pattern Data
static const char* SONG =
"org.tildearrow.furnace - Pattern Data (8)\n"
"8\n"
"E-2007F|.......\n"
"G-2007F|.......\n"
"A-2007F|.......\n"
"OFF....|.......\n"
"E-2007F|.......\n"
"G-2007F|.......\n"
"A-2007F|.......\n"
"OFF....|.......\n";

int main(int, char**) {
    SDL_Init(SDL_INIT_AUDIO);

    IngameFMPlayer player;
    player.set_song(SONG, /*tick_rate=*/60, /*speed=*/6);
    player.add_patch(0x00, MY_BASS);  // instrument 00 in pattern → MY_BASS
    player.play();                    // blocks until finished

    SDL_Quit();
}
```

---

## Non-blocking playback in a game loop

For a real game you open the audio device yourself, call `start()`, and drive your own event loop.

```cpp
SDL_AudioSpec desired{};
desired.freq     = IngameFMPlayer::SAMPLE_RATE;  // 44100
desired.format   = AUDIO_S16SYS;
desired.channels = 2;
desired.samples  = 128;   // small buffer keeps SFX latency low (~3ms)
desired.callback = IngameFMPlayer::s_audio_callback;
desired.userdata = &player;

SDL_AudioSpec obtained{};
SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);

player.set_song(SONG, 60, 6);
player.add_patch(0x00, MY_BASS);
player.start(dev, /*loop=*/true);   // non-blocking, loops forever
SDL_PauseAudioDevice(dev, 0);

// Game loop
while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) { /* ... */ }
    SDL_WaitEventTimeout(nullptr, 1);  // yield without SDL_Delay latency
}

SDL_CloseAudioDevice(dev);
```

**Important:** use `SDL_WaitEventTimeout` rather than `SDL_Delay(1)` in your event loop.  `SDL_Delay` typically sleeps 10–15 ms on most operating systems regardless of the argument, adding noticeable SFX latency.  `SDL_WaitEventTimeout` returns as soon as any event arrives.

**Buffer size:** `desired.samples = 128` gives ~3 ms worst-case SFX latency (128 / 44100 Hz).  512 is fine for music-only use but makes SFX feel sluggish.  The OS may round up to the next power of two.

---

## Getting patches into your code

### From Furnace tracker

1. Open Furnace and design your patch in the FM editor on any YM2612 channel.
2. Note the four operator rows (OP1–OP4) and the algorithm/feedback settings at the top.
3. Transcribe each value into a `YM2612Patch` struct — the field names match Furnace's labels exactly (`AR`, `DR`, `SR`, `RR`, `SL`, `TL`, `MUL`, `DT`, `RS`, `AM`, `SSG`).

### From the ARM2612 plugin

ARM2612 is a YM2612 VST/AUv3 synthesiser plugin.  Its patch editor has a **Code** tab that serialises the current patch to a C++ string in ingamefm format.  Copy and paste it directly into your source.  The serialised format looks like:

```cpp
static constexpr YM2612Patch PATCH_NAME = {
    .ALG = 4, .FB = 5, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 3, .MUL = 1, .TL = 34, .RS = 0, .AR = 31, .AM = 0,
          .DR = 10, .SR = 6, .SL = 4, .RR = 7, .SSG = 0 },
        // ... OP2, OP3, OP4
    }
};
static constexpr int PATCH_NAME_BLOCK      = 0;  // octave offset
static constexpr int PATCH_NAME_LFO_ENABLE = 0;
static constexpr int PATCH_NAME_LFO_FREQ   = 4;
```

You can also read this format back at runtime using `IngameFMSerializer::parse()` from `ingamefm_patch_serializer.h`.

### Built-in patches

`ingamefm_patches.h` includes a ready-to-use catalogue:

| Constant | Sound |
|---|---|
| `PATCH_SLAP_BASS` | Punchy slap bass |
| `PATCH_SYNTH_BASS` | Fat synthesiser bass |
| `PATCH_ELECTRIC_BASS` | Warm electric bass |
| `PATCH_ACOUSTIC_BASS` | Round acoustic bass |
| `PATCH_GUITAR` | Bright FM guitar stab |
| `PATCH_SUPERSAW` | Dense detuned lead |
| `PATCH_FLUTE` | Soft breathy flute pad |
| `PATCH_KICK` | FM kick drum |
| `PATCH_SNARE` | FM snare |
| `PATCH_HIHAT` | Sharp metallic hi-hat |
| `PATCH_CLANG` | Inharmonic metal clang |

---

## Getting songs into your code

### From Furnace tracker

1. Compose your song in Furnace.  Each channel in your pattern will become one YM2612 channel (up to 4 supported by default; extend `MAX_CHANNELS` to 6 for the full chip).
2. Select all rows in the pattern editor.
3. **Edit → Copy Pattern Data** — this copies the text to the clipboard.
4. Paste it as a C string literal in your source.

The header line (`org.tildearrow.furnace - Pattern Data (N)`) is optional; the parser accepts patterns with or without it.

### Pattern format

```
org.tildearrow.furnace - Pattern Data (N)   ← optional header
<row_count>
<ch0_col>|<ch1_col>|<ch2_col>|...
...
```

Each channel column is exactly 7+ characters:

| Chars | Field | Examples |
|---|---|---|
| 3 | Note | `C-4`  `F#3`  `OFF`  `...` |
| 2 | Instrument (hex) | `00`  `0B`  `..` |
| 2 | Volume (hex) | `7F`  `40`  `..` |
| 3n | Effects (ignored) | `...` |

- `...` in any field means inherit the previous value on that channel.
- `OFF` triggers key-off (note release).
- Volume `7F` = loudest, `00` = silent.  Channels remember the last instrument and volume until a new value is written.
- Effect columns are parsed and silently ignored, so you can paste patterns that include effects directly from Furnace without stripping them.

### Tempo

```
seconds per row = speed / tick_rate
```

`tick_rate` is ticks per second (typically 60).  `speed` is ticks per row (Furnace's Speed setting).  Some example feels:

| tick_rate | speed | ms / row | Character |
|---|---|---|---|
| 60 | 3 | 50 ms | Very fast, arcade |
| 60 | 6 | 100 ms | Fast, action game |
| 60 | 12 | 200 ms | Medium, RPG battle |
| 60 | 20 | 333 ms | Slow, ambient |

---

## Sound effects with the SFX system

The player has a priority-based SFX system that shares YM2612 channels with the music.  You reserve the rightmost N channels as SFX voices; music plays on them freely when idle, and SFX evicts them on demand.

### Setup

```cpp
player.sfx_reserve(2);   // last 2 channels are SFX-capable

// Pre-parse an SFX pattern by id
// id: 0-255, pattern: single-channel Furnace text, same format as songs
player.sfx_define(0, SFX_JUMP_PATTERN, /*tick_rate=*/60, /*speed=*/3);
player.sfx_define(1, SFX_COIN_PATTERN, 60, 3);
```

`sfx_define` can be called at any time, even while audio is running.  The pattern is pre-parsed once and stored.

### Triggering SFX

```cpp
// Must be called with the SDL audio device locked
SDL_LockAudioDevice(dev);
player.sfx_play(/*id=*/0, /*priority=*/4, /*duration_rows=*/10);
SDL_UnlockAudioDevice(dev);
```

`sfx_play` fires immediately — the first note key-on happens before the function returns, so the sound starts within the next audio buffer (~3 ms with `samples=128`).  It does not wait for the next musical tick.

### Priority rules

- The player scans reserved voices right-to-left.
- It prefers a free voice.  If none is free, it picks the rightmost voice with **strictly lower** priority than the incoming SFX.
- If all reserved voices have equal or higher priority, the call is silently ignored (the currently playing sounds are protected).
- When an SFX expires its voice is silently released.  Music resumes on it automatically the next time the song touches that channel.
- Priority 0 is reserved for music.  Use 1 or higher for SFX.

### Suggested priority scale

| Priority | Use |
|---|---|
| 1–2 | Ambient / background (footsteps, breathing, climb) |
| 3–4 | Common gameplay (jump, coin, attack, talk) |
| 5–6 | Significant events (damage, fanfare, launch, scream) |
| 7–9 | Critical / must-hear (death, level up, boss hit) |

### SFX patterns

An SFX pattern is a standard single-channel Furnace pattern.  Use a fast row rate (speed=3, ~50 ms/row) for tight game-feel timing:

```cpp
static const char* SFX_JUMP =
"5\n"
"C-4007F\n"   // note, inst 00, vol 7F
"G-4007F\n"
"C-5007F\n"
"OFF....\n"
".......\n";

player.sfx_define(SFX_ID_JUMP, SFX_JUMP, 60, 3);
```

Make sure `duration_rows` passed to `sfx_play` is at least as long as the pattern so the SFX voice is held for its full length before being released.

---

## Volume control

Music and SFX volumes are independent.  Both take effect on the **next note key-on** — already-ringing notes are not interrupted, which gives a natural fade-like behaviour.

```cpp
player.set_music_volume(0.7f);   // 70% — takes effect next note
player.set_sfx_volume(1.0f);     // 100% (default)
```

Both methods are thread-safe (atomic store) and can be called from any thread without locking the audio device.  Volume is applied as a TL (Total Level) addition on carrier operators at key-on time, which is the hardware-accurate way to attenuate on the YM2612.

---

## Channels

`IngameFMPlayer` supports up to **4 YM2612 channels** by default (3 on port 0 + 1 on port 1).  `ingamefm_patchlib.h` handles both ports transparently — channels 0–2 use port 0, channels 3–5 use port 1, and all `IngameFMChip` methods accept channel numbers 0–5 directly.

To use more than 4 channels in the player, change `MAX_CHANNELS` in `ingamefm_player.h`:

```cpp
static constexpr int MAX_CHANNELS = 6;  // full YM2612
```

The YM2612 has no channel 6 in the traditional sense — channel 3 can be switched to a special 3-operator CSM mode, which ingamefm does not implement.

---

## Playing notes directly

`IngameFMChip` can be driven directly for real-time note triggers outside the song system — useful for instrument previews, procedural one-shot sounds, or the keyboard piano in demo2/demo3.  Always lock the SDL audio device first.

```cpp
// Load a patch onto a channel
SDL_LockAudioDevice(dev);
player.chip()->load_patch(PATCH_GUITAR, 2);

// Play a note
double hz = IngameFMChip::midi_to_hz(60);  // C4
player.chip()->set_frequency(2, hz);
player.chip()->key_on(2);
SDL_UnlockAudioDevice(dev);

// Release later
SDL_LockAudioDevice(dev);
player.chip()->key_off(2);
SDL_UnlockAudioDevice(dev);
```

`chip()` returns null before `start()` is called.

---

## Patch format reference

```cpp
struct YM2612Operator {
    int DT;   // Detune:        -3..+3
    int MUL;  // Multiplier:    0-15
    int TL;   // Total Level:   0-127  (0 = loudest, 127 = silent)
    int RS;   // Rate Scaling:  0-3
    int AR;   // Attack Rate:   0-31   (31 = fastest)
    int AM;   // AM Enable:     0-1
    int DR;   // Decay Rate:    0-31
    int SR;   // Sustain Rate:  0-31
    int SL;   // Sustain Level: 0-15
    int RR;   // Release Rate:  0-15
    int SSG;  // SSG-EG:        0=off, 1-8=hardware modes 0-7
};

struct YM2612Patch {
    int ALG;           // Algorithm:      0-7
    int FB;            // Feedback:       0-7  (on OP1 only)
    int AMS;           // AM Sensitivity: 0-3
    int FMS;           // FM Sensitivity: 0-7
    YM2612Operator op[4];   // OP1, OP2, OP3, OP4
};
```

**TL** is an attenuation value, not a level — 0 is the loudest an operator can be, 127 is inaudible.  On carrier operators TL directly controls output volume.  On modulator operators TL controls how much that modulator affects the carrier — lower TL means more aggressive FM modulation.

**AR=31** is the fastest possible attack (sample-accurate onset).  All SFX patches in the built-in catalogue use AR=31 on every operator to ensure zero-delay starts.

**ALG** determines which operators are carriers (audible outputs) and which are modulators (FM inputs).  Algorithms 0–3 have one carrier (OP4); algorithm 4 has two; algorithms 5–6 have three; algorithm 7 has four (all operators are carriers, no FM modulation).

---

## API summary

### `IngameFMPlayer`

```cpp
// Configuration
void set_song(const std::string& pattern_text, int tick_rate, int speed);
void add_patch(int instrument_id, const YM2612Patch& patch);
void sfx_reserve(int n);
void sfx_define(int id, const std::string& pattern, int tick_rate, int speed);

// Playback
void play(bool loop = false);                 // blocking
void start(SDL_AudioDeviceID, bool loop);     // non-blocking
void stop(SDL_AudioDeviceID);
bool is_finished() const;

// SFX  (call with audio device locked)
void sfx_play(int id, int priority, int duration_rows);

// Volume  (thread-safe, no lock needed)
void set_music_volume(float v);   // 0.0 (silent) .. 1.0 (full)
void set_sfx_volume(float v);

// Direct chip access  (call with audio device locked)
IngameFMChip* chip();

// SDL audio callback — wire into SDL_AudioSpec::callback
static void s_audio_callback(void* userdata, Uint8* stream, int len);

static constexpr int SAMPLE_RATE = 44100;
static constexpr int MAX_CHANNELS = 4;
```

### `IngameFMChip`

```cpp
void load_patch(const YM2612Patch& patch, int ch);    // ch 0-5
void set_frequency(int ch, double hz, int octaveOffset = 0);
void key_on(int ch);
void key_off(int ch);
void enable_lfo(bool enable, uint8_t freq = 0);       // freq 0-7
void generate(int16_t* stereo_buffer, int samples);
void write(uint8_t port, uint8_t reg, uint8_t val);   // raw register access

static double midi_to_hz(int midiNote);   // A4 = MIDI 69 = 440 Hz
static constexpr uint32_t YM_CLOCK = 7670453;
```

---

## Error handling

`set_song()` and `sfx_define()` throw `std::runtime_error` with a message identifying the line number and the parse problem if the pattern text is malformed.  Wrap setup calls in a try/catch block.

```cpp
try {
    player.set_song(SONG, 60, 6);
    player.add_patch(0x00, PATCH_FLUTE);
    player.start(dev, true);
} catch (const std::exception& e) {
    fprintf(stderr, "ingamefm setup failed: %s\n", e.what());
    return 1;
}
```
