// =============================================================================
// xfm_export.cpp — WAV Export Implementation
// =============================================================================

#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cstdint>

// Include implementation first to access internal structures
#include "xfm_api.h"
#include "xfm_impl.cpp"

// =============================================================================
// Minimal WAV Writer (no external dependencies)
// =============================================================================

static void write_le16(uint8_t* buf, uint16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

static void write_le32(uint8_t* buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static int write_wav_file(const char* filename, int sample_rate, int num_samples, const int16_t* data)
{
    int num_channels = 2;  // Stereo
    int bits_per_sample = 16;
    int byte_rate = sample_rate * num_channels * bits_per_sample / 8;
    int block_align = num_channels * bits_per_sample / 8;
    int data_size = num_samples * num_channels * sizeof(int16_t);
    int file_size = 36 + data_size;  // 44 byte header - 8 = 36
    
    // Allocate header (44 bytes)
    uint8_t header[44];
    memset(header, 0, sizeof(header));
    
    // RIFF header
    memcpy(header + 0, "RIFF", 4);
    write_le32(header + 4, file_size);
    memcpy(header + 8, "WAVE", 4);
    
    // fmt chunk
    memcpy(header + 12, "fmt ", 4);
    write_le32(header + 16, 16);  // Chunk size (16 for PCM)
    write_le16(header + 20, 1);   // Audio format (1 = PCM)
    write_le16(header + 22, num_channels);
    write_le32(header + 24, sample_rate);
    write_le32(header + 28, byte_rate);
    write_le16(header + 32, block_align);
    write_le16(header + 34, bits_per_sample);
    
    // data chunk
    memcpy(header + 36, "data", 4);
    write_le32(header + 40, data_size);
    
    // Write file
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return -1;
    }
    
    // Write header
    if (fwrite(header, 1, 44, f) != 44) {
        fprintf(stderr, "Failed to write WAV header\n");
        fclose(f);
        return -1;
    }
    
    // Write audio data
    if (fwrite(data, 1, data_size, f) != (size_t)data_size) {
        fprintf(stderr, "Failed to write WAV data\n");
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

// =============================================================================
// SONG EXPORT
// =============================================================================

extern "C" int xfm_export_song(xfm_module* m, xfm_song_id song_id, const char* filename)
{
    if (!m || !filename) return -1;
    if (song_id <= 0 || song_id > 15) return -1;
    
    // Get song info
    int num_rows = xfm_song_get_total_rows(m, song_id);
    if (num_rows <= 0) {
        fprintf(stderr, "xfm_export_song: Song %d not found or empty\n", song_id);
        return -1;
    }
    
    // Calculate total samples needed
    // We need to render: num_rows * samples_per_row
    // Add extra rows for release tail
    int samples_per_row = m->song_patterns[song_id].samples_per_row;
    int total_rows = num_rows + 2;  // Extra rows for release tail
    int total_samples = total_rows * samples_per_row;
    
    // Allocate stereo buffer (int16_t, interleaved)
    int16_t* buffer = (int16_t*)malloc(total_samples * 2 * sizeof(int16_t));
    if (!buffer) {
        fprintf(stderr, "xfm_export_song: Failed to allocate %d bytes\n", 
                total_samples * 2 * (int)sizeof(int16_t));
        return -1;
    }
    memset(buffer, 0, total_samples * 2 * sizeof(int16_t));
    
    // Render the song
    // Start song (no loop for export)
    xfm_song_play(m, song_id, false);
    
    // Render in chunks (to match how audio callback works)
    int frames_per_chunk = m->buffer_frames;
    int samples_rendered = 0;
    
    while (samples_rendered < total_samples) {
        int frames_to_render = frames_per_chunk;
        if (samples_rendered + frames_to_render > total_samples) {
            frames_to_render = total_samples - samples_rendered;
        }
        
        // Render this chunk
        xfm_mix_song(m, buffer + (samples_rendered * 2), frames_to_render);
        samples_rendered += frames_to_render;
        
        // Check if song finished
        if (!m->active_song.active) {
            // Song finished early - rest is silence (buffer zero-initialized)
            break;
        }
    }
    
    // Stop the song
    xfm_song_play(m, 0, false);
    
    // Write WAV file
    int result = write_wav_file(filename, m->sample_rate, total_samples, buffer);
    
    free(buffer);
    
    if (result < 0) {
        return -1;
    }
    
    printf("Exported song %d to %s (%d samples, %d Hz, %.2f seconds)\n",
           song_id, filename, total_samples, m->sample_rate,
           (float)total_samples / m->sample_rate);
    
    return 0;
}

// =============================================================================
// SFX EXPORT
// =============================================================================

extern "C" int xfm_export_sfx(xfm_module* m, int sfx_id, const char* filename)
{
    if (!m || !filename) return -1;
    if (sfx_id < 0 || sfx_id > 255) return -1;
    if (!m->sfx_present[sfx_id]) {
        fprintf(stderr, "xfm_export_sfx: SFX %d not found\n", sfx_id);
        return -1;
    }
    
    // Get SFX info
    int num_rows = m->sfx_patterns[sfx_id].num_rows;
    int samples_per_row = m->sfx_patterns[sfx_id].samples_per_row;
    
    // Add extra rows for release tail
    int total_rows = num_rows + 4;  // Extra rows for release
    int total_samples = total_rows * samples_per_row;
    
    // Allocate stereo buffer
    int16_t* buffer = (int16_t*)malloc(total_samples * 2 * sizeof(int16_t));
    if (!buffer) {
        fprintf(stderr, "xfm_export_sfx: Failed to allocate buffer\n");
        return -1;
    }
    memset(buffer, 0, total_samples * 2 * sizeof(int16_t));
    
    // Play the SFX
    xfm_voice_id voice = xfm_sfx_play(m, sfx_id, 0);  // Priority 0
    if (voice < 0) {
        free(buffer);
        fprintf(stderr, "xfm_export_sfx: Failed to start SFX %d\n", sfx_id);
        return -1;
    }
    
    // Render in chunks
    int frames_per_chunk = m->buffer_frames;
    int samples_rendered = 0;
    
    while (samples_rendered < total_samples) {
        int frames_to_render = frames_per_chunk;
        if (samples_rendered + frames_to_render > total_samples) {
            frames_to_render = total_samples - samples_rendered;
        }
        
        // Render this chunk
        xfm_mix_sfx(m, buffer + (samples_rendered * 2), frames_to_render);
        samples_rendered += frames_to_render;
        
        // Check if SFX finished
        bool sfx_active = false;
        for (int i = 0; i < 6; i++) {
            if (m->active_sfx[i].active && m->active_sfx[i].sfx_id == sfx_id) {
                sfx_active = true;
                break;
            }
        }
        
        if (!sfx_active) {
            // SFX finished - add a bit more silence for release tail
            break;
        }
    }
    
    // Write WAV file
    int result = write_wav_file(filename, m->sample_rate, total_samples, buffer);
    
    free(buffer);
    
    if (result < 0) {
        return -1;
    }
    
    printf("Exported SFX %d to %s (%d samples, %d Hz, %.2f seconds)\n",
           sfx_id, filename, total_samples, m->sample_rate,
           (float)total_samples / m->sample_rate);
    
    return 0;
}
