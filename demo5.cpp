// =============================================================================
// demo5.cpp  — ingamefm: 4-channel music + game SFX + volume controls
//
// Channels:
//   Ch0 — Lead   (PATCH_GUITAR)      music-only
//   Ch1 — Pad    (PATCH_FLUTE)       music-only
//   Ch2 — Beat   (PATCH_HIHAT/CLANG) SFX voice
//   Ch3 — Bass   (PATCH_SLAP_BASS)   SFX voice (rightmost)
//
// Tempo: tick_rate=60, speed=20  ->  ~333ms/row,  32 rows = ~10.7s loop
//
// Volume controls (take effect on next note key-on):
//   Up   / Down  — music volume  +/- 10%
//   Left / Right — SFX volume    +/- 10%
//
// SFX keys (fire immediately on own clock, speed=3 ~50ms/row):
//   1  — DING         p1   short blip
//   2  — ALARM        p3   two-hit warning
//   3  — FANFARE      p5   ascending four-note run
//   q  — JUMP         p4   upward chirp
//   w  — COIN         p3   bright high ping
//   e  — LEVEL_UP     p6   triumphant five-note rise
//   r  — DEATH        p8   descending sad fall
//   t  — DAMAGE       p5   harsh buzz hit
//   y  — ATTACK       p4   sharp sword crack
//   u  — CLIMB        p2   ratchet ticks
//   a  — FALL_WATER   p3   splashy descending plunge
//   s  — TALK         p2   short vocal-like blip sequence
//   d  — LAUGH        p3   rising bouncy triplet
//   f  — SCREAM       p6   fast rising then held shriek
//   g  — CRY          p3   slow descending sob phrase
//   h  — LAUNCH       p5   rising whoosh burst
//   j  — WARP         p5   sci-fi pitch sweep
//   k  — SLIDE        p3   smooth descending glide
//   l  — SUBMERSE     p3   low bubbling descent
//
// Esc to quit.
// =============================================================================

#include "ingamefm.h"
#include <cstdio>
#include <algorithm>

// =============================================================================
// Song  (4 channels, Am pentatonic, slow drift)
//
// Instrument IDs (music):
//   00 = PATCH_GUITAR    ch0 lead
//   01 = PATCH_FLUTE     ch1 pad
//   02 = PATCH_CLANG     ch2 clang
//   03 = PATCH_HIHAT     ch2 hihat
//   04 = PATCH_SLAP_BASS ch3 bass
// =============================================================================

static const char* SONG =
"org.tildearrow.furnace - Pattern Data (32)\n"
"32\n"
/* r 0  */ "A-4007F|A-3017F|A-5027F|A-2047F\n"
/* r 1  */ ".......|.......|OFF....|.......\n"
/* r 2  */ ".......|.......|A-5037F|.......\n"
/* r 3  */ ".......|.......|OFF....|.......\n"
/* r 4  */ "C-5007F|.......|A-5027F|E-2047F\n"
/* r 5  */ ".......|.......|OFF....|.......\n"
/* r 6  */ ".......|.......|A-5037F|.......\n"
/* r 7  */ ".......|.......|OFF....|.......\n"
/* r 8  */ "E-5007F|E-3017F|A-5027F|A-2047F\n"
/* r 9  */ ".......|.......|OFF....|.......\n"
/* r10  */ ".......|.......|A-5037F|.......\n"
/* r11  */ ".......|.......|OFF....|.......\n"
/* r12  */ "D-5007F|.......|A-5027F|E-2047F\n"
/* r13  */ ".......|.......|OFF....|.......\n"
/* r14  */ ".......|.......|A-5037F|.......\n"
/* r15  */ ".......|.......|OFF....|.......\n"
/* r16  */ "C-5007F|C-3017F|A-5027F|C-2047F\n"
/* r17  */ ".......|.......|OFF....|.......\n"
/* r18  */ ".......|.......|A-5037F|.......\n"
/* r19  */ ".......|.......|OFF....|.......\n"
/* r20  */ "A-4007F|.......|A-5027F|G-2047F\n"
/* r21  */ ".......|.......|OFF....|.......\n"
/* r22  */ ".......|.......|A-5037F|.......\n"
/* r23  */ ".......|.......|OFF....|.......\n"
/* r24  */ "G-4007F|G-3017F|A-5027F|G-2047F\n"
/* r25  */ ".......|.......|OFF....|.......\n"
/* r26  */ ".......|.......|A-5037F|.......\n"
/* r27  */ ".......|.......|OFF....|.......\n"
/* r28  */ "A-4007F|A-3017F|A-5027F|A-2047F\n"
/* r29  */ ".......|.......|OFF....|.......\n"
/* r30  */ ".......|.......|A-5037F|.......\n"
/* r31  */ ".......|.......|OFF....|.......\n";

