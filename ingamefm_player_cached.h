#pragma once
#include <sstream>
// =============================================================================
// ingamefm_player.h — Configurable Sample Rate Version
// =============================================================================
#include "ingamefm_patchlib.h"
#include <SDL.h>
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <memory>

static constexpr int NOTE_NONE = -1;
static constexpr int NOTE_OFF  = -2;

struct IngameFMEvent  { int note; int instrument; int volume; };
struct IngameFMRow    { std::vector<IngameFMEvent> channels; };
struct IngameFMSong   { int num_rows=0; int num_channels=0; std::vector<IngameFMRow> rows; };

// --- Helper Functions (Unchanged) ---
static std::string trim_right(const std::string& s) {
    size_t end = s.find_last_not_of(" \t\r\n");
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

static int parse_note_field(const char* nc, int line_num) {
    if (nc[0]=='.'&&nc[1]=='.'&&nc[2]=='.') return NOTE_NONE;
    if (nc[0]=='O'&&nc[1]=='F'&&nc[2]=='F') return NOTE_OFF;
    int semitone = -1;
    switch (nc[0]) {
    case 'C': semitone=0;  break; case 'D': semitone=2;  break;
    case 'E': semitone=4;  break; case 'F': semitone=5;  break;
    case 'G': semitone=7;  break; case 'A': semitone=9;  break;
    case 'B': semitone=11; break;
    default: throw std::runtime_error(std::string("Line ")+std::to_string(line_num)+": bad note '"+std::string(nc,3)+"'");
    }
    if      (nc[1]=='#') semitone++;
    else if (nc[1]=='-') {}
    else throw std::runtime_error(std::string("Line ")+std::to_string(line_num)+": bad accidental");
    if (nc[2]<'0'||nc[2]>'9') throw std::runtime_error(std::string("Line ")+std::to_string(line_num)+": bad octave");
    
    // FIX: Changed base offset from 12 to 24 to correct pitch octave
    return 24 + (nc[2]-'0')*12 + semitone; 
}

static int parse_hex2(const char* p, int line_num, const char* field_name) {
    if (p[0]=='.'&&p[1]=='.') return -1;
    auto hex = [&](char c) -> int {
        if (c>='0'&&c<='9') return c-'0';
        if (c>='A'&&c<='F') return c-'A'+10;
        if (c>='a'&&c<='f') return c-'a'+10;
        throw std::runtime_error(std::string("Line ")+std::to_string(line_num)+": bad hex in "+field_name);
        return 0;
    };
    return hex(p[0])*16+hex(p[1]);
}

static IngameFMEvent parse_channel_column(const std::string& row, size_t colStart, size_t colWidth, int line_num) {
    IngameFMEvent ev; ev.note=NOTE_NONE; ev.instrument=-1; ev.volume=-1;
    std::string col = row.substr(colStart, colWidth);
    while (col.size()<colWidth) col+='.';
    ev.note = parse_note_field(col.c_str(), line_num);
    if (col.size()>=5) ev.instrument = parse_hex2(col.c_str()+3, line_num, "instrument");
    if (col.size()>=7) ev.volume     = parse_hex2(col.c_str()+5, line_num, "volume");
    return ev;
}

static IngameFMSong parse_ingamefm_song(const std::string& text) {
    std::vector<std::string> lines;
    { std::istringstream ss(text); std::string l; while(std::getline(ss,l)) lines.push_back(trim_right(l)); }
    while (!lines.empty()&&lines.front().empty()) lines.erase(lines.begin());
    if (lines.empty()) throw std::runtime_error("Song text is empty");
    int lineNum=1;
    if (lines[0].find("org.tildearrow.furnace")!=std::string::npos) { lines.erase(lines.begin()); lineNum++; }
    while (!lines.empty()&&lines.front().empty()) { lines.erase(lines.begin()); lineNum++; }
    if (lines.empty()) throw std::runtime_error("Song text has no content after header");
    
    int num_rows=0;
    { const std::string& rc=lines[0]; char* e=nullptr; long v=std::strtol(rc.c_str(),&e,10);
      while(e&&*e==' ')e++; if(!e||*e!='\0') throw std::runtime_error("Line "+std::to_string(lineNum)+": expected row count");
      if(v<=0||v>65536) throw std::runtime_error("Line "+std::to_string(lineNum)+": row count out of range");
      num_rows=static_cast<int>(v); lines.erase(lines.begin()); lineNum++; }
    
    IngameFMSong song; song.num_rows=num_rows;
    std::vector<std::string> dataLines;
    for (const auto& l:lines) if(!l.empty()) dataLines.push_back(l);
    if (dataLines.empty()) throw std::runtime_error("Song has row count but no data lines");
    
    auto split_channels=[](const std::string& row)->std::vector<std::string>{
        std::vector<std::string> cols; std::string cur;
        for(char c:row){ if(c=='|'){cols.push_back(cur);cur.clear();}else cur+=c; }
        if(!cur.empty()) cols.push_back(cur); return cols; };
    
    std::vector<std::string> firstCols=split_channels(dataLines[0]);
    song.num_channels=static_cast<int>(firstCols.size());
    std::vector<size_t> colWidths; for(const auto& c:firstCols) colWidths.push_back(c.size());
    for(int ch=0;ch<song.num_channels;ch++)
        if(colWidths[ch]<7) throw std::runtime_error("Channel "+std::to_string(ch)+" too narrow");
    
    int parsedRows=0;
    for(const auto& line:dataLines) {
        if(parsedRows>=num_rows) break;
        IngameFMRow row; std::vector<std::string> cols=split_channels(line);
        while(static_cast<int>(cols.size())<song.num_channels) cols.push_back("");
        for(int ch=0;ch<song.num_channels;ch++) {
            std::string col=cols[ch]; while(col.size()<colWidths[ch]) col+='.';
            row.channels.push_back(parse_channel_column(col,0,colWidths[ch],lineNum)); }
        song.rows.push_back(row); parsedRows++; lineNum++; 
    }
    if(parsedRows<num_rows) throw std::runtime_error("Song declared "+std::to_string(num_rows)+" rows but found "+std::to_string(parsedRows));
    return song;
}

struct IngameFMChannelState { bool active=false; int instrument=0; int volume=0x7F; };

struct SfxVoiceState {
    int  sfx_id=-1; int priority=0; int ticks_remaining=0;
    int  current_row=0; int sample_in_row=0; int samples_per_row=0;
    bool pending_has_note=false; bool pending_is_off=false;
    int  pending_note=0; int pending_inst=0; int pending_vol=0x7F;
    int  last_instrument=0; int last_volume=0x7F;
    int  cache_key_id = -1;
    bool active() const { return sfx_id>=0 && ticks_remaining>0; }
};

enum class SongChangeWhen { NOW, AT_PATTERN_END };

class IngameFMPlayer
{
public:
    static constexpr int MAX_CHANNELS   = 6;
    static constexpr int MAX_SFX_VOICES = 6;
    // SAMPLE_RATE is no longer hardcoded here. It is now an instance variable.

    // --- CONFIGURATION ---
    
    // Set the sample rate BEFORE defining songs or loading xxd data.
    // Default is 44100. Common values: 22050, 44100, 48000.
    void set_sample_rate(int rate) {
        if(rate <= 0) throw std::runtime_error("Sample rate must be > 0");
        sample_rate_ = rate;
    }

    int get_sample_rate() const { return sample_rate_; }

    void set_music_volume(float v) { music_vol_.store(std::max(0.f,std::min(1.f,v))); }
    void set_sfx_volume  (float v) { sfx_vol_  .store(std::max(0.f,std::min(1.f,v))); }

    void add_patch(int instrument_id, const YM2612Patch& patch) {
        if(instrument_id<0||instrument_id>255) throw std::runtime_error("instrument_id must be 0-255");
        patches_[instrument_id]=patch; 
        patches_present_[instrument_id]=true;
    }

    // ── Song Definition ──────────────────────────────────────────────────────
    void song_define(int id, const std::string& text, int tick_rate, int speed) {
        if(tick_rate<=0) throw std::runtime_error("tick_rate must be > 0");
        if(speed<=0)     throw std::runtime_error("speed must be > 0");

        DefinedSong def;
        def.song = parse_ingamefm_song(text);
        def.tick_rate = tick_rate;
        def.speed = speed;
        // UPDATED: Uses instance variable sample_rate_
        def.samples_per_row = static_cast<int>(static_cast<double>(sample_rate_)/tick_rate*speed);
        def.valid = true;
        def.cache_key_id = id;

        if(use_cache_) {
            if(song_cache_.find(id) == song_cache_.end()) {
                song_cache_[id] = prerender_song(def.song, tick_rate, speed, patches_, patches_present_, sample_rate_);
            }
        }
        defined_songs_[id] = std::move(def);
    }

    // ── Song Selection ───────────────────────────────────────────────────────
    void song_select(int id, bool loop = false) {
        auto it = defined_songs_.find(id);
        if(it == defined_songs_.end()) {
            throw std::runtime_error("song_select: ID " + std::to_string(id) + " not defined.");
        }
        const DefinedSong& def = it->second;

        if(!ym_music_) {
            ym_music_ = std::make_unique<IngameFMChip>();
            ym_sfx_   = std::make_unique<IngameFMChip>();
            for(int c=0;c<3;++c) {
                ym_music_->write(0,0xB4+c,0xC0); ym_music_->write(1,0xB4+c,0xC0);
                ym_sfx_  ->write(0,0xB4+c,0xC0); ym_sfx_  ->write(1,0xB4+c,0xC0);
            }
        }

        for(int ch=0; ch<MAX_CHANNELS; ch++) ym_music_->key_off(ch);

        song_ = def.song;
        tick_rate_ = def.tick_rate;
        speed_ = def.speed;
        samples_per_row_ = def.samples_per_row;
        current_song_cache_key_id_ = def.cache_key_id;
        current_song_id_ = id;

        loop_ = loop;
        current_row_.store(0);
        sample_in_row_ = 0;
        finished_.store(false);
        pending_song_.pending = false;

        for(auto& c : ch_state_) c = IngameFMChannelState{};
        for(auto& p : pending_)  p = {};

        process_row(0);
        commit_keyon();
        sample_in_row_ = KEY_OFF_GAP_SAMPLES;
    }

    // ── Song Change ──────────────────────────────────────────────────────────
    void change_song(int id, SongChangeWhen when = SongChangeWhen::AT_PATTERN_END, int start_row = 0) {
        auto it = defined_songs_.find(id);
        if(it == defined_songs_.end()) {
            throw std::runtime_error("change_song: ID " + std::to_string(id) + " not defined.");
        }
        const DefinedSong& def = it->second;

        PendingSong ps;
        ps.song            = def.song;
        ps.tick_rate       = def.tick_rate;
        ps.speed           = def.speed;
        ps.samples_per_row = def.samples_per_row;
        ps.start_row       = std::max(0, std::min(start_row, static_cast<int>(ps.song.rows.size())-1));
        ps.when            = when;
        ps.pending         = true;
        ps.cache_key_id    = def.cache_key_id;

        pending_song_ = std::move(ps);
    }

    int get_current_row()  const { return current_row_.load(); }
    int get_song_length()  const { return static_cast<int>(song_.rows.size()); }

    void sfx_set_voices(int n) {
        if(n<1||n>MAX_SFX_VOICES) throw std::runtime_error("sfx_set_voices: n must be 1.."+std::to_string(MAX_SFX_VOICES));
        sfx_voices_=n;
    }

    void sfx_define(int id, const std::string& pattern, int tick_rate, int speed) {
        if(id<0||id>255) throw std::runtime_error("sfx_define: id must be 0-255");
        if(tick_rate<=0||speed<=0) throw std::runtime_error("sfx_define: tick_rate and speed must be > 0");
        
        SfxDef def; 
        def.song=parse_ingamefm_song(pattern); 
        def.tick_rate=tick_rate; 
        def.speed=speed;
        // UPDATED: Uses instance variable sample_rate_
        def.samples_per_row=static_cast<int>(static_cast<double>(sample_rate_)/tick_rate*speed);
        def.cache_key_id = id;
        
        if(use_cache_) {
            if(sfx_cache_.find(id) == sfx_cache_.end()) {
                sfx_cache_[id] = prerender_sfx(def.song, tick_rate, speed, patches_, patches_present_, sample_rate_);
            }
        }
        sfx_defs_[id]=std::move(def);
        sfx_defs_present_[id]=true;
    }

    void play(bool loop=false) {
        if(song_.rows.empty()) throw std::runtime_error("No song loaded");
        reset_state(loop);
        SDL_AudioSpec desired{}; 
        // UPDATED: Uses instance variable sample_rate_ for SDL
        desired.freq=sample_rate_; 
        desired.format=AUDIO_S16SYS;
        desired.channels=2; desired.samples=512; desired.callback=s_audio_callback; desired.userdata=this;
        SDL_AudioSpec obtained{};
        SDL_AudioDeviceID dev=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
        if(dev==0) throw std::runtime_error(std::string("SDL_OpenAudioDevice failed: ")+SDL_GetError());
        SDL_PauseAudioDevice(dev,0);
        while(!finished_.load()) SDL_Delay(10);
        SDL_CloseAudioDevice(dev);
    }

    void start(SDL_AudioDeviceID dev, bool loop=false) {
        if(!ym_music_) {
             ym_music_ = std::make_unique<IngameFMChip>();
             ym_sfx_   = std::make_unique<IngameFMChip>();
             for(int c=0;c<3;++c) {
                ym_music_->write(0,0xB4+c,0xC0); ym_music_->write(1,0xB4+c,0xC0);
                ym_sfx_  ->write(0,0xB4+c,0xC0); ym_sfx_  ->write(1,0xB4+c,0xC0);
            }
        }
        
        if(song_.rows.empty()) throw std::runtime_error("No song loaded");
        SDL_LockAudioDevice(dev); 
        loop_=loop; current_row_.store(0); sample_in_row_=0; finished_.store(false); pending_song_.pending=false;
        for(auto& c:ch_state_) c=IngameFMChannelState{};
        for(auto& p:pending_)  p={};
        for(auto& v:sfx_voice_) v=SfxVoiceState{};
        for(int c=0;c<3;++c) {
            ym_music_->write(0,0xB4+c,0xC0); ym_music_->write(1,0xB4+c,0xC0);
            ym_sfx_  ->write(0,0xB4+c,0xC0); ym_sfx_  ->write(1,0xB4+c,0xC0);
        }
        if(!song_.rows.empty()) {
            process_row(0);
            commit_keyon();
            sample_in_row_=KEY_OFF_GAP_SAMPLES;
        }
        SDL_UnlockAudioDevice(dev);
    }

    void stop(SDL_AudioDeviceID dev) {
        SDL_LockAudioDevice(dev);
        finished_.store(true);
        if(ym_music_) for(int ch=0;ch<MAX_CHANNELS;ch++) ym_music_->key_off(ch);
        if(ym_sfx_)   for(int v=0;v<sfx_voices_;v++)     ym_sfx_->key_off(v);
        SDL_UnlockAudioDevice(dev);
    }

    bool is_finished() const { return finished_.load(); }
    IngameFMChip* chip() { return ym_music_.get(); }

    void sfx_play(int id, int priority, int duration_ticks) {
        if(!sfx_defs_present_[id]) return;
        if(sfx_voices_==0)         return;
        if(!ym_sfx_)               return;
        
        const SfxDef& def = sfx_defs_[id];
        int best_v = -1, best_prio = priority;
        
        for(int v=sfx_voices_-1; v>=0; --v) {
            if(!sfx_voice_[v].active()) { best_v=v; best_prio=-1; break; }
            if(sfx_voice_[v].priority < best_prio) { best_v=v; best_prio=sfx_voice_[v].priority; }
        }
        if(best_v<0) return;

        ym_sfx_->key_off(best_v);
        
        SfxVoiceState& vs = sfx_voice_[best_v];
        vs.sfx_id          = id;
        vs.priority        = priority;
        vs.ticks_remaining = duration_ticks;
        vs.current_row     = 0;
        vs.sample_in_row   = 0;
        vs.samples_per_row = def.samples_per_row;
        vs.pending_has_note= false;
        vs.pending_is_off  = false;
        vs.last_instrument = 0;
        vs.last_volume     = 0x7F;
        vs.cache_key_id    = def.cache_key_id;

        sfx_process_row(best_v, 0, true);
        sfx_commit_keyon(best_v);
        vs.sample_in_row = KEY_OFF_GAP_SAMPLES;
    }

    static void s_audio_callback(void* userdata, Uint8* stream, int len) {
        static_cast<IngameFMPlayer*>(userdata)->audio_callback(
            reinterpret_cast<int16_t*>(stream), len/4);
    }

    bool use_cache_ = false;

    // =======================================================================
    // CACHE DUMP/LOAD (For xxd embedding workflow)
    // =======================================================================

    void song_dump(int id, const std::string& filename) {
        if(!use_cache_) {
            std::fprintf(stderr, "[IngameFM] Warning: song_dump called but use_cache_ is false.\n");
            return;
        }
        auto it = song_cache_.find(id);
        if(it == song_cache_.end() || !it->second.valid) {
            std::fprintf(stderr, "[IngameFM] Error: No cached data found for Song ID %d.\n", id);
            return;
        }
        _write_cache_file(filename, it->second.samples, it->second.samples_per_row, it->second.total_rows, 'M');
    }

    void sfx_dump(int id, const std::string& filename) {
        if(!use_cache_) {
            std::fprintf(stderr, "[IngameFM] Warning: sfx_dump called but use_cache_ is false.\n");
            return;
        }
        auto it = sfx_cache_.find(id);
        if(it == sfx_cache_.end() || !it->second.valid) {
            std::fprintf(stderr, "[IngameFM] Error: No cached data found for SFX ID %d.\n", id);
            return;
        }
        _write_cache_file(filename, it->second.samples, it->second.samples_per_row, it->second.total_rows, 'S');
    }

    // UPDATED: xxd loaders now rely on the internal sample_rate_ for calculations if needed,
    // but primarily they trust the data in the file. The tick_rate/speed args are kept 
    // to reconstruct the 'Definition' metadata correctly.
    void song_from_xxd(int id, const uint8_t* data, size_t len, int tick_rate, int speed) {
        CachedSong cached = _parse_cache_data<CachedSong>(data, len, 'M');
        if(cached.valid) {
            // Recalculate based on current instance sample_rate_
            cached.samples_per_row = static_cast<int>(static_cast<double>(sample_rate_)/tick_rate*speed);
            if(cached.samples_per_row > 0) {
                cached.total_rows = static_cast<int>(cached.samples.size() / 2 / cached.samples_per_row);
            }
            song_cache_[id] = cached;
            
            DefinedSong def;
            def.tick_rate = tick_rate;
            def.speed = speed;
            def.samples_per_row = cached.samples_per_row;
            def.cache_key_id = id;
            def.valid = true; 
            
            def.song.num_rows = cached.total_rows;
            def.song.num_channels = MAX_CHANNELS;
            // FIX: Create dummy rows to prevent crash in process_row
            def.song.rows.resize(cached.total_rows);
            for(auto& row : def.song.rows) {
                row.channels.resize(MAX_CHANNELS);
                for(auto& ev : row.channels) {
                    ev.note = NOTE_NONE; ev.instrument = -1; ev.volume = -1;
                }
            }

            defined_songs_[id] = std::move(def);
            std::printf("[IngameFM] Loaded Song ID %d from xxd (%d rows, %d samples, SR=%d).\n", 
                        id, cached.total_rows, (int)cached.samples.size(), sample_rate_);
        } else {
            std::fprintf(stderr, "[IngameFM] Failed to parse Song ID %d from xxd data.\n", id);
        }
    }

    void sfx_from_xxd(int id, const uint8_t* data, size_t len, int tick_rate, int speed) {
        CachedSfx cached = _parse_cache_data<CachedSfx>(data, len, 'S');
        if(cached.valid) {
            cached.samples_per_row = static_cast<int>(static_cast<double>(sample_rate_)/tick_rate*speed);
            if(cached.samples_per_row > 0) {
                cached.total_rows = static_cast<int>(cached.samples.size() / 2 / cached.samples_per_row);
            }
            sfx_cache_[id] = cached;

            SfxDef def;
            def.tick_rate = tick_rate;
            def.speed = speed;
            def.samples_per_row = cached.samples_per_row;
            def.cache_key_id = id;
            
            def.song.num_rows = cached.total_rows;
            def.song.num_channels = 1;
            // FIX: Create dummy rows
            def.song.rows.resize(cached.total_rows);
            for(auto& row : def.song.rows) {
                row.channels.resize(1);
                row.channels[0].note = NOTE_NONE; row.channels[0].instrument = -1; row.channels[0].volume = -1;
            }
            
            sfx_defs_[id] = std::move(def);
            sfx_defs_present_[id] = true;

            std::printf("[IngameFM] Loaded SFX ID %d from xxd (%d rows, %d samples, SR=%d).\n", 
                        id, cached.total_rows, (int)cached.samples.size(), sample_rate_);
        } else {
            std::fprintf(stderr, "[IngameFM] Failed to parse SFX ID %d from xxd data.\n", id);
        }
    }

private:
    struct DefinedSong {
        IngameFMSong song;
        int tick_rate = 60;
        int speed = 6;
        int samples_per_row = 0;
        int cache_key_id = -1;
        bool valid = false;
    };
    std::unordered_map<int, DefinedSong> defined_songs_;
    int current_song_id_ = -1;

    struct SfxDef {
        IngameFMSong song;
        int tick_rate=60;
        int speed=6;
        int samples_per_row=0;
        int cache_key_id = -1;
    };
    std::array<SfxDef, 256>         sfx_defs_{};
    std::array<bool,   256>         sfx_defs_present_{};
    int                             sfx_voices_ = 3;
    std::array<SfxVoiceState, MAX_SFX_VOICES> sfx_voice_{};

    IngameFMSong song_;
    int  tick_rate_=60, speed_=6, samples_per_row_=0;
    std::atomic<int> current_row_{0};
    int  sample_in_row_=0;
    bool loop_=false;
    std::atomic<bool> finished_{false};
    std::array<IngameFMChannelState, MAX_CHANNELS> ch_state_{};
    std::array<YM2612Patch, 256>  patches_{};
    std::array<bool, 256>         patches_present_{};
    
    struct PendingNote { bool has_note=false; bool is_off=false; int midi_note=0; int instId=0; int volume=0x7F; };
    std::array<PendingNote, MAX_CHANNELS> pending_{};

    struct PendingSong {
        IngameFMSong   song;
        int            tick_rate       = 60;
        int            speed           = 6;
        int            samples_per_row = 0;
        int            start_row       = 0;
        SongChangeWhen when            = SongChangeWhen::AT_PATTERN_END;
        bool           pending         = false;
        int            cache_key_id    = -1;
    };
    PendingSong pending_song_{};

    std::unique_ptr<IngameFMChip> ym_music_;
    std::unique_ptr<IngameFMChip> ym_sfx_;
    std::atomic<float> music_vol_{1.0f};
    std::atomic<float> sfx_vol_  {1.0f};
    static constexpr int KEY_OFF_GAP_SAMPLES = 44;

    // NEW: Configurable Sample Rate
    int sample_rate_ = 44100; 

    struct CachedSong {
        std::vector<int16_t> samples; 
        int samples_per_row = 0;
        int total_rows = 0;
        bool valid = false;
    };
    struct CachedSfx {
        std::vector<int16_t> samples; 
        int samples_per_row = 0;
        int total_rows = 0;
        bool valid = false;
    };
    std::unordered_map<int, CachedSong> song_cache_;
    std::unordered_map<int, CachedSfx> sfx_cache_;
    int current_song_cache_key_id_ = -1;

    void reset_state(bool loop) {
        if(!ym_music_) {
            ym_music_=std::make_unique<IngameFMChip>();
            ym_sfx_  =std::make_unique<IngameFMChip>();
            for(int c=0;c<3;++c) {
                ym_music_->write(0,0xB4+c,0xC0); ym_music_->write(1,0xB4+c,0xC0);
                ym_sfx_  ->write(0,0xB4+c,0xC0); ym_sfx_  ->write(1,0xB4+c,0xC0);
            }
        }
        loop_=loop; current_row_.store(0); sample_in_row_=0; finished_.store(false); pending_song_.pending=false;
        for(auto& c:ch_state_) c=IngameFMChannelState{};
        for(auto& p:pending_)  p={};
        for(auto& v:sfx_voice_) v=SfxVoiceState{};
        
        if(!song_.rows.empty()) {
            process_row(0);
            commit_keyon();
            sample_in_row_=KEY_OFF_GAP_SAMPLES;
        }
    }

    static CachedSong prerender_song(const IngameFMSong& song, int tick_rate, int speed,
                                     const std::array<YM2612Patch,256>& patches,
                                     const std::array<bool,256>& patches_present,
                                     int sample_rate) { // Added arg
        CachedSong result;
        if(song.rows.empty()) return result;
        // UPDATED: Uses passed sample_rate
        int samples_per_row = static_cast<int>(static_cast<double>(sample_rate)/tick_rate*speed);
        int total_samples = song.num_rows * samples_per_row;
        result.samples.resize(total_samples * 2); 
        result.samples_per_row = samples_per_row;
        result.total_rows = song.num_rows;
        result.valid = true;

        IngameFMChip chip;
        for(int c=0;c<3;++c) { chip.write(0,0xB4+c,0xC0); chip.write(1,0xB4+c,0xC0); }
        std::array<IngameFMChannelState, MAX_CHANNELS> ch_state{};
        std::array<PendingNote, MAX_CHANNELS> pending{};

        for(int rowIdx=0; rowIdx<song.num_rows; ++rowIdx) {
            int total = song.num_rows;
            if (total > 0 && (rowIdx % (total / 10 + 1)) == 0) { 
                int percent = (rowIdx * 100) / total;
                if (percent % 10 == 0) std::printf("[IngameFM] Pre-rendering: %d%% complete\n", percent);
            }
            const IngameFMRow& row = song.rows[rowIdx];
            int numCh = std::min((int)row.channels.size(), MAX_CHANNELS);
            for(int ch=0;ch<numCh;ch++) {
                const IngameFMEvent& ev=row.channels[ch];
                pending[ch]={};
                if(ev.instrument>=0) ch_state[ch].instrument=ev.instrument;
                if(ev.volume>=0)     ch_state[ch].volume=ev.volume;
                if(ev.note==NOTE_OFF) {
                    chip.key_off(ch); ch_state[ch].active=false; pending[ch].is_off=true;
                } else if(ev.note>=0) {
                    chip.key_off(ch); ch_state[ch].active=false;
                    pending[ch].has_note=true; pending[ch].midi_note=ev.note;
                    pending[ch].instId=ch_state[ch].instrument; pending[ch].volume=ch_state[ch].volume;
                }
            }
            for(int sample_in_row=0; sample_in_row<samples_per_row; ++sample_in_row) {
                if(sample_in_row == KEY_OFF_GAP_SAMPLES) {
                    for(int ch=0;ch<MAX_CHANNELS;ch++) {
                        if(!pending[ch].has_note) continue;
                        int instId=pending[ch].instId;
                        if(patches_present[instId]) {
                            YM2612Patch p=apply_volume(patches[instId],pending[ch].volume);
                            chip.load_patch(p,ch);
                        }
                        chip.set_frequency(ch,IngameFMChip::midi_to_hz(pending[ch].midi_note),0);
                        chip.key_on(ch);
                        ch_state[ch].active=true; pending[ch].has_note=false;
                    }
                }
                int16_t frame[2];
                chip.generate(frame, 1);
                int out_idx = (rowIdx * samples_per_row + sample_in_row) * 2;
                result.samples[out_idx] = frame[0];
                result.samples[out_idx+1] = frame[1];
            }
        }
        return result;
    }

    static CachedSfx prerender_sfx(const IngameFMSong& song, int tick_rate, int speed,
                                   const std::array<YM2612Patch,256>& patches,
                                   const std::array<bool,256>& patches_present,
                                   int sample_rate) { // Added arg
        CachedSfx result;
        if(song.rows.empty()) return result;
        // UPDATED: Uses passed sample_rate
        int samples_per_row = static_cast<int>(static_cast<double>(sample_rate)/tick_rate*speed);
        int total_samples = song.num_rows * samples_per_row;
        result.samples.resize(total_samples * 2); 
        result.samples_per_row = samples_per_row;
        result.total_rows = song.num_rows;
        result.valid = true;

        IngameFMChip chip;
        for(int c=0;c<3;++c) { chip.write(0,0xB4+c,0xC0); chip.write(1,0xB4+c,0xC0); }
        
        int current_instrument = 0;
        int current_volume = 0x7F;
        bool pending_has_note = false;
        bool pending_is_off = false;
        int pending_note = 0;
        int pending_inst = 0;
        int pending_vol = 0x7F;

        for(int rowIdx=0; rowIdx<song.num_rows; ++rowIdx) {
            int total = song.num_rows;
            if (total > 0 && (rowIdx % (total / 10 + 1)) == 0) { 
                int percent = (rowIdx * 100) / total;
                if (percent % 10 == 0) std::printf("[IngameFM] Pre-rendering SFX: %d%% complete\n", percent);
            }
            const IngameFMRow& row = song.rows[rowIdx];
            if(!row.channels.empty()) {
                const IngameFMEvent& ev = row.channels[0];
                if(ev.instrument>=0) current_instrument=ev.instrument;
                if(ev.volume>=0)     current_volume=ev.volume;
                if(ev.note==NOTE_OFF) {
                    chip.key_off(0);
                    pending_is_off=true; pending_has_note=false;
                } else if(ev.note>=0) {
                    chip.key_off(0);
                    pending_has_note=true; pending_note=ev.note;
                    pending_inst=current_instrument; pending_vol=current_volume;
                    pending_is_off=false;
                }
            }
            for(int sample_in_row=0; sample_in_row<samples_per_row; ++sample_in_row) {
                if(sample_in_row == KEY_OFF_GAP_SAMPLES) {
                    if(pending_has_note) {
                        if(patches_present[pending_inst]) {
                            YM2612Patch p=apply_volume(patches[pending_inst],pending_vol);
                            chip.load_patch(p,0);
                        }
                        chip.set_frequency(0,IngameFMChip::midi_to_hz(pending_note),0);
                        chip.key_on(0);
                        pending_has_note=false;
                    }
                }
                int16_t frame[2];
                chip.generate(frame, 1);
                int out_idx = (rowIdx * samples_per_row + sample_in_row) * 2;
                result.samples[out_idx] = frame[0];
                result.samples[out_idx+1] = frame[1];
            }
        }
        return result;
    }

    static YM2612Patch apply_volume(const YM2612Patch& src, int vol) {
        YM2612Patch p=src;
        int tl_add=((0x7F-vol)*127)/0x7F;
        bool isCarrier[4]={false,false,false,false};
        switch(p.ALG) {
            case 0:case 1:case 2:case 3: isCarrier[3]=true; break;
            case 4: isCarrier[1]=isCarrier[3]=true; break;
            case 5:case 6: isCarrier[1]=isCarrier[2]=isCarrier[3]=true; break;
            case 7: isCarrier[0]=isCarrier[1]=isCarrier[2]=isCarrier[3]=true; break;
        }
        for(int op=0;op<4;op++) if(isCarrier[op]) p.op[op].TL=std::min(127,p.op[op].TL+tl_add);
        return p;
    }

    void apply_pending_song() {
        PendingSong& ps = pending_song_;
        for(int ch=0;ch<MAX_CHANNELS;ch++) ym_music_->key_off(ch);
        for(auto& c:ch_state_) c=IngameFMChannelState{};
        for(auto& p:pending_)  p={};
        
        song_            = std::move(ps.song);
        tick_rate_       = ps.tick_rate;
        speed_           = ps.speed;
        samples_per_row_ = ps.samples_per_row;
        current_row_.store(ps.start_row);
        sample_in_row_   = 0;
        finished_.store(false);
        ps.pending       = false;
        current_song_cache_key_id_ = ps.cache_key_id;
        current_song_id_ = -1;

        process_row(current_row_.load());
        commit_keyon();
        sample_in_row_   = KEY_OFF_GAP_SAMPLES;
    }

    void process_row(int rowIdx) {
        if(rowIdx>=(int)song_.rows.size()) return;
        const IngameFMRow& row=song_.rows[rowIdx];
        int numCh=std::min((int)row.channels.size(),MAX_CHANNELS);
        for(int ch=0;ch<numCh;ch++) {
            const IngameFMEvent& ev=row.channels[ch];
            pending_[ch]={};
            if(ev.instrument>=0) ch_state_[ch].instrument=ev.instrument;
            if(ev.volume>=0)     ch_state_[ch].volume=ev.volume;
            if(ev.note==NOTE_OFF) {
                ym_music_->key_off(ch); ch_state_[ch].active=false; pending_[ch].is_off=true;
            } else if(ev.note>=0) {
                ym_music_->key_off(ch); ch_state_[ch].active=false;
                pending_[ch].has_note=true; pending_[ch].midi_note=ev.note;
                pending_[ch].instId=ch_state_[ch].instrument; pending_[ch].volume=ch_state_[ch].volume;
            }
        }
    }

    void commit_keyon() {
        for(int ch=0;ch<MAX_CHANNELS;ch++) {
            if(!pending_[ch].has_note) continue;
            int instId=pending_[ch].instId;
            if(patches_present_[instId]) {
                YM2612Patch p=apply_volume(patches_[instId],pending_[ch].volume);
                ym_music_->load_patch(p,ch);
            }
            ym_music_->set_frequency(ch,IngameFMChip::midi_to_hz(pending_[ch].midi_note),0);
            ym_music_->key_on(ch);
            ch_state_[ch].active=true; pending_[ch].has_note=false;
        }
    }

    void sfx_process_row(int v, int rowIdx, bool skip_keyoff=false) {
        SfxVoiceState& vs=sfx_voice_[v];
        const SfxDef& def=sfx_defs_[vs.sfx_id];
        vs.pending_has_note=false; vs.pending_is_off=false;
        if(rowIdx>=(int)def.song.rows.size()) return;
        const IngameFMEvent& ev=def.song.rows[rowIdx].channels[0];
        if(ev.instrument>=0) vs.last_instrument=ev.instrument;
        if(ev.volume>=0)     vs.last_volume=ev.volume;
        if(ev.note==NOTE_OFF)  { if(!skip_keyoff) ym_sfx_->key_off(v); vs.pending_is_off=true; }
        else if(ev.note>=0)    {
            if(!skip_keyoff) ym_sfx_->key_off(v);
            vs.pending_has_note=true; vs.pending_note=ev.note;
            vs.pending_inst=vs.last_instrument; vs.pending_vol=vs.last_volume;
        }
    }

    void sfx_commit_keyon(int v) {
        SfxVoiceState& vs=sfx_voice_[v];
        if(!vs.pending_has_note) return;
        if(patches_present_[vs.pending_inst]) {
            YM2612Patch p=apply_volume(patches_[vs.pending_inst],vs.pending_vol);
            ym_sfx_->load_patch(p,v);
        }
        ym_sfx_->set_frequency(v,IngameFMChip::midi_to_hz(vs.pending_note),0);
        ym_sfx_->key_on(v);
        vs.pending_has_note=false;
    }

    void sfx_tick_voice(int v, int samples) {
        SfxVoiceState& vs=sfx_voice_[v];
        if(!vs.active()) return;
        const SfxDef& def=sfx_defs_[vs.sfx_id];
        int remaining=samples;
        while(remaining>0) {
            bool in_gap=vs.sample_in_row<KEY_OFF_GAP_SAMPLES;
            int next_bound=in_gap?KEY_OFF_GAP_SAMPLES:vs.samples_per_row;
            int to_advance=std::min(remaining,next_bound-vs.sample_in_row);
            vs.sample_in_row+=to_advance; remaining-=to_advance;
            if(in_gap&&vs.sample_in_row>=KEY_OFF_GAP_SAMPLES) sfx_commit_keyon(v);
            if(vs.sample_in_row>=vs.samples_per_row) {
                vs.sample_in_row=0; vs.ticks_remaining--;
                if(vs.ticks_remaining<=0) { ym_sfx_->key_off(v); vs=SfxVoiceState{}; return; }
                vs.current_row++;
                if(vs.current_row>=(int)def.song.rows.size()) vs.current_row=0;
                sfx_process_row(v,vs.current_row);
            }
        }
    }

    void audio_callback(int16_t* stream, int samples) {
        if(finished_.load()) { std::memset(stream,0,samples*4); return; }
        
        if(pending_song_.pending && pending_song_.when==SongChangeWhen::NOW)
            apply_pending_song();

        int remaining=samples;
        int16_t* out=stream;
        while(remaining>0) {
            int pos_in_row=sample_in_row_;
            bool in_gap=pos_in_row<KEY_OFF_GAP_SAMPLES;
            int next_boundary=in_gap?KEY_OFF_GAP_SAMPLES:samples_per_row_;
            int to_generate=std::min(remaining,next_boundary-pos_in_row);

            const float mv=music_vol_.load();
            const int   n = to_generate*2;
            bool used_cached_music = false;
            
            if(use_cache_ && current_song_cache_key_id_ >= 0) {
                auto it = song_cache_.find(current_song_cache_key_id_);
                if(it == song_cache_.end() && defined_songs_.count(current_song_cache_key_id_)) {
                    const DefinedSong& def = defined_songs_[current_song_cache_key_id_];
                    std::printf("[IngameFM] Cache Miss (Music): Generating on-demand...\n");
                    // UPDATED: Pass sample_rate_
                    song_cache_[current_song_cache_key_id_] = prerender_song(def.song, def.tick_rate, def.speed, patches_, patches_present_, sample_rate_);
                    it = song_cache_.find(current_song_cache_key_id_);
                }

                if(it != song_cache_.end() && it->second.valid) {
                    const CachedSong& cached = it->second;
                    int start_offset = (current_row_.load() * cached.samples_per_row + pos_in_row) * 2;
                    if(start_offset + n <= static_cast<int>(cached.samples.size())) {
                        std::memcpy(out, &cached.samples[start_offset], n * sizeof(int16_t));
                        if(mv < 1.0f) {
                            for(int i=0;i<n;++i) out[i]=static_cast<int16_t>(static_cast<float>(out[i])*mv);
                        }
                        used_cached_music = true;
                    }
                }
            }
            
            if(!used_cached_music) {
                if(ym_music_) {
                    ym_music_->generate(out, to_generate);
                    if(mv < 1.0f) {
                        for(int i=0;i<n;++i) out[i]=static_cast<int16_t>(out[i]*mv);
                    }
                } else {
                    std::memset(out, 0, n * sizeof(int16_t));
                }
            }

            const float sv = sfx_vol_.load();
            bool any_sfx = false;
            for(int v=0; v<sfx_voices_; ++v) if(sfx_voice_[v].active()){any_sfx=true;break;}
            
            if(any_sfx && sv > 0.0f) {
                if(use_cache_) {
                    for(int i=0; i<n; ++i) {
                        float mixed = static_cast<float>(out[i]);
                        for(int v=0; v<sfx_voices_; ++v) {
                            if(!sfx_voice_[v].active()) continue;
                            const SfxVoiceState& vs = sfx_voice_[v];
                            if(vs.cache_key_id >= 0) {
                                auto it = sfx_cache_.find(vs.cache_key_id);
                                if(it == sfx_cache_.end() && sfx_defs_present_[vs.sfx_id]) {
                                    const SfxDef& def = sfx_defs_[vs.sfx_id];
                                    std::printf("[IngameFM] Cache Miss (SFX): Generating on-demand...\n");
                                    // UPDATED: Pass sample_rate_
                                    sfx_cache_[vs.cache_key_id] = prerender_sfx(def.song, def.tick_rate, def.speed, patches_, patches_present_, sample_rate_);
                                    it = sfx_cache_.find(vs.cache_key_id);
                                }

                                if(it != sfx_cache_.end() && it->second.valid) {
                                    const CachedSfx& cached = it->second;
                                    int sfx_offset = (vs.current_row * cached.samples_per_row + vs.sample_in_row) * 2;
                                    int absolute_sfx_sample = sfx_offset + i;
                                    if(absolute_sfx_sample >= 0 && absolute_sfx_sample < static_cast<int>(cached.samples.size())) {
                                        mixed += static_cast<float>(cached.samples[absolute_sfx_sample]) * sv;
                                    }
                                }
                            }
                        }
                        out[i] = static_cast<int16_t>(std::max(-32768.f, std::min(32767.f, mixed)));
                    }
                } else {
                    if(ym_sfx_) {
                        int16_t sfx_out[2048];
                        if(to_generate * 2 > 2048) { 
                             std::vector<int16_t> dynamic_buf(to_generate * 2);
                             ym_sfx_->generate(dynamic_buf.data(), to_generate);
                             for(int i=0; i<n; ++i) {
                                 float mixed = static_cast<float>(out[i]) + static_cast<float>(dynamic_buf[i]) * sv;
                                 out[i] = static_cast<int16_t>(std::max(-32768.f, std::min(32767.f, mixed)));
                             }
                        } else {
                            ym_sfx_->generate(sfx_out, to_generate);
                            for(int i=0; i<n; ++i) {
                                float mixed = static_cast<float>(out[i]) + static_cast<float>(sfx_out[i]) * sv;
                                out[i] = static_cast<int16_t>(std::max(-32768.f, std::min(32767.f, mixed)));
                            }
                        }
                    }
                }
            } else {
                if(!use_cache_ && ym_sfx_) {
                    int16_t dummy[2048];
                    if(to_generate * 2 <= 2048) {
                        ym_sfx_->generate(dummy, to_generate);
                    }
                }
            }

            for(int v=0; v<sfx_voices_; ++v) sfx_tick_voice(v, to_generate);
            
            out += to_generate*2; 
            remaining -= to_generate; 
            sample_in_row_ += to_generate;
            
            if(in_gap && sample_in_row_ >= KEY_OFF_GAP_SAMPLES) commit_keyon();
            
            if(sample_in_row_ >= samples_per_row_) {
                sample_in_row_ = 0;
                int row = current_row_.load() + 1;
                if(row >= (int)song_.rows.size()) {
                    if(pending_song_.pending && pending_song_.when == SongChangeWhen::AT_PATTERN_END) {
                        apply_pending_song();
                        break;
                    }
                    if(loop_) { for(auto& p:pending_) p={}; row=0; }
                    else {
                        std::memset(out, 0, remaining*4);
                        if(ym_music_) for(int ch=0; ch<MAX_CHANNELS; ch++) ym_music_->key_off(ch);
                        finished_.store(true); 
                        return;
                    }
                }
                current_row_.store(row);
                process_row(row);
            }
        }
    }

    void _write_cache_file(const std::string& filename, const std::vector<int16_t>& samples, int spr, int rows, char type) {
        FILE* f = std::fopen(filename.c_str(), "wb");
        if(!f) {
            std::perror(("Failed to open " + filename + " for writing").c_str());
            return;
        }
        char magic[5] = {0};
        magic[0] = 'I'; magic[1] = 'F'; magic[2] = type; magic[3] = 'C';

        uint32_t u_spr = static_cast<uint32_t>(spr);
        uint32_t u_rows = static_cast<uint32_t>(rows);
        uint32_t u_count = static_cast<uint32_t>(samples.size());

        std::fwrite(magic, 1, 4, f);
        std::fwrite(&u_spr, sizeof(uint32_t), 1, f);
        std::fwrite(&u_rows, sizeof(uint32_t), 1, f);
        std::fwrite(&u_count, sizeof(uint32_t), 1, f);
        if(!samples.empty()) {
            std::fwrite(samples.data(), sizeof(int16_t), samples.size(), f);
        }
        std::fclose(f);
        std::printf("[IngameFM] Dumped %c cache to %s (%d samples)\n", type, filename.c_str(), u_count);
    }

    template<typename T>
    T _parse_cache_data(const uint8_t* data, size_t len, char expectedType) {
        T result;
        result.valid = false;

        if(len < 16) return result;

        if(data[0] != 'I' || data[1] != 'F' || data[2] != expectedType || data[3] != 'C') {
            std::fprintf(stderr, "[IngameFM] Invalid cache magic header. Expected 'IF%cC'.\n", expectedType);
            return result;
        }

        uint32_t spr, rows, count;
        std::memcpy(&spr, data + 4, sizeof(uint32_t));
        std::memcpy(&rows, data + 8, sizeof(uint32_t));
        std::memcpy(&count, data + 12, sizeof(uint32_t));

        size_t expectedBytes = 16 + (count * sizeof(int16_t));
        if(len < expectedBytes) {
            std::fprintf(stderr, "[IngameFM] Data truncated. Expected %zu bytes, got %zu.\n", expectedBytes, len);
            return result;
        }

        result.samples_per_row = static_cast<int>(spr);
        result.total_rows = static_cast<int>(rows);
        result.samples.resize(count);
        std::memcpy(result.samples.data(), data + 16, count * sizeof(int16_t));
        result.valid = true;

        return result;
    }
};