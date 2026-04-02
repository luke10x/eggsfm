// =============================================================================
// xfm_textplayer.cpp — Console-based FM text file player with hot-reloading
// =============================================================================
//
// Usage: xfm_textplayer <song.txt> <ticks_per_second> <ticks_per_row>
//
// Features:
//   - Loads patches from patch_XX.h files in current directory
//   - Plays song from text file (Furnace-like format)
//   - Hot-reloads files on change with exponential backoff
//   - Ctrl+X to exit
//   - Prints every line it plays
//
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "xfm_api.h"

// =============================================================================
// Configuration
// =============================================================================

static constexpr int SAMPLE_RATE = 44100;
static constexpr int BUFFER_FRAMES = 256;
static constexpr int MAX_PATCHES = 256;
static constexpr int MAX_BACKOFF = 32;  // seconds

// =============================================================================
// Global State
// =============================================================================

struct GlobalState {
    xfm_module* music_module = nullptr;
    std::string song_filename;
    int ticks_per_second = 60;
    int ticks_per_row = 6;
    bool running = true;
    bool playing = false;
    bool has_errors = false;
    
    // File tracking
    std::map<int, std::string> patch_files;  // patch_id -> filename
    std::map<int, time_t> patch_mtime;       // patch_id -> last modified time
    time_t song_mtime = 0;
    
    // Error tracking with backoff
    time_t last_error_time = 0;
    int backoff_seconds = 2;  // starts at 2s
    time_t next_retry_time = 0;
    int retry_count = 0;
    
    // Parsed data
    std::map<int, xfm_patch_opn> patches;
    std::set<int> required_patches;  // patches used in song
    std::string song_pattern;
    int song_num_rows = 0;
    std::vector<std::string> song_lines;  // Store original lines for display
    
    // Playback tracking
    int last_played_row = -1;
};

static GlobalState g_state;
static SDL_AudioDeviceID g_audio_dev;

// =============================================================================
// Utility Functions
// =============================================================================

static time_t get_file_mtime(const std::string& filename) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_mtime;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static void print_error(const std::string& msg) {
    std::cerr << "\033[31m[ERROR]\033[0m " << msg << std::endl;
}

static void print_warning(const std::string& msg) {
    std::cerr << "\033[33m[WARN]\033[0m " << msg << std::endl;
}

static void print_info(const std::string& msg) {
    std::cout << "\033[36m[INFO]\033[0m " << msg << std::endl;
}

static void print_success(const std::string& msg) {
    std::cout << "\033[32m[OK]\033[0m " << msg << std::endl;
}

static void print_playing(const std::string& msg) {
    std::cout << "\033[35m[PLAY]\033[0m " << msg << std::endl;
}

// =============================================================================
// Patch File Parser (C++-like syntax)
// =============================================================================

struct ParseError {
    std::string message;
    int line;
    int col;
    ParseError(const std::string& msg, int l, int c = 0) : message(msg), line(l), col(c) {}
};

