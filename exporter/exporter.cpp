// =============================================================================
// exporter.cpp — WAV Exporter CLI for eggsfm
// =============================================================================
//
// Exports all songs and SFX from the demo to WAV files.
// Usage: ./exporter [output_directory]
//
// Default output directory: ./exported/
// =============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#include "xfm_api.h"
#include "xfm_export.h"

// =============================================================================
// INSTRUMENT PATCHES (from demo)
// =============================================================================

static constexpr xfm_patch_opn PATCH_00 =
{
    .ALG = 2, .FB = 5, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 1,  .MUL = 3, .TL = 38, .RS = 0, .AR = 12, .AM = 0, .DR = 7,  .SR = 11, .SL = 4, .RR = 6,  .SSG = 0 },
        { .DT = -1, .MUL = 1, .TL = 38, .RS = 0, .AR = 17, .AM = 0, .DR = 5,  .SR = 2,  .SL = 2, .RR = 1,  .SSG = 0 },
        { .DT = 1,  .MUL = 2, .TL = 5,  .RS = 0, .AR = 11, .AM = 0, .DR = 13, .SR = 11, .SL = 5, .RR = 13, .SSG = 0 },
        { .DT = -1, .MUL = 1, .TL = 0,  .RS = 0, .AR = 31, .AM = 0, .DR = 9,  .SR = 15, .SL = 5, .RR = 8,  .SSG = 3 }
    }
};

static constexpr xfm_patch_opn PATCH_01 =
{
    .ALG = 4, .FB = 6, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 0, .MUL = 3, .TL = 35, .RS = 0, .AR = 13, .AM = 0, .DR = 1,  .SR = 25, .SL = 2, .RR = 0, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL = 20, .RS = 0, .AR = 17, .AM = 0, .DR = 10, .SR = 8,  .SL = 2, .RR = 7, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL = 11, .RS = 0, .AR = 8,  .AM = 0, .DR = 4,  .SR = 23, .SL = 7, .RR = 1, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL = 14, .RS = 0, .AR = 25, .AM = 0, .DR = 0,  .SR = 10, .SL = 0, .RR = 9, .SSG = 0 }
    }
};

static constexpr xfm_patch_opn PATCH_HIHAT =
{
    .ALG = 7, .FB = 7, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 3, .MUL = 13, .TL =  8, .RS = 3, .AR = 31, .AM = 0, .DR = 31, .SR = 0, .SL = 15, .RR = 15, .SSG = 0 },
        { .DT = 2, .MUL = 11, .TL = 12, .RS = 3, .AR = 31, .AM = 0, .DR = 31, .SR = 0, .SL = 15, .RR = 15, .SSG = 0 },
        { .DT = 1, .MUL =  7, .TL = 16, .RS = 3, .AR = 31, .AM = 0, .DR = 30, .SR = 0, .SL = 15, .RR = 14, .SSG = 0 },
        { .DT = 0, .MUL = 15, .TL = 20, .RS = 3, .AR = 31, .AM = 0, .DR = 29, .SR = 0, .SL = 15, .RR = 13, .SSG = 0 }
    }
};

static constexpr xfm_patch_opn PATCH_KICK =
{
    .ALG = 0, .FB = 7, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 0, .MUL = 1, .TL =  0, .RS = 3, .AR = 31, .AM = 0, .DR = 31, .SR = 0, .SL = 15, .RR = 15, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL = 16, .RS = 2, .AR = 31, .AM = 0, .DR = 20, .SR = 0, .SL = 15, .RR = 10, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL = 20, .RS = 1, .AR = 31, .AM = 0, .DR = 18, .SR = 0, .SL = 15, .RR =  8, .SSG = 0 },
        { .DT = 0, .MUL = 1, .TL =  0, .RS = 0, .AR = 31, .AM = 0, .DR = 14, .SR = 0, .SL = 15, .RR =  8, .SSG = 0 }
    }
};

static constexpr xfm_patch_opn PATCH_SNARE =
{
    .ALG = 4, .FB = 7, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 3, .MUL = 15, .TL =  0, .RS = 3, .AR = 31, .AM = 0, .DR = 31, .SR = 0, .SL = 15, .RR = 15, .SSG = 0 },
        { .DT = 0, .MUL =  3, .TL =  0, .RS = 2, .AR = 31, .AM = 0, .DR = 28, .SR = 0, .SL = 15, .RR = 14, .SSG = 0 },
        { .DT = 2, .MUL =  7, .TL = 10, .RS = 1, .AR = 31, .AM = 0, .DR = 24, .SR = 0, .SL = 15, .RR = 12, .SSG = 0 },
        { .DT = 0, .MUL =  1, .TL =  2, .RS = 0, .AR = 31, .AM = 0, .DR = 22, .SR = 0, .SL = 15, .RR = 10, .SSG = 0 }
    }
};