// =============================================================================
// SFX patterns
//
// SFX instrument IDs:
//   10 = PATCH_SLAP_BASS   — punchy thud  (damage, ding, fall_water)
//   11 = PATCH_CLANG       — metallic     (alarm, attack, launch)
//   12 = PATCH_GUITAR      — bright stab  (fanfare, jump, scream, level_up)
//   13 = PATCH_SYNTH_BASS  — fat buzz     (death, warp, submerse)
//   14 = PATCH_HIHAT       — sharp tick   (coin, climb, slide)
//   15 = PATCH_ELECTRIC_BASS — warm round (talk, laugh, cry)
//
// All SFX: tick_rate=60, speed=3  ->  ~50ms/row
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
"10\n"
"C-4127F\n"
"E-4127F\n"
"G-4127F\n"
"C-5127F\n"
"OFF....\n"
".......\n"
".......\n"
".......\n"
".......\n"
".......\n";

static const char* SFX_JUMP =
"5\n"
"C-4127F\n"
"G-4127F\n"
"C-5127F\n"
"OFF....\n"
".......\n";

static const char* SFX_COIN =
"4\n"
"A-5147F\n"
"E-6147F\n"
"OFF....\n"
".......\n";

static const char* SFX_LEVEL_UP =
"12\n"
"C-4127F\n"
"E-4127F\n"
"G-4127F\n"
"C-5127F\n"
"E-5127F\n"
"G-5127F\n"
"OFF....\n"
".......\n"
".......\n"
".......\n"
".......\n"
".......\n";

static const char* SFX_DEATH =
"14\n"
"A-4137F\n"
".......\n"
"F-4137F\n"
".......\n"
"D-4137F\n"
".......\n"
"B-3137F\n"
".......\n"
"G-3137F\n"
".......\n"
"OFF....\n"
".......\n"
".......\n"
".......\n";

static const char* SFX_DAMAGE =
"6\n"
"A-2107F\n"
"G-2107F\n"
"OFF....\n"
".......\n"
".......\n"
".......\n";

static const char* SFX_ATTACK =
"4\n"
"D-5117F\n"
"OFF....\n"
".......\n"
".......\n";

static const char* SFX_CLIMB =
"6\n"
"A-4147F\n"
"OFF....\n"
"A-4147F\n"
"OFF....\n"
".......\n"
".......\n";

// a — FALL_WATER: descending plunge then low splashing rumble
static const char* SFX_FALL_WATER =
"10\n"
"A-4107F\n"
"E-4107F\n"
"C-4107F\n"
"A-3107F\n"
"E-3107F\n"
"C-3107F\n"
"OFF....\n"
".......\n"
".......\n"
".......\n";

// s — TALK: short blippy staccato phrase (3 quick mid notes)
static const char* SFX_TALK =
"8\n"
"E-4157F\n"
"OFF....\n"
"G-4157F\n"
"OFF....\n"
"E-4157F\n"
"OFF....\n"
".......\n"
".......\n";

// d — LAUGH: rising bouncy three-note phrase, repeated
static const char* SFX_LAUGH =
"10\n"
"C-4157F\n"
"E-4157F\n"
"G-4157F\n"
"OFF....\n"
"C-4157F\n"
"E-4157F\n"
"G-4157F\n"
"OFF....\n"
".......\n"
".......\n";

// f — SCREAM: fast ascending sweep then held shriek
static const char* SFX_SCREAM =
"12\n"
"C-3127F\n"
"G-3127F\n"
"C-4127F\n"
"G-4127F\n"
"C-5127F\n"
"G-5127F\n"
"C-6127F\n"
".......\n"
".......\n"
"OFF....\n"
".......\n"
".......\n";

// g — CRY: slow descending sob — long notes stepping down
static const char* SFX_CRY =
"14\n"
"G-4157F\n"
".......\n"
".......\n"
"E-4157F\n"
".......\n"
".......\n"
"D-4157F\n"
".......\n"
".......\n"
"B-3157F\n"
".......\n"
".......\n"
"OFF....\n"
".......\n";

