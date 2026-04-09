/**
 * =============================================================================
 * xfm_export.h — WAV Export for eggsfm
 * =============================================================================
 *
 * Export songs and SFX patterns to WAV files.
 * Includes minimal WAV writer (no external dependencies).
 *
 * Usage:
 *   xfm_module* m = xfm_module_create(44100, 256, XFM_CHIP_YM2612);
 *   // ... add patches, declare songs/SFX ...
 *   xfm_export_song(m, song_id, "output.wav");
 *   xfm_export_sfx(m, sfx_id, "sfx.wav");
 */

#ifndef XFM_EXPORT_H
#define XFM_EXPORT_H

#include "xfm_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load a WAV file into memory as an embedded array.
 *
 * Helper function to load WAV data for use with xfm_wav_load_memory().
 * Returns malloc'd buffer that caller must free().
 *
 * @param filename Path to WAV file
 * @param outSize Receives the file size
 * @return Pointer to WAV data, or NULL on error
 */
void* xfm_wav_file_to_memory(const char* filename, int* outSize);

/**
 * @brief Export a song pattern to a WAV file.
 *
 * Renders the entire song (all rows) to a stereo WAV file.
 * The song is rendered at the module's sample rate.
 *
 * @param m Module instance (must have song declared)
 * @param song_id Song ID to export (1-15)
 * @param filename Output WAV file path
 * @return 0 on success, -1 on error
 *
 * @note File will be overwritten if it exists
 * @note Song loops are NOT applied - exports exactly num_rows
 */
int xfm_export_song(xfm_module* m, xfm_song_id song_id, const char* filename);

/**
 * @brief Export a song pattern to a WAV buffer in memory.
 *
 * Renders the entire song (all rows) to a stereo WAV buffer.
 * The song is rendered at the module's sample rate.
 *
 * @param m Module instance (must have song declared)
 * @param song_id Song ID to export (1-15)
 * @param outSize Receives the size of the WAV data in bytes
 * @return Pointer to malloc'd WAV data, or NULL on error. Caller must free().
 *
 * @note Song loops are NOT applied - exports exactly num_rows
 */
void* xfm_export_song_to_memory(xfm_module* m, xfm_song_id song_id, int* outSize);

/**
 * @brief Export an SFX pattern to a WAV file.
 *
 * Renders the entire SFX pattern to a stereo WAV file.
 * The SFX is rendered at the module's sample rate.
 *
 * @param m Module instance (must have SFX declared)
 * @param sfx_id SFX ID to export (0-255)
 * @param filename Output WAV file path
 * @return 0 on success, -1 on error
 *
 * @note File will be overwritten if it exists
 */
int xfm_export_sfx(xfm_module* m, int sfx_id, const char* filename);

/**
 * @brief Export an SFX pattern to a WAV buffer in memory.
 *
 * Renders the entire SFX pattern to a stereo WAV buffer.
 * The SFX is rendered at the module's sample rate.
 *
 * @param m Module instance (must have SFX declared)
 * @param sfx_id SFX ID to export (0-255)
 * @param outSize Receives the size of the WAV data in bytes
 * @return Pointer to malloc'd WAV data, or NULL on error. Caller must free().
 */
void* xfm_export_sfx_to_memory(xfm_module* m, int sfx_id, int* outSize);

#ifdef __cplusplus
}
#endif

#endif /* XFM_EXPORT_H */