static bool parse_patch_file(const std::string& filename, int patch_id,
                             xfm_patch_opn& patch, std::vector<ParseError>& errors) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errors.push_back(ParseError("Cannot open file", 0));
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    // Initialize patch with defaults
    memset(&patch, 0, sizeof(patch));

    // Helper to find integer field value
    auto find_int_field = [&](const std::string& name, int lo, int hi, uint8_t& out) -> bool {
        std::string pattern = "." + name + " =";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) {
            pattern = name + " =";
            pos = content.find(pattern);
        }
        if (pos == std::string::npos) return false;

        pos += pattern.length();
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) pos++;

        bool negative = false;
        if (pos < content.size() && content[pos] == '-') {
            negative = true;
            pos++;
        }

        int val = 0;
        while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
            val = val * 10 + (content[pos] - '0');
            pos++;
        }

        if (val < lo || val > hi) {
            char buf[128];
            snprintf(buf, sizeof(buf), "%s must be %d-%d, got %d", name.c_str(), lo, hi, val);
            errors.push_back(ParseError(buf, 0));
            return false;
        }

        out = static_cast<uint8_t>(negative ? -val : val);
        return true;
    };

    // Parse global fields
    find_int_field("ALG", 0, 7, patch.ALG);
    find_int_field("FB", 0, 7, patch.FB);
    find_int_field("AMS", 0, 3, patch.AMS);
    find_int_field("FMS", 0, 7, patch.FMS);

    // Parse operator arrays - find all operator structs
    std::vector<std::map<std::string, int>> ops(4);

    size_t op_array = content.find(".op");
    if (op_array != std::string::npos) {
        size_t array_start = content.find('{', op_array);
        if (array_start != std::string::npos) {
            size_t pos = array_start + 1;
            int op_idx = 0;

            while (pos < content.size() && op_idx < 4) {
                // Find next operator struct
                size_t struct_start = content.find('{', pos);
                if (struct_start == std::string::npos) break;

                // Find matching close brace
                size_t struct_end = struct_start + 1;
                int depth = 1;
                while (struct_end < content.size() && depth > 0) {
                    if (content[struct_end] == '{') depth++;
                    else if (content[struct_end] == '}') depth--;
                    struct_end++;
                }

                if (depth != 0) {
                    errors.push_back(ParseError("Unmatched brace in operator struct", 0));
                    return false;
                }

                std::string op_content = content.substr(struct_start + 1, struct_end - struct_start - 2);

                // Parse operator fields
                auto parse_op_field = [&](const std::string& name, int lo, int hi, int& out) {
                    std::string pattern = "." + name + " =";
                    size_t p = op_content.find(pattern);
                    if (p == std::string::npos) {
                        pattern = name + " =";
                        p = op_content.find(pattern);
                    }
                    if (p == std::string::npos) return;

                    p += pattern.length();
                    while (p < op_content.size() && (op_content[p] == ' ' || op_content[p] == '\t')) p++;

                    bool neg = false;
                    if (p < op_content.size() && op_content[p] == '-') {
                        neg = true;
                        p++;
                    }

                    int val = 0;
                    while (p < op_content.size() && op_content[p] >= '0' && op_content[p] <= '9') {
                        val = val * 10 + (op_content[p] - '0');
                        p++;
                    }

                    if (val < lo || val > hi) {
                        char buf[128];
                        snprintf(buf, sizeof(buf), "Operator %d: %s must be %d-%d, got %d",
                                op_idx + 1, name.c_str(), lo, hi, val);
                        errors.push_back(ParseError(buf, 0));
                    } else {
                        out = neg ? -val : val;
                    }
                };

                parse_op_field("DT", -3, 3, ops[op_idx]["DT"]);
                parse_op_field("MUL", 0, 15, ops[op_idx]["MUL"]);
                parse_op_field("TL", 0, 127, ops[op_idx]["TL"]);
                parse_op_field("RS", 0, 3, ops[op_idx]["RS"]);
                parse_op_field("AR", 0, 31, ops[op_idx]["AR"]);
                parse_op_field("AM", 0, 1, ops[op_idx]["AM"]);
                parse_op_field("DR", 0, 31, ops[op_idx]["DR"]);
                parse_op_field("SR", 0, 31, ops[op_idx]["SR"]);
                parse_op_field("SL", 0, 15, ops[op_idx]["SL"]);
                parse_op_field("RR", 0, 15, ops[op_idx]["RR"]);
                parse_op_field("SSG", 0, 8, ops[op_idx]["SSG"]);

                op_idx++;
                pos = struct_end;
            }
        }
    }

    // Apply operator values
    for (int op = 0; op < 4; op++) {
        if (ops[op].count("DT")) patch.op[op].DT = ops[op]["DT"];
        if (ops[op].count("MUL")) patch.op[op].MUL = ops[op]["MUL"];
        if (ops[op].count("TL")) patch.op[op].TL = ops[op]["TL"];
        if (ops[op].count("RS")) patch.op[op].RS = ops[op]["RS"];
        if (ops[op].count("AR")) patch.op[op].AR = ops[op]["AR"];
        if (ops[op].count("AM")) patch.op[op].AM = ops[op]["AM"];
        if (ops[op].count("DR")) patch.op[op].DR = ops[op]["DR"];
        if (ops[op].count("SR")) patch.op[op].SR = ops[op]["SR"];
        if (ops[op].count("SL")) patch.op[op].SL = ops[op]["SL"];
        if (ops[op].count("RR")) patch.op[op].RR = ops[op]["RR"];
        if (ops[op].count("SSG")) patch.op[op].SSG = ops[op]["SSG"];
    }

    return errors.empty();
}

// =============================================================================
// Song File Parser
// =============================================================================

