/** @file ingamefm.h
 *  @brief IngameFM: C99 FM Synthesis Library for YM2612-compatible Chips
 *
 *  Provides a clean, C99 API for playing music and sound effects on YM2612-compatible
 *  FM synthesis chips, with support for polyphonic voices, envelope handling,
 *  and separate volume control for music and SFX.
 */

#ifndef INGAMEFM_H
#define INGAMEFM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// TYPES
// =============================================================================

/** @brief OPN operator parameters (4 operators per patch) */
typedef struct {
    int8_t  DT;   ///< Detune
    uint8_t MUL;  ///< Multiple
    uint8_t TL;   ///< Total Level
    uint8_t RS;   ///< Rate Scaling
    uint8_t AR;   ///< Attack Rate
    uint8_t AM;   ///< Amplitude Modulation
    uint8_t DR;   ///< Decay Rate
    uint8_t SR;   ///< Sustain Rate
    uint8_t SL;   ///< Sustain Level
    uint8_t RR;   ///< Release Rate
    uint8_t SSG;  ///< SSG-EG
} OPNOperator;

/** @brief YM2612 patch (instrument) */
typedef struct {
    uint8_t  ALG; ///< Algorithm
    uint8_t  FB;  ///< Feedback
    uint8_t  AMS; ///< Amplitude Modulation Sensitivity
    uint8_t  FMS; ///< Frequency Modulation Sensitivity
    OPNOperator op[4]; ///< Operators (4 per patch)
} YM2612Patch;

// Voice, SFX, and Song IDs
typedef int voice_id;
typedef int sfx_id;
typedef int song_id;
typedef int priority;
typedef float volume;

// =============================================================================
// INITIALIZATION & TEARDOWN
// =============================================================================

/** @brief Initialize the audio system
 *  @param sample_rate Audio sample rate (e.g., 44100)
 *  @param buffer_frames Audio buffer size (e.g., 256, 512)
 *  @return true on success, false on failure
 */
bool audio_init(int sample_rate, int buffer_frames);

/** @brief Shutdown the audio system and free resources */
void audio_teardown(void);

// =============================================================================
// SONG MODULE (Music Chip)
// =============================================================================

/** @brief Initialize the song module
 *  @param sample_rate Audio sample rate
 *  @param num_channels Number of channels (e.g., 5)
 *  @return true on success, false on failure
 */
bool song_init(int sample_rate, int num_channels);

/** @brief Load a Furnace-format song
 *  @param song_id Unique song ID
 *  @param song_data Furnace-format song data (tick-based, pattern-based)
 *  @param tick_rate Ticks per second (e.g., 60)
 *  @param speed Song speed (rows per tick)
 *  @return true on success, false on failure
 */
bool song_load(song_id song_id, const char* song_data, int tick_rate, int speed);

/** @brief Play a loaded song
 *  @param song_id Song ID to play
 *  @param loop true to loop, false to play once
 */
void song_play(song_id song_id, bool loop);

/** @brief Stop playback */
void song_stop(void);

/** @brief Switch to a new song (immediately or at loop end)
 *  @param song_id Song ID to switch to
 *  @param now true to switch immediately, false to switch at loop end
 */
void song_select(song_id song_id, bool now);

/** @brief Set music volume
 *  @param volume Volume (0.0–1.0)
 */
void song_set_volume(volume volume);

/** @brief Enable/disable LFO for the song chip
 *  @param enable true to enable, false to disable
 *  @param freq LFO frequency index (0–8)
 */
void song_set_lfo(bool enable, int freq);

/** @brief Render a song to cache for low-latency playback
 *  @param song_id Song ID to cache
 */
void song_render_to_cache(song_id song_id);

/** @brief Check if a song is cached
 *  @param song_id Song ID
 *  @return true if cached, false otherwise
 */
bool song_is_cached(song_id song_id);

/** @brief Get the number of rows captured for a song
 *  @param song_id Song ID
 *  @return Number of rows captured
 */
int song_capture_rows_done(song_id song_id);

/** @brief Check if a song is being captured
 *  @param song_id Song ID
 *  @return true if capturing, false otherwise
 */
bool song_is_capturing(song_id song_id);

/** @brief Get the current song ID
 *  @return Current song ID, or -1 if no song is playing
 */
song_id song_get_current(void);

// =============================================================================
// SFX MODULE (Sound Effects Chip)
// =============================================================================

/** @brief Initialize the SFX module
 *  @param sample_rate Audio sample rate
 *  @param num_voices Number of voices (e.g., 6)
 *  @return true on success, false on failure
 */