// h — LAUNCH: low thud then fast rising burst
static const char* SFX_LAUNCH =
"8\n"
"C-2117F\n"
"OFF....\n"
"C-3117F\n"
"G-3117F\n"
"C-4117F\n"
"G-4117F\n"
"OFF....\n"
".......\n";

// j — WARP: sci-fi pitch sweep up then down (synth bass)
static const char* SFX_WARP =
"12\n"
"C-2137F\n"
"C-3137F\n"
"C-4137F\n"
"C-5137F\n"
"C-6137F\n"
".......\n"
"C-5137F\n"
"C-4137F\n"
"C-3137F\n"
"C-2137F\n"
"OFF....\n"
".......\n";

// k — SLIDE: smooth descending glide (hihat timbre, stepping down quickly)
static const char* SFX_SLIDE =
"8\n"
"A-5147F\n"
"G-5147F\n"
"E-5147F\n"
"D-5147F\n"
"C-5147F\n"
"A-4147F\n"
"OFF....\n"
".......\n";

// l — SUBMERSE: low bubbling descent (synth bass, slow steps down)
static const char* SFX_SUBMERSE =
"12\n"
"A-3137F\n"
".......\n"
"G-3137F\n"
".......\n"
"E-3137F\n"
".......\n"
"C-3137F\n"
".......\n"
"A-2137F\n"
".......\n"
"OFF....\n"
".......\n";

// =============================================================================
// SFX table
// =============================================================================

struct SfxEntry {
    int         id;
    int         priority;
    int         duration;
    SDL_Keycode key;
    const char* pattern;
    const char* name;
};

static const SfxEntry SFX_TABLE[] =
{
    {  0, 1,  6,  SDLK_1, SFX_DING,       "DING      " },
    {  1, 3, 12,  SDLK_2, SFX_ALARM,      "ALARM     " },
    {  2, 5, 20,  SDLK_3, SFX_FANFARE,    "FANFARE   " },
    {  3, 4, 10,  SDLK_q, SFX_JUMP,       "JUMP      " },
    {  4, 3,  8,  SDLK_w, SFX_COIN,       "COIN      " },
    {  5, 6, 24,  SDLK_e, SFX_LEVEL_UP,   "LEVEL UP  " },
    {  6, 8, 28,  SDLK_r, SFX_DEATH,      "DEATH     " },
    {  7, 5, 12,  SDLK_t, SFX_DAMAGE,     "DAMAGE    " },
    {  8, 4,  8,  SDLK_y, SFX_ATTACK,     "ATTACK    " },
    {  9, 2, 12,  SDLK_u, SFX_CLIMB,      "CLIMB     " },
    { 10, 3, 20,  SDLK_a, SFX_FALL_WATER, "FALL/WATER" },
    { 11, 2, 16,  SDLK_s, SFX_TALK,       "TALK      " },
    { 12, 3, 20,  SDLK_d, SFX_LAUGH,      "LAUGH     " },
    { 13, 6, 24,  SDLK_f, SFX_SCREAM,     "SCREAM    " },
    { 14, 3, 28,  SDLK_g, SFX_CRY,        "CRY       " },
    { 15, 5, 16,  SDLK_h, SFX_LAUNCH,     "LAUNCH    " },
    { 16, 5, 24,  SDLK_j, SFX_WARP,       "WARP      " },
    { 17, 3, 16,  SDLK_k, SFX_SLIDE,      "SLIDE     " },
    { 18, 3, 24,  SDLK_l, SFX_SUBMERSE,   "SUBMERSE  " },
};
static constexpr int SFX_COUNT = 19;

// =============================================================================
// Volume state — kept in main thread, read/written around audio lock
// =============================================================================

static float g_music_vol = 1.0f;
static float g_sfx_vol   = 1.0f;
static constexpr float VOL_STEP = 0.1f;

