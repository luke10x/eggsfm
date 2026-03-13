// =============================================================================
// demo5.cpp  — ingamefm: music + SFX priority system
//
// Song design:
//   Ch0 — Flute melody, slow (speed=16, ~267ms/row at tick_rate=60)
//          Gentle descending/ascending phrases, long sustain.
//   Ch1 — Metallic clang on beat 1 & 3  (SFX voice, evictable)
//   Ch2 — Sharp hi-hat on beats 2 & 4   (SFX voice, evictable)
//
// SFX (keyboard keys 1 / 2 / 3) — all fire immediately on their own clock,
// AR=31 patches so the attack is sample-accurate:
//   1  — DING    priority 1   slap bass blip    (low)
//   2  — ALARM   priority 5   two-note metal    (medium)
//   3  — FANFARE priority 9   four-note ascent  (high)
//
// Priority rules:
//   Higher priority evicts lower priority from a reserved voice.
//   Equal/higher priority new SFX is ignored (does not interrupt).
//   When SFX expires, music resumes on next song tick for that channel.
//
// Press 1, 2, 3 to fire SFX. Esc or close window to quit.
// =============================================================================

#include "ingamefm.h"
#include <cstdio>

// =============================================================================
// Background music  (3 channels, slow tempo)
//
// tick_rate=60, speed=16  ->  44100/60*16 = 11760 samples/row (~267ms)
// 16 rows * 267ms = ~4.3s loop
//
// Ch0: Flute pad — gentle C major arpeggio, 2 rows per note
// Ch1: Clang on beats 1 & 3  (inst 01 = PATCH_CLANG, D5)
// Ch2: Hi-hat on beats 2 & 4 (inst 02 = PATCH_HIHAT, A5)
// =============================================================================

static const char* SONG =
"org.tildearrow.furnace - Pattern Data (16)\n"
"16\n"
/* row  0 */ "C-5007F|D-5017F|.......\n"
/* row  1 */ ".......|OFF....|A-5027F\n"
/* row  2 */ "A-4007F|D-5017F|.......\n"
/* row  3 */ ".......|OFF....|A-5027F\n"
/* row  4 */ "G-4007F|D-5017F|.......\n"
/* row  5 */ ".......|OFF....|A-5027F\n"
/* row  6 */ "E-4007F|D-5017F|.......\n"
/* row  7 */ ".......|OFF....|A-5027F\n"
/* row  8 */ "F-4007F|D-5017F|.......\n"
/* row  9 */ ".......|OFF....|A-5027F\n"
/* row 10 */ "A-4007F|D-5017F|.......\n"
/* row 11 */ ".......|OFF....|A-5027F\n"
/* row 12 */ "G-4007F|D-5017F|.......\n"
/* row 13 */ ".......|OFF....|A-5027F\n"
/* row 14 */ "E-4007F|D-5017F|.......\n"
/* row 15 */ ".......|OFF....|A-5027F\n";

// =============================================================================
// SFX patterns  (single-channel)
//
// inst 10 = PATCH_SLAP_BASS  — punchy low blip
// inst 11 = PATCH_CLANG      — metallic mid crack
// inst 12 = PATCH_GUITAR     — bright high stab
//
// tick_rate=60, speed=3  ->  44100/60*3 = 2205 samples/row (~50ms)
// Fast row rate so SFX notes feel snappy and tight.
// =============================================================================

static const char* SFX_DING =
"3\n"
"C-3107F\n"
"OFF....\n"
".......\n";

static const char* SFX_ALARM =
"6\n"
"D-5117F\n"
"OFF....\n"
"A-4117F\n"
"OFF....\n"
".......\n"
".......\n";

static const char* SFX_FANFARE =
"8\n"
"C-4127F\n"
"E-4127F\n"
"G-4127F\n"
"C-5127F\n"
"OFF....\n"
".......\n"
".......\n"
".......\n";

// =============================================================================
// IDs, priorities, durations (in SFX rows)
// =============================================================================

static constexpr int ID_DING    = 0;
static constexpr int ID_ALARM   = 1;
static constexpr int ID_FANFARE = 2;

static constexpr int PRIO_DING    = 1;
static constexpr int PRIO_ALARM   = 5;
static constexpr int PRIO_FANFARE = 9;