bool sfx_init(int sample_rate, int num_voices);

/** @brief Define an SFX pattern
 *  @param sfx_id Unique SFX ID
 *  @param sfx_data SFX pattern data (tick-based)
 *  @param tick_rate Ticks per second (e.g., 60)
 *  @param speed SFX speed (rows per tick)
 *  @return true on success, false on failure
 */
bool sfx_define(sfx_id sfx_id, const char* sfx_data, int tick_rate, int speed);

/** @brief Play an SFX
 *  @param sfx_id SFX ID to play
 *  @param priority Priority (higher = more important)
 *  @param duration Duration in ticks (0 for infinite)
 */
void sfx_play(sfx_id sfx_id, priority priority, int duration);

/** @brief Stop all SFX playback */
void sfx_stop(void);

/** @brief Set SFX volume
 *  @param volume Volume (0.0–1.0)
 */
void sfx_set_volume(volume volume);

/** @brief Enable/disable LFO for the SFX chip
 *  @param enable true to enable, false to disable
 *  @param freq LFO frequency index (0–8)
 */
void sfx_set_lfo(bool enable, int freq);

/** @brief Allocate a voice from the voice pool
 *  @return Voice ID, or -1 if no voices are available
 */
voice_id sfx_voice_allocate(void);

/** @brief Free a voice back to the pool
 *  @param voice_id Voice ID to free
 */
void sfx_voice_free(voice_id voice_id);

/** @brief Play a note (e.g., piano key press)
 *  @param voice_id Voice ID to use (allocated via sfx_voice_allocate)
 *  @param note MIDI note (e.g., 60 for C4)
 *  @param patch_id Patch ID (instrument) to use
 */
void sfx_note_on(voice_id voice_id, int note, int patch_id);

/** @brief Release a note (e.g., piano key release)
 *  @param voice_id Voice ID to release
 */
void sfx_note_off(voice_id voice_id);

/** @brief Check if an SFX is playing
 *  @param sfx_id SFX ID
 *  @return true if playing, false otherwise
 */
bool sfx_is_playing(sfx_id sfx_id);

// =============================================================================
// VOICE POOL
// =============================================================================

/** @brief Initialize the voice pool
 *  @param num_voices Number of voices to allocate
 */
void voice_pool_init(int num_voices);

/** @brief Allocate a voice from the pool
 *  @return Voice ID, or -1 if no voices are available
 */
voice_id voice_pool_allocate(void);

/** @brief Free a voice back to the pool
 *  @param voice_id Voice ID to free
 */
void voice_pool_free(voice_id voice_id);

/** @brief Check if a voice is free
 *  @param voice_id Voice ID
 *  @return true if free, false otherwise
 */
bool voice_pool_is_free(voice_id voice_id);

/** @brief Get the number of free voices
 *  @return Number of free voices
 */
int voice_pool_free_count(void);

// =============================================================================
// PATCH SYSTEM
// =============================================================================

/** @brief Load a patch (instrument)
 *  @param patch_id Unique patch ID
 *  @param patch Patch data (YM2612Patch struct)
 */
void patch_load(int patch_id, const YM2612Patch* patch);

/** @brief Assign a patch to a song instrument
 *  @param song_id Song ID
 *  @param channel Channel index (0–4)
 *  @param patch_id Patch ID to assign
 */
void patch_set_song_instrument(song_id song_id, int channel, int patch_id);

/** @brief Assign a patch to an SFX instrument
 *  @param sfx_id SFX ID
 *  @param patch_id Patch ID to assign
 */
void patch_set_sfx_instrument(sfx_id sfx_id, int patch_id);

// =============================================================================
// AUDIO CONTROL
// =============================================================================

/** @brief Lock the audio device (for thread safety) */
void audio_lock(void);

/** @brief Unlock the audio device */
void audio_unlock(void);

/** @brief Audio callback function (for SDL/OpenAL/etc.) */
void audio_callback(void* userdata, uint8_t* stream, int len);

// =============================================================================
// STATE QUERIES
// =============================================================================

/** @brief Check if the audio system is running
 *  @return true if running, false otherwise
 */
bool audio_is_running(void);

/** @brief Check if the system is in live mode (not cached)
 *  @return true if live, false if cached
 */
bool audio_is_live(void);

/** @brief Get the current sample rate
 *  @return Sample rate (e.g., 44100)
 */
int audio_get_sample_rate(void);

#ifdef __cplusplus
}
#endif

#endif // INGAMEFM_H