static constexpr xfm_patch_opn PATCH_CLANG =
{
    .ALG = 3, .FB = 6, .AMS = 0, .FMS = 0,
    .op = {
        { .DT = 3, .MUL = 11, .TL =  0, .RS = 3, .AR = 31, .AM = 0, .DR = 25, .SR = 0, .SL = 15, .RR = 12, .SSG = 0 },
        { .DT = -2, .MUL = 7, .TL =  8, .RS = 2, .AR = 31, .AM = 0, .DR = 22, .SR = 0, .SL = 15, .RR = 10, .SSG = 0 },
        { .DT = 2, .MUL = 13, .TL = 12, .RS = 2, .AR = 31, .AM = 0, .DR = 20, .SR = 0, .SL = 15, .RR =  9, .SSG = 0 },
        { .DT = 0, .MUL =  1, .TL =  0, .RS = 0, .AR = 31, .AM = 0, .DR = 18, .SR = 0, .SL = 15, .RR =  8, .SSG = 0 }
    }
};

// =============================================================================
// SFX PATTERNS
// tick_rate=60, speed=3 → 50ms per row
// =============================================================================

// JUMP — rising three-note sweep, snappy
static const char* SFX_JUMP =
"6\n"
"C-4007F\n"
"E-4007F\n"
"G-4007F\n"
"C-5007F\n"
"OFF....\n"
".......\n";

// COIN — bright high two-note ping
static const char* SFX_COIN =
"5\n"
"E-5017F\n"
"A-5017F\n"
"E-6017F\n"
"OFF....\n"
".......\n";

// ALARM — urgent repeating two-tone pulse
static const char* SFX_ALARM =
"8\n"
"A-4007F\n"
"E-4007F\n"
"A-4007F\n"
"E-4007F\n"
"A-4007F\n"
"E-4007F\n"
"OFF....\n"
".......\n";

// FANFARE — triumphant ascending arpeggio with held final note
static const char* SFX_FANFARE =
"12\n"
"C-4017F\n"
"E-4017F\n"
"G-4017F\n"
"C-5017F\n"
"E-5017F\n"
"G-5017F\n"
"C-6017F\n"
".......\n"
".......\n"
".......\n"
"OFF....\n"
".......\n";

static constexpr int SFX_ID_JUMP    = 0;
static constexpr int SFX_ID_COIN    = 1;
static constexpr int SFX_ID_ALARM   = 2;
static constexpr int SFX_ID_FANFARE = 3;

// =============================================================================
// SONGS
// =============================================================================

// Song 1: 5 channels, tick_rate=60, speed=6 -> 100ms/row, 64 rows = 6.4s loop
// Instruments: 00=PATCH_00 (ch0), 01=PATCH_01 (ch2-4), 02=PATCH_HIHAT (ch1)
static const char* SONG_1 =
"64\n"
"C-3007F....|C#30266....|C-3017F....|E-3017F....|G-3017F....\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"G-300......|C#302......|G-301......|B-301......|D-301......\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#302......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"A-300......|C#402......|A-301......|C-301......|E-301......\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"F-300......|C#302......|F-301......|A-301......|C-301......\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|C#402......|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
"...........|...........|...........|...........|...........\n"
;

// Song 2: 4 channels, tick_rate=60, speed=4 -> ~66ms/row, 16 rows = ~1.06s loop
// Instruments: 20=KICK, 21=SNARE, 22=HIHAT, 23=CLANG
static const char* SONG_2 =
"16\n"
"C-3207F....|...........|C-3227F....|...........\n"
"...........|...........|...........|...........\n"
"...........|C-3217F....|...........|C-3237F....\n"
"...........|...........|...........|...........\n"
"C-3207F....|...........|C-3227F....|...........\n"
"...........|...........|...........|...........\n"
"...........|C-3217F....|...........|...........\n"
"...........|...........|C-3227F....|...........\n"
"C-3207F....|...........|...........|C-3237F....\n"
"...........|...........|C-3227F....|...........\n"
"...........|C-3217F....|...........|...........\n"
"...........|...........|...........|...........\n"
"C-3207F....|...........|C-3227F....|...........\n"
"...........|...........|...........|...........\n"
"...........|C-3217F....|...........|C-3237F....\n"
"...........|...........|C-3227F....|...........\n"
;