static bool parse_song_file(const std::string& filename, std::string& pattern,
                            int& num_rows, std::set<int>& required_patches,
                            std::vector<std::string>& lines_out,
                            std::vector<ParseError>& errors) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        errors.push_back({ "Cannot open song file", 0});
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    int line_num = 0;

    bool in_song = false;
    bool has_begin = false;
    bool has_end = false;
    int begin_line = 0;
    int end_line = 0;

    while (std::getline(file, line)) {
        line_num++;

        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Check for full-line comments (-- at start or only whitespace before --)
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.substr(0, 2) == "--") {
            continue;
        }

        // Check for BEGIN/END tags (must be exact match, no other chars)
        if (trimmed == "BEGIN") {
            if (has_begin) {
                errors.push_back({ "Multiple BEGIN tags found (first at line " + 
                                 std::to_string(begin_line) + ")", line_num});
                return false;
            }
            has_begin = true;
            begin_line = line_num;
            in_song = true;
            continue;
        }

        if (trimmed == "END") {
            if (has_end) {
                errors.push_back({ "Multiple END tags found (first at line " + 
                                 std::to_string(end_line) + ")", line_num});
                return false;
            }
            has_end = true;
            end_line = line_num;
            in_song = false;
            continue;
        }

        // If no BEGIN tag yet, everything before BEGIN is part of song
        // If BEGIN exists, only content between BEGIN and END is part of song
        if (!has_begin) {
            in_song = true;
        }

        if (in_song) {
            // Remove inline comments (everything after --)
            size_t comment_pos = line.find("--");
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }

            trimmed = trim(line);
            if (!trimmed.empty()) {
                lines.push_back(trimmed);

                // Parse instrument from line (format: NOTE INST VOL or NOTE INST)
                // Instrument is 2 hex chars after the note (positions 3-4)
                // Note format: C-4, E-4, OFF, etc (3 chars)
                if (trimmed.length() >= 5) {
                    std::string inst_str = trimmed.substr(3, 2);
                    if (inst_str != ".." && inst_str != "  " && inst_str != "..") {
                        try {
                            int inst = std::stoi(inst_str, nullptr, 16);
                            if (inst >= 0 && inst < MAX_PATCHES) {
                                required_patches.insert(inst);
                            }
                        } catch (...) {
                            // Ignore invalid instrument numbers
                        }
                    }
                }
            }
        }
    }

    file.close();

    if (lines.empty()) {
        errors.push_back({ "No song data found (file may be empty or only comments)", 0});
        return false;
    }

    // Build pattern string for xfm_sfx_declare
    num_rows = lines.size();
    pattern = std::to_string(num_rows) + "\n";
    for (const auto& l : lines) {
        pattern += l + "\n";
    }

    lines_out = lines;
    return true;
}

// =============================================================================
// File Loading and Validation
// =============================================================================

static void scan_patch_files() {
    // Look for patch_XX.h files in current directory
    for (int i = 0; i < MAX_PATCHES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "patch_%02d.h", i);
        
        time_t mtime = get_file_mtime(filename);
        if (mtime > 0) {
            g_state.patch_files[i] = filename;
            g_state.patch_mtime[i] = mtime;
        }
    }
}

static bool load_all_patches(std::vector<ParseError>& errors) {
    bool success = true;
    
    for (auto& kv : g_state.patch_files) {
        int patch_id = kv.first;
        const std::string& filename = kv.second;
        
        xfm_patch_opn patch = {};
        if (!parse_patch_file(filename, patch_id, patch, errors)) {
            success = false;
            print_error("Failed to parse " + filename);
        } else {
            g_state.patches[patch_id] = patch;
        }
    }
    
    return success;
}