static void print_volumes()
{
    std::printf("  Music: %.0f%%   SFX: %.0f%%\n",
                g_music_vol * 100.0f, g_sfx_vol * 100.0f);
}

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
        "ingamefm demo5  |  1-3 q-u a-l = SFX  |  Up/Dn=music  Lt/Rt=sfx  |  Esc",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        680, 100,
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
        player.set_song(SONG, /*tick_rate=*/60, /*speed=*/20);

        // Music patches
        player.add_patch(0x00, PATCH_GUITAR);
        player.add_patch(0x01, PATCH_FLUTE);
        player.add_patch(0x02, PATCH_CLANG);
        player.add_patch(0x03, PATCH_HIHAT);
        player.add_patch(0x04, PATCH_SLAP_BASS);

        // Last 2 channels are SFX voices
        player.sfx_reserve(2);

        // SFX patches
        player.add_patch(0x10, PATCH_SLAP_BASS);
        player.add_patch(0x11, PATCH_CLANG);
        player.add_patch(0x12, PATCH_GUITAR);
        player.add_patch(0x13, PATCH_SYNTH_BASS);
        player.add_patch(0x14, PATCH_HIHAT);
        player.add_patch(0x15, PATCH_ELECTRIC_BASS);

        // Register all SFX
        for (int i = 0; i < SFX_COUNT; i++)
            player.sfx_define(SFX_TABLE[i].id, SFX_TABLE[i].pattern, 60, 3);
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "Setup error: %s\n", ex.what());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_AudioSpec desired{};
    desired.freq     = IngameFMPlayer::SAMPLE_RATE;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 128;
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

    std::printf("=== ingamefm demo5: 4-channel music + %d SFX ===\n\n", SFX_COUNT);
    std::printf("  Volume:  Up/Down = music +/-10%%    Left/Right = SFX +/-10%%\n\n");
    std::printf("  SFX:\n");
    for (int i = 0; i < SFX_COUNT; i++)
    {
        char kn[2] = { '?', 0 };
        SDL_Keycode k = SFX_TABLE[i].key;
        if (k >= SDLK_a && k <= SDLK_z)      kn[0] = (char)(k - SDLK_a + 'a');
        else if (k >= SDLK_0 && k <= SDLK_9) kn[0] = (char)(k - SDLK_0 + '0');
        std::printf("    %s  p%d  %s\n", kn, SFX_TABLE[i].priority, SFX_TABLE[i].name);
    }
    std::printf("\n");
    print_volumes();
    std::printf("\n  Esc to quit.\n\n");

    bool running = true;
    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_KEYDOWN && !e.key.repeat)
            {
                bool handled = false;

                // Volume controls
                switch (e.key.keysym.sym)
                {
                case SDLK_ESCAPE:
                    running = false;
                    handled = true;
                    break;
                case SDLK_UP:
                    g_music_vol = std::min(1.0f, g_music_vol + VOL_STEP);
                    player.set_music_volume(g_music_vol);
                    std::printf("  Music volume: %.0f%%\n", g_music_vol * 100.0f);
                    handled = true;
                    break;
                case SDLK_DOWN:
                    g_music_vol = std::max(0.0f, g_music_vol - VOL_STEP);
                    player.set_music_volume(g_music_vol);
                    std::printf("  Music volume: %.0f%%\n", g_music_vol * 100.0f);
                    handled = true;
                    break;
                case SDLK_RIGHT:
                    g_sfx_vol = std::min(1.0f, g_sfx_vol + VOL_STEP);
                    player.set_sfx_volume(g_sfx_vol);
                    std::printf("  SFX volume:   %.0f%%\n", g_sfx_vol * 100.0f);
                    handled = true;
                    break;
                case SDLK_LEFT:
                    g_sfx_vol = std::max(0.0f, g_sfx_vol - VOL_STEP);
                    player.set_sfx_volume(g_sfx_vol);
                    std::printf("  SFX volume:   %.0f%%\n", g_sfx_vol * 100.0f);
                    handled = true;
                    break;
                default:
                    break;
                }

                if (handled) continue;

                // SFX triggers
                for (int i = 0; i < SFX_COUNT; i++)
                {
                    if (e.key.keysym.sym == SFX_TABLE[i].key)
                    {
                        SDL_LockAudioDevice(dev);
                        player.sfx_play(SFX_TABLE[i].id,
                                        SFX_TABLE[i].priority,
                                        SFX_TABLE[i].duration);
                        SDL_UnlockAudioDevice(dev);
                        std::printf("  >> %s (p%d)\n",
                                    SFX_TABLE[i].name,
                                    SFX_TABLE[i].priority);
                        break;
                    }
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