static constexpr int SONG_ID_1 = 1;
static constexpr int SONG_ID_2 = 2;

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv)
{
    // Determine output directory
    const char* output_dir = "./exported";
    if (argc > 1) {
        output_dir = argv[1];
    }
    
    // Create output directory
    mkdir(output_dir, 0755);
    
    printf("eggsfm WAV Exporter\n");
    printf("===================\n");
    printf("Output directory: %s\n\n", output_dir);
    
    // Create module for SFX export
    // Use 44100 Hz sample rate, 256 frame buffer
    xfm_module* sfx_module = xfm_module_create(44100, 256, XFM_CHIP_YM2612);
    if (!sfx_module) {
        fprintf(stderr, "Failed to create SFX module\n");
        return 1;
    }
    
    // Create module for song export
    xfm_module* song_module = xfm_module_create(44100, 256, XFM_CHIP_YM2612);
    if (!song_module) {
        fprintf(stderr, "Failed to create song module\n");
        xfm_module_destroy(sfx_module);
        return 1;
    }
    
    // Load patches into both modules
    const struct { int id; const xfm_patch_opn* patch; } patches[] = {
        { 0x00, &PATCH_00 },
        { 0x01, &PATCH_01 },
        { 0x02, &PATCH_HIHAT },
        { 0x20, &PATCH_KICK },
        { 0x21, &PATCH_SNARE },
        { 0x23, &PATCH_CLANG },
    };
    
    for (size_t i = 0; i < sizeof(patches)/sizeof(patches[0]); i++) {
        xfm_patch_set(sfx_module, patches[i].id, patches[i].patch, sizeof(xfm_patch_opn), XFM_CHIP_YM2612);
        xfm_patch_set(song_module, patches[i].id, patches[i].patch, sizeof(xfm_patch_opn), XFM_CHIP_YM2612);
    }
    
    printf("Loaded %zu patches\n\n", sizeof(patches)/sizeof(patches[0]));
    
    // Declare SFX
    xfm_sfx_declare(sfx_module, SFX_ID_JUMP,    SFX_JUMP,    60, 3);
    xfm_sfx_declare(sfx_module, SFX_ID_COIN,    SFX_COIN,    60, 3);
    xfm_sfx_declare(sfx_module, SFX_ID_ALARM,   SFX_ALARM,   60, 3);
    xfm_sfx_declare(sfx_module, SFX_ID_FANFARE, SFX_FANFARE, 60, 3);
    
    // Declare songs
    xfm_song_declare(song_module, SONG_ID_1, SONG_1, 60, 6);
    xfm_song_declare(song_module, SONG_ID_2, SONG_2, 60, 4);
    
    printf("Declared 4 SFX and 2 songs\n\n");
    
    // Export SFX
    printf("Exporting SFX...\n");
    char filename[512];
    
    snprintf(filename, sizeof(filename), "%s/sfx_jump.wav", output_dir);
    xfm_export_sfx(sfx_module, SFX_ID_JUMP, filename);
    
    snprintf(filename, sizeof(filename), "%s/sfx_coin.wav", output_dir);
    xfm_export_sfx(sfx_module, SFX_ID_COIN, filename);
    
    snprintf(filename, sizeof(filename), "%s/sfx_alarm.wav", output_dir);
    xfm_export_sfx(sfx_module, SFX_ID_ALARM, filename);
    
    snprintf(filename, sizeof(filename), "%s/sfx_fanfare.wav", output_dir);
    xfm_export_sfx(sfx_module, SFX_ID_FANFARE, filename);
    
    // Export songs
    printf("\nExporting songs...\n");
    
    snprintf(filename, sizeof(filename), "%s/song_1.wav", output_dir);
    xfm_export_song(song_module, SONG_ID_1, filename);
    
    snprintf(filename, sizeof(filename), "%s/song_2.wav", output_dir);
    xfm_export_song(song_module, SONG_ID_2, filename);
    
    // Cleanup
    xfm_module_destroy(sfx_module);
    xfm_module_destroy(song_module);
    
    printf("\nExport complete!\n");
    printf("Output files in: %s/\n", output_dir);
    
    return 0;
}