static bool validate_and_load() {
    g_state.has_errors = false;
    std::vector<ParseError> errors;
    
    // Clear previous state
    g_state.patches.clear();
    g_state.required_patches.clear();
    g_state.song_lines.clear();
    
    // Scan for patch files
    scan_patch_files();
    
    // Load patches
    if (!load_all_patches(errors)) {
        g_state.has_errors = true;
    }
    
    // Parse song file
    if (!parse_song_file(g_state.song_filename, g_state.song_pattern, 
                         g_state.song_num_rows, g_state.required_patches,
                         g_state.song_lines, errors)) {
        g_state.has_errors = true;
    }
    
    // Check for missing patches
    for (int patch_id : g_state.required_patches) {
        if (g_state.patches.find(patch_id) == g_state.patches.end()) {
            print_error("Missing patch " + std::to_string(patch_id) + 
                       " (file: patch_" + std::to_string(patch_id) + ".h)");
            g_state.has_errors = true;
        }
    }
    
    // Report errors
    for (const auto& err : errors) {
        if (err.line > 0) {
            print_error(err.message + " at line " + std::to_string(err.line));
        } else {
            print_error(err.message);
        }
    }
    
    if (g_state.has_errors) {
        // Set up backoff: 2, 4, 8, 16, 32 (max)
        time_t now = time(nullptr);
        g_state.last_error_time = now;
        g_state.next_retry_time = now + g_state.backoff_seconds;
        
        // Double backoff, cap at MAX_BACKOFF
        if (g_state.backoff_seconds < MAX_BACKOFF) {
            g_state.backoff_seconds = std::min(g_state.backoff_seconds * 2, MAX_BACKOFF);
        }
        g_state.retry_count++;

        std::cout << "\nWill retry in " << (g_state.next_retry_time - now) << " seconds..." << std::endl;
        return false;
    }
    
    // Reset backoff on success
    g_state.backoff_seconds = 2;
    g_state.retry_count = 0;
    g_state.last_error_time = 0;
    g_state.next_retry_time = 0;
    
    // Load patches into FM module
    for (auto& kv : g_state.patches) {
        xfm_patch_set(g_state.music_module, kv.first, &kv.second, 
                     sizeof(xfm_patch_opn), XFM_CHIP_YM2612);
    }
    
    // Declare song as SFX pattern (single channel)
    xfm_sfx_declare(g_state.music_module, 1, g_state.song_pattern.c_str(),
                   g_state.ticks_per_second, g_state.ticks_per_row);
    
    print_success("All files loaded successfully!");
    print_info("Song: " + std::to_string(g_state.song_num_rows) + " rows");
    print_info("Patches: " + std::to_string(g_state.patches.size()) + " loaded");
    
    return true;
}

static bool check_files_changed() {
    bool changed = false;
    
    // Check song file
    time_t song_mtime = get_file_mtime(g_state.song_filename);
    if (song_mtime != g_state.song_mtime) {
        g_state.song_mtime = song_mtime;
        changed = true;
    }
    
    // Check patch files
    for (auto& kv : g_state.patch_files) {
        int patch_id = kv.first;
        const std::string& filename = kv.second;
        time_t mtime = get_file_mtime(filename);
        
        if (mtime != g_state.patch_mtime[patch_id]) {
            g_state.patch_mtime[patch_id] = mtime;
            changed = true;
        }
    }
    
    // Check for new patch files
    for (int i = 0; i < MAX_PATCHES; i++) {
        char filename[64];
        snprintf(filename, sizeof(filename), "patch_%02d.h", i);
        time_t mtime = get_file_mtime(filename);
        
        if (mtime > 0 && g_state.patch_files.find(i) == g_state.patch_files.end()) {
            g_state.patch_files[i] = filename;
            g_state.patch_mtime[i] = mtime;
            changed = true;
        }
    }
    
    return changed;
}

// =============================================================================
// Audio Callback
// =============================================================================

static void audio_callback(void* userdata, Uint8* stream, int len) {
    int16_t* buffer = (int16_t*)stream;
    int frames = len / 4;  // stereo int16
    
    // Mix audio from module
    xfm_mix_sfx(g_state.music_module, buffer, frames);
}

// =============================================================================
// Playback Control
// =============================================================================

static void start_playback() {
    // Stop any previous playback
    xfm_sfx_play(g_state.music_module, 0, 0);  // Dummy to reset
    
    // Start playing SFX (we use SFX system for single-channel playback)
    xfm_sfx_play(g_state.music_module, 1, 5);
    g_state.playing = true;
    g_state.last_played_row = -1;
    
    print_playing("Starting playback...");
}

static void stop_playback() {
    g_state.playing = false;
}

// =============================================================================
// Main Loop
// =============================================================================