static constexpr int DUR_DING    =  6;
static constexpr int DUR_ALARM   = 12;
static constexpr int DUR_FANFARE = 16;

// =============================================================================
// main
// =============================================================================

int main(int /*argc*/, char** /*argv*/)
{
    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
    {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "ingamefm demo5  |  1=ding(p1)  2=alarm(p5)  3=fanfare(p9)  |  Esc: quit",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        540, 100,
        SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    IngameFMPlayer player;
    try
    {
        player.set_song(SONG, /*tick_rate=*/60, /*speed=*/16);

        // Music patches
        player.add_patch(0x00, PATCH_FLUTE);   // ch0 soft melody
        player.add_patch(0x01, PATCH_CLANG);   // ch1 metallic clang
        player.add_patch(0x02, PATCH_HIHAT);   // ch2 sharp hi-hat

        // Last 2 channels are SFX voices
        player.sfx_reserve(2);

        // SFX patches — all sharp attack (AR=31 in every operator)
        player.add_patch(0x10, PATCH_SLAP_BASS);
        player.add_patch(0x11, PATCH_CLANG);
        player.add_patch(0x12, PATCH_GUITAR);

        // speed=3 -> ~50ms/row for tight SFX timing
        player.sfx_define(ID_DING,    SFX_DING,    60, 3);
        player.sfx_define(ID_ALARM,   SFX_ALARM,   60, 3);
        player.sfx_define(ID_FANFARE, SFX_FANFARE, 60, 3);
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "Setup error: %s\n", e.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec desired{};
    desired.freq     = IngameFMPlayer::SAMPLE_RATE;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 128;  // small buffer = low SFX latency (~3ms vs ~12ms)
    desired.callback = IngameFMPlayer::s_audio_callback;
    desired.userdata = &player;

    SDL_AudioSpec obtained{};
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (dev == 0)
    {
        std::fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    player.start(dev, /*loop=*/true);
    SDL_PauseAudioDevice(dev, 0);

    std::printf("=== ingamefm demo5 ===\n");
    std::printf("  Ch0: Flute  — slow soft melody  (music-only)\n");
    std::printf("  Ch1: Clang  — metallic beat 1&3 (SFX voice)\n");
    std::printf("  Ch2: Hi-hat — sharp beat 2&4    (SFX voice)\n\n");
    std::printf("  1 — DING    (priority 1)  punchy low blip\n");
    std::printf("  2 — ALARM   (priority 5)  two metallic hits\n");
    std::printf("  3 — FANFARE (priority 9)  four-note stab\n\n");
    std::printf("  SFX fires immediately on its own clock.\n");
    std::printf("  Beat resumes when SFX expires.\n");
    std::printf("  Esc to quit.\n\n");

    bool running = true;
    while (running)
    {
        // SDL_WaitEventTimeout(1) blocks for at most 1ms waiting for an event,
        // then returns whether one arrived. This keeps CPU usage low while
        // polling events far more frequently than SDL_Delay(1) which typically
        // sleeps 10-15ms on most OSes — that extra sleep is what makes SFX
        // feel delayed compared to demo3's vsync-driven loop.
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                case SDLK_1:
                    SDL_LockAudioDevice(dev);
                    player.sfx_play(ID_DING, PRIO_DING, DUR_DING);
                    SDL_UnlockAudioDevice(dev);
                    std::printf("  >> DING    p%d\n", PRIO_DING);
                    break;
                case SDLK_2:
                    SDL_LockAudioDevice(dev);
                    player.sfx_play(ID_ALARM, PRIO_ALARM, DUR_ALARM);
                    SDL_UnlockAudioDevice(dev);
                    std::printf("  >> ALARM   p%d\n", PRIO_ALARM);
                    break;
                case SDLK_3:
                    SDL_LockAudioDevice(dev);
                    player.sfx_play(ID_FANFARE, PRIO_FANFARE, DUR_FANFARE);
                    SDL_UnlockAudioDevice(dev);
                    std::printf("  >> FANFARE p%d\n", PRIO_FANFARE);
                    break;
                default:
                    break;
                }
            }
        }
        SDL_WaitEventTimeout(nullptr, 1);
    }

    SDL_CloseAudioDevice(dev);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