static void print_usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <song.txt> <ticks_per_second> <ticks_per_row>" << std::endl;
    std::cerr << std::endl;
    std::cerr << "  song.txt          - Song file in Furnace-like format" << std::endl;
    std::cerr << "  ticks_per_second  - Number of ticks per second (e.g., 60)" << std::endl;
    std::cerr << "  ticks_per_row     - Number of ticks per row (e.g., 6)" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Controls:" << std::endl;
    std::cerr << "  Ctrl+X - Exit" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Patch files should be named patch_XX.h in current directory." << std::endl;
    std::cerr << "Song file supports BEGIN/END tags and -- comments." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }
    
    g_state.song_filename = argv[1];
    g_state.ticks_per_second = std::atoi(argv[2]);
    g_state.ticks_per_row = std::atoi(argv[3]);
    
    if (g_state.ticks_per_second <= 0 || g_state.ticks_per_row <= 0) {
        print_error("Invalid tick rate or speed values");
        return 1;
    }
    
    // Check if song file exists
    if (get_file_mtime(g_state.song_filename) == 0) {
        print_error("Song file not found: " + g_state.song_filename);
        return 1;
    }
    
    std::cout << "========================================" << std::endl;
    std::cout << "  FM Text Player" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Song: " << g_state.song_filename << std::endl;
    std::cout << "Tick rate: " << g_state.ticks_per_second << " Hz" << std::endl;
    std::cout << "Speed: " << g_state.ticks_per_row << " ticks/row" << std::endl;
    std::cout << "Press Ctrl+X to exit" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        print_error(std::string("SDL initialization failed: ") + SDL_GetError());
        return 1;
    }
    
    // Set up audio
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = BUFFER_FRAMES;
    want.callback = audio_callback;
    
    g_audio_dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (g_audio_dev == 0) {
        print_error(std::string("Failed to open audio device: ") + SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    // Create FM module
    g_state.music_module = xfm_module_create(SAMPLE_RATE, BUFFER_FRAMES, XFM_CHIP_YM2612);
    if (!g_state.music_module) {
        print_error("Failed to create FM module");
        SDL_CloseAudioDevice(g_audio_dev);
        SDL_Quit();
        return 1;
    }
    
    // Start audio
    SDL_PauseAudioDevice(g_audio_dev, 0);
    
    // Initial load attempt
    bool loaded = validate_and_load();
    
    // Main loop
    Uint32 last_row_check = 0;
    int prev_sfx_row = -1;
    
    while (g_state.running) {
        // Handle SDL events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                g_state.running = false;
            }
        }
        
        // Check for Ctrl+X
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);
        if (keystate[SDL_SCANCODE_X] && (keystate[SDL_SCANCODE_LCTRL] || keystate[SDL_SCANCODE_RCTRL])) {
            std::cout << "\nExiting..." << std::endl;
            g_state.running = false;
            break;
        }
        
        time_t now = time(nullptr);
        
        // Check if we should retry loading
        if (g_state.has_errors && now >= g_state.next_retry_time) {
            std::cout << "\n\033[33mRetrying file load...\033[0m" << std::endl;
            loaded = validate_and_load();
            
            if (loaded && !g_state.playing) {
                start_playback();
            }
        }
        
        // Check for file changes when not playing or after errors
        if (!g_state.playing || g_state.has_errors) {
            if (check_files_changed()) {
                std::cout << "\n\033[33mFiles changed, reloading...\033[0m" << std::endl;
                loaded = validate_and_load();
                
                if (loaded && !g_state.playing) {
                    start_playback();
                }
            }
        }
        
        // Start playback if loaded and not playing
        if (loaded && !g_state.playing) {
            start_playback();
        }
        
        // Track and print current row being played
        if (g_state.playing) {
            Uint32 tick = SDL_GetTicks();
            if (tick - last_row_check >= 50) {  // Check every 50ms
                last_row_check = tick;
                
                // We need to track the current row in the SFX playback
                // For now, estimate based on time
                static Uint32 playback_start = 0;
                if (playback_start == 0) playback_start = tick;
                
                Uint32 elapsed_ms = tick - playback_start;
                float ms_per_row = (1000.0f / g_state.ticks_per_second) * g_state.ticks_per_row;
                int estimated_row = (int)(elapsed_ms / ms_per_row) % g_state.song_num_rows;
                
                // Check if SFX finished (loop detection)
                // When row wraps around, print the line
                if (estimated_row != prev_sfx_row && estimated_row >= 0 && 
                    estimated_row < (int)g_state.song_lines.size()) {
                    std::cout << "\r\033[35m[ROW " << std::to_string(estimated_row) << "] " 
                              << g_state.song_lines[estimated_row] << "\033[0m          " << std::flush;
                    prev_sfx_row = estimated_row;
                }
            }
        }
        
        // Small sleep to avoid busy-waiting
        SDL_Delay(16);
    }
    
    // Cleanup
    SDL_PauseAudioDevice(g_audio_dev, 1);
    SDL_CloseAudioDevice(g_audio_dev);
    
    if (g_state.music_module) {
        xfm_module_destroy(g_state.music_module);
    }
    
    SDL_Quit();
    
    std::cout << std::endl;
    return 0;
}
