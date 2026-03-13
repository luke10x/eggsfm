// =============================================================================
// demo4.cpp — ingamefm YM2612 patch editor
//
// Aurora background + ImGui patch editor panel.
//
// Audio:
//   Channel 0 — long pad: C4, E4, A4, F4  (instrument 0x00)
//   Channel 1 — short staccato: same notes, offset rhythm  (instrument 0x00)
//   Both channels always use the instrument currently selected in the editor.
//   Selecting a different instrument or moving any slider takes effect live.
//
// Editor:
//   Dropdown — selects which catalogue patch is loaded as instrument 0x00
//   Global row — ALG, FB, AMS, FMS sliders
//   4 operator columns — all 11 params per operator
//   FPS counter shown inside the window
//
// No interactive keyboard/mouse notes. Esc to quit.
//
// Build commands at bottom of file.
// =============================================================================

// =============================================================================
// 1. INCLUDES
// =============================================================================

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#  include <emscripten/html5.h>
#  include <GLES3/gl3.h>
#else
#  include <SDL_opengles2.h>
#endif

#include <SDL.h>

#include "imgui.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"

#include <cstdio>
#include <cmath>
#include <cstring>

#include "ingamefm.h"

// =============================================================================
// 2. GLSL
// =============================================================================

#define GLSL_VERSION "#version 300 es\n"

// =============================================================================
// 3. AURORA SHADERS
// =============================================================================

static const char* AURORA_VERT =
    GLSL_VERSION
    R"(
    precision highp float;
    layout(location = 0) in vec3 aPos;
    layout(location = 1) in vec2 aTexCoord;
    out vec2 TexCoord;
    void main() {
        TexCoord    = aTexCoord;
        gl_Position = vec4(aPos, 1.0);
    }
    )";

static const char* AURORA_FRAG =
    GLSL_VERSION
    R"(
    precision highp float;
    in  vec2 TexCoord;
    out vec4 FragColor;
    uniform float uYaw;
    uniform float uPitch;
    uniform float uTime;

    vec3 mod289v3(vec3 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
    vec2 mod289v2(vec2 x) { return x - floor(x * (1.0/289.0)) * 289.0; }
    vec3 permute3(vec3 x) { return mod289v3(((x * 34.0) + 1.0) * x); }
    float snoise(vec2 v) {
        const vec4 C = vec4(0.211324865405187,  0.366025403784439,
                           -0.577350269189626,  0.024390243902439);
        vec2 i  = floor(v + dot(v, C.yy));
        vec2 x0 = v - i + dot(i, C.xx);
        vec2 i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
        vec4 x12 = x0.xyxy + C.xxzz;
        x12.xy -= i1;
        i = mod289v2(i);
        vec3 p = permute3(permute3(i.y + vec3(0.0,i1.y,1.0))
                          + i.x  + vec3(0.0,i1.x,1.0));
        vec3 m = max(0.5 - vec3(dot(x0,x0),
                                dot(x12.xy,x12.xy),
                                dot(x12.zw,x12.zw)), 0.0);
        m = m*m*m*m;
        vec3 x  = 2.0*fract(p*C.www) - 1.0;
        vec3 h  = abs(x) - 0.5;
        vec3 ox = floor(x + 0.5);
        vec3 a0 = x - ox;
        m *= 1.79284291400159 - 0.85373472095314*(a0*a0 + h*h);
        vec3 g;
        g.x  = a0.x *x0.x  + h.x *x0.y;
        g.yz = a0.yz*x12.xz + h.yz*x12.yw;
        return 130.0*dot(m,g);
    }
    void main() {
        float yawNorm     = uYaw / 3.14159;
        float yawOffset   = yawNorm * abs(yawNorm);
        float pitchNorm   = (uPitch + 1.5708) / 3.14159;
        float pitchOffset = (pitchNorm - 0.5) * 4.0;
        float timeOffset  = uTime * 0.0005;
        vec2 uv = TexCoord + vec2(yawOffset, pitchOffset + timeOffset);
        float n1 = snoise(uv * 3.0)  * 0.5;
        float n2 = snoise(uv * 7.0  + vec2(uTime*0.01, 0.0)) * 0.3;
        float n3 = snoise(uv * 15.0 + vec2(uTime*0.02, 0.0)) * 0.2;
        float intensity = clamp(n1+n2+n3, 0.0, 1.0);
        vec3 col1 = vec3(sin(TexCoord.x+TexCoord.y+uTime*0.001), 0.2, 0.3);
        vec3 col2 = vec3(0.9, sin(TexCoord.y+uTime*0.0005), 0.5);
        vec3 color = mix(col1, col2, intensity);
        FragColor = vec4(color, pitchNorm);
    }
    )";

// =============================================================================
// 4. SONG
//
// 16 rows, tick_rate=60, speed=12  →  200ms/row
//
// Ch0 (inst 00) — long pad, one note every 4 rows: C4, E4, A4, F4
// Ch1 (inst 00) — short staccato, same notes offset by 2 rows, OFF after 1 row
//
// Both channels use instrument 0x00 so editing a single patch affects both.
// Volume 7F throughout — volume scaling can be explored via TL sliders.
//
// Column format: note(3) inst(2) vol(2)  =  7 chars
// =============================================================================

static const char* SONG =
"org.tildearrow.furnace - Pattern Data (16)\n"
"16\n"
/* row  0 */ "C-4007F|.......|" "\n"   // pad: C4 on
/* row  1 */ ".......|.......|" "\n"
/* row  2 */ ".......|C-4007F|" "\n"   // staccato: C4 on
/* row  3 */ ".......|OFF....|" "\n"   //            C4 off
/* row  4 */ "E-4007F|.......|" "\n"   // pad: E4
/* row  5 */ ".......|.......|" "\n"
/* row  6 */ ".......|E-4007F|" "\n"   // staccato: E4
/* row  7 */ ".......|OFF....|" "\n"
/* row  8 */ "A-4007F|.......|" "\n"   // pad: A4
/* row  9 */ ".......|.......|" "\n"
/* row 10 */ ".......|A-4007F|" "\n"   // staccato: A4
/* row 11 */ ".......|OFF....|" "\n"
/* row 12 */ "F-4007F|.......|" "\n"   // pad: F4
/* row 13 */ ".......|.......|" "\n"
/* row 14 */ ".......|F-4007F|" "\n"   // staccato: F4
/* row 15 */ "OFF....|OFF....|" "\n";  // both off before loop

// =============================================================================
// 5. GL HELPERS
// =============================================================================

static void checkGLError(const char* where)
{
    GLenum err = glGetError();
    if (err != GL_NO_ERROR)
        fprintf(stderr, "GL error 0x%x at %s\n", err, where);
}

static GLuint compileShader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "Shader %s error:\n%s\n",
                type == GL_VERTEX_SHADER ? "vert" : "frag", log);
        exit(1);
    }
    return s;
}

static GLuint createProgram(const char* vert, const char* frag)
{
    GLuint vs = compileShader(GL_VERTEX_SHADER,   vert);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, frag);
    GLuint p  = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        fprintf(stderr, "Program link error:\n%s\n", log);
        exit(1);
    }
    return p;
}

// =============================================================================
// 6. AURORA RENDERER
// =============================================================================

struct AuroraRenderer
{
    GLuint program = 0;
    GLuint vao     = 0;
    float  time    = 3.0f;

    void init()
    {
        program = createProgram(AURORA_VERT, AURORA_FRAG);

        static const GLfloat verts[] = {
            -1.f,-1.f, 1.000f, 0.f,0.f,
             1.f,-1.f, 0.998f, 1.f,0.f,
            -1.f, 1.f, 0.998f, 0.f,1.f,
             1.f, 1.f, 0.998f, 1.f,1.f,
        };
        static const GLuint idx[] = {0,1,2, 1,3,2};

        GLuint vbo, ebo;
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              5*sizeof(GLfloat), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                              5*sizeof(GLfloat), (void*)(3*sizeof(GLfloat)));
        glBindVertexArray(0);
        checkGLError("AuroraRenderer::init");
    }

    void render(float dt)
    {
        time += dt;
        float yaw   = sinf(time * 0.07f) * 1.2f;
        float pitch = sinf(time * 0.04f) * 0.8f - 0.4f;
        glUseProgram(program);
        glUniform1f(glGetUniformLocation(program, "uTime"),  time);
        glUniform1f(glGetUniformLocation(program, "uYaw"),   yaw);
        glUniform1f(glGetUniformLocation(program, "uPitch"), pitch);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
};

// =============================================================================
// 7. IMGUI MODULE  — stock dark style, no customisation
// =============================================================================

struct ModImgui
{
    ImGuiContext* ctx = nullptr;

    void init(SDL_Window* window, SDL_GLContext glCtx)
    {
        IMGUI_CHECKVERSION();
        ctx = ImGui::CreateContext();
        ImGui::SetCurrentContext(ctx);
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();   // stock — nothing more
        ImGui_ImplSDL2_InitForOpenGL(window, glCtx);
        ImGui_ImplOpenGL3_Init("#version 300 es");
    }

    void shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext(ctx);
        ctx = nullptr;
    }

    void processEvent(const SDL_Event& e) const
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
    }

    void newFrame() const
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
    }

    void render() const
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }
};

// =============================================================================
// 8. PATCH EDITOR STATE
//
// Holds one mutable working copy per catalogue patch.
// selectedIdx  — which patch is currently loaded as instrument 0x00
// =============================================================================

static constexpr int NUM_PATCHES = PATCH_CATALOGUE_SIZE;

struct PatchEditorState
{
    YM2612Patch editPatches[NUM_PATCHES];
    int         selectedIdx = 0;

    void init()
    {
        for (int i = 0; i < NUM_PATCHES; i++)
            editPatches[i] = *PATCH_CATALOGUE[i].patch;
    }

    YM2612Patch& current() { return editPatches[selectedIdx]; }
};

// =============================================================================
// 9. APPLICATION STATE
// =============================================================================

struct AppState
{
    // Window / GL
    SDL_Window*   window = nullptr;
    SDL_GLContext glCtx  = nullptr;
    int           winW   = 1024;
    int           winH   = 640;

    // Subsystems
    AuroraRenderer   aurora;
    ModImgui         imgui;
    PatchEditorState editor;

    // Audio
    SDL_AudioDeviceID audioDev = 0;
    IngameFMPlayer    player;

    // Timing
    Uint32 lastTick  = 0;
    float  fpsSmooth = 0.0f;

    bool running = true;
};

static AppState g_app;

// =============================================================================
// 10. PATCH SYNC HELPERS
//
// pushPatch — writes editor's current patch into the player (instrument 0x00)
//             AND directly into the chip for channels 0 and 1 so the
//             currently sounding note changes without waiting for a retrigger.
//
// Because the player's commit_keyon() applies volume scaling on a copy, the
// direct chip write skips volume — good enough for a live editor preview.
// On the next note trigger the player re-reads patches_[0x00] and applies
// correct volume.  That's exactly what we want.
// =============================================================================

static void pushPatch(AppState& app)
{
    const YM2612Patch& p = app.editor.current();
    SDL_LockAudioDevice(app.audioDev);
    // Update the player's patch map so future note triggers use the new patch
    app.player.add_patch(0x00, p);
    // Apply to both song channels immediately so the live sound changes
    app.player.chip()->load_patch(p, 0);
    app.player.chip()->load_patch(p, 1);
    SDL_UnlockAudioDevice(app.audioDev);
}

// =============================================================================
// 11. IMGUI PATCH EDITOR
//
// Returns true if any parameter changed this frame.
// =============================================================================

static bool drawPatchEditor(AppState& app, float fps)
{
    bool changed = false;
    PatchEditorState& ed = app.editor;

    // Initial position / size — user can drag and resize freely after that
    ImGui::SetNextWindowPos( ImVec2(12.0f, 12.0f), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(700.0f, (float)app.winH - 24.0f),
                             ImGuiCond_Once);

    if (!ImGui::Begin("YM2612 Patch Editor"))
    {
        ImGui::End();
        return false;
    }

    // ── FPS ──────────────────────────────────────────────────────────────────
    ImGui::Text("%.0f FPS", fps);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Instrument dropdown ──────────────────────────────────────────────────
    ImGui::Text("Instrument");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

    if (ImGui::BeginCombo("##inst", PATCH_CATALOGUE[ed.selectedIdx].name))
    {
        for (int i = 0; i < NUM_PATCHES; i++)
        {
            bool selected = (i == ed.selectedIdx);
            if (ImGui::Selectable(PATCH_CATALOGUE[i].name, selected))
            {
                ed.selectedIdx = i;
                // pushPatch() will be called below since changed=true
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Global parameters ────────────────────────────────────────────────────
    YM2612Patch& p = ed.current();

    ImGui::Text("Global");
    ImGui::Spacing();

    // Layout: "LABEL  [====slider====]" repeated 4 times on one line.
    // We draw Text() then SameLine() then a ##-only slider so the label
    // sits visibly to the left of the track.
    // Each group is (labelW + sliderW) wide; sliderW is computed so all four
    // groups fit in the available width with ItemSpacing gaps between them.
    {
        float avail   = ImGui::GetContentRegionAvail().x;
        float sp      = ImGui::GetStyle().ItemSpacing.x;
        float labelW  = ImGui::CalcTextSize("FMS").x + sp; // widest label = "FMS"
        // 4 groups, 3 gaps between them
        float sliderW = (avail - (labelW * 4.0f) - sp * 3.0f) / 4.0f;
        if (sliderW < 30.0f) sliderW = 30.0f;

        ImGui::Text("ALG"); ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::SliderInt("##ALGg", &p.ALG, 0, 7)) changed = true;

        ImGui::SameLine();
        ImGui::Text("FB"); ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::SliderInt("##FBg",  &p.FB,  0, 7)) changed = true;

        ImGui::SameLine();
        ImGui::Text("AMS"); ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::SliderInt("##AMSg", &p.AMS, 0, 3)) changed = true;

        ImGui::SameLine();
        ImGui::Text("FMS"); ImGui::SameLine();
        ImGui::SetNextItemWidth(sliderW);
        if (ImGui::SliderInt("##FMSg", &p.FMS, 0, 7)) changed = true;
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // ── Operator columns ─────────────────────────────────────────────────────
    ImGui::Text("Operators");
    ImGui::Spacing();

    // Correct ImGui table sequencing with BordersInnerV (uses draw channel splitter):
    //   1. TableSetupColumn x4   — register columns, splitter not yet active
    //   2. TableHeadersRow       — ImGui internally handles row + channels
    //   3. TableNextRow          — first data row, one row holds all sliders
    //   4. TableSetColumnIndex   — switch column, place widgets vertically
    //   5. EndTable

    if (ImGui::BeginTable("##ops", 4,
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn("OP 1");
        ImGui::TableSetupColumn("OP 2");
        ImGui::TableSetupColumn("OP 3");
        ImGui::TableSetupColumn("OP 4");
        ImGui::TableHeadersRow();
        ImGui::TableNextRow();

        // For each operator: draw a Text label on the left, then SameLine(),
        // then SetNextItemWidth(-1) with a ##-only slider ID.
        // This keeps the label visible inside the column — if the label is
        // embedded in the slider ID (e.g. "DT##0"), ImGui renders it to the
        // RIGHT of the track and it gets clipped by the column boundary.
        // Using a separate Text widget puts it on the LEFT where there is room.

        for (int op = 0; op < 4; op++)
        {
            ImGui::TableSetColumnIndex(op);
            YM2612Operator& o = p.op[op];
            char s[16];

// Macro: visible text label left, slider fills rest of column, unique ID per op.
#define OPS(lbl, fld, lo, hi) \
    ImGui::Text(lbl); ImGui::SameLine(); \
    ImGui::SetNextItemWidth(-1); \
    snprintf(s, sizeof(s), "##%s%d", lbl, op); \
    if (ImGui::SliderInt(s, &o.fld, lo, hi)) changed = true;

            OPS("DT",  DT,  -3,   3)
            OPS("MUL", MUL,  0,  15)
            OPS("TL",  TL,   0, 127)
            OPS("RS",  RS,   0,   3)
            OPS("AR",  AR,   0,  31)
            OPS("AM",  AM,   0,   1)
            OPS("DR",  DR,   0,  31)
            OPS("SR",  SR,   0,  31)
            OPS("SL",  SL,   0,  15)
            OPS("RR",  RR,   0,  15)
            OPS("SSG", SSG,  0,   8)
#undef OPS
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Ch0: long pad  |  Ch1: staccato  |  both use instrument 0x00");

    ImGui::End();
    return changed;
}

// =============================================================================
// 12. PER-FRAME TICK
// =============================================================================

static void mainTick()
{
    AppState& app = g_app;

    // ── Timing ───────────────────────────────────────────────────────────────
    Uint32 now = SDL_GetTicks();
    float  dt  = (app.lastTick == 0) ? 0.016f
                                     : (now - app.lastTick) * 0.001f;
    app.lastTick = now;
    if (dt > 0.1f) dt = 0.1f;

    float fps    = (dt > 0.0001f) ? (1.0f / dt) : 9999.0f;
    app.fpsSmooth = (app.fpsSmooth < 1.0f)
                  ? fps
                  : app.fpsSmooth * 0.9f + fps * 0.1f;

    // ── Events ───────────────────────────────────────────────────────────────
    SDL_Event e;
    while (SDL_PollEvent(&e))
    {
        app.imgui.processEvent(e);

        if (e.type == SDL_QUIT)
        {
            app.running = false;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        }

        if (e.type == SDL_KEYDOWN && !e.key.repeat &&
            e.key.keysym.sym == SDLK_ESCAPE)
        {
            app.running = false;
#ifdef __EMSCRIPTEN__
            emscripten_cancel_main_loop();
#endif
        }

        if (e.type == SDL_WINDOWEVENT &&
            e.window.event == SDL_WINDOWEVENT_RESIZED)
        {
            app.winW = e.window.data1;
            app.winH = e.window.data2;
            glViewport(0, 0, app.winW, app.winH);
        }
    }

    // ── Aurora ───────────────────────────────────────────────────────────────
    glClearColor(0.0f, 0.0f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    app.aurora.render(dt);

    // ── ImGui ────────────────────────────────────────────────────────────────
    app.imgui.newFrame();
    bool changed = drawPatchEditor(app, app.fpsSmooth);
    app.imgui.render();

    // Push patch to player + chip immediately if anything changed
    if (changed)
        pushPatch(app);

    SDL_GL_SwapWindow(app.window);
}

// =============================================================================
// 13. INIT
// =============================================================================

static bool initVideo(AppState& app)
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_SetHint(SDL_HINT_VIDEO_HIGHDPI_DISABLED, "1");

    app.window = SDL_CreateWindow(
        "ingamefm patch editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        app.winW, app.winH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN
    );
    if (!app.window) { fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }

    app.glCtx = SDL_GL_CreateContext(app.window);
    if (!app.glCtx) { fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError()); return false; }

    if (SDL_GL_SetSwapInterval(-1) != 0)
        if (SDL_GL_SetSwapInterval(1) != 0)
            SDL_GL_SetSwapInterval(0);

    glViewport(0, 0, app.winW, app.winH);
    return true;
}

static bool initAudio(AppState& app)
{
    try {
        app.player.set_song(SONG, /*tick_rate=*/60, /*speed=*/12);
        // Instrument 0x00 gets the first catalogue patch; can be changed at runtime
        app.player.add_patch(0x00, app.editor.current());
    } catch (const std::exception& ex) {
        fprintf(stderr, "Song parse error: %s\n", ex.what());
        return false;
    }

    SDL_AudioSpec desired{};
    desired.freq     = IngameFMPlayer::SAMPLE_RATE;
    desired.format   = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples  = 512;
    desired.callback = IngameFMPlayer::s_audio_callback;
    desired.userdata = &app.player;

    SDL_AudioSpec obtained{};
    app.audioDev = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (app.audioDev == 0) { fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError()); return false; }

    app.player.start(app.audioDev, /*loop=*/true);
    SDL_PauseAudioDevice(app.audioDev, 0);
    return true;
}

// =============================================================================
// 14. MAIN
// =============================================================================

int main(int /*argc*/, char** /*argv*/)
{
    AppState& app = g_app;

    if (!initVideo(app)) return 1;

    app.editor.init();   // must be before initAudio (uses editor.current())

    if (!initAudio(app)) return 1;

    app.aurora.init();
    app.imgui.init(app.window, app.glCtx);

    printf("=== ingamefm patch editor ===\n");
    printf("Looping C-E-A-F pad + staccato.  Edit sliders to change sound live.\n");
    printf("Esc to quit.\n\n");

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(mainTick, 0, 1);
#else
    while (app.running)
        mainTick();

    SDL_CloseAudioDevice(app.audioDev);
    app.imgui.shutdown();
    SDL_GL_DeleteContext(app.glCtx);
    SDL_DestroyWindow(app.window);
    SDL_Quit();
#endif

    return 0;
}

// =============================================================================
// BUILD COMMANDS
// =============================================================================
//
// Assumes imgui/ is a sibling directory containing imgui.h, imgui.cpp,
// imgui_draw.cpp, imgui_tables.cpp, imgui_widgets.cpp, and backends/.
//
// ── NATIVE (macOS) ───────────────────────────────────────────────────────────
//
//   IMGUI=../imgui
//   YMFM=../my-ym2612-plugin/build/_deps/ymfm-src/src
//   SDL_INC=../bowling/3rdparty/SDL/include
//   SDL_LIB=../bowling/build/macos/usr/lib/libSDL2.a
//
//   g++ -std=c++17 -O2 \
//       -I../bowling/build/macos/sdl2/include/ -I$SDL_INC \
//       -I$YMFM -I$IMGUI \
//       $SDL_LIB \
//       $YMFM/ymfm_misc.cpp $YMFM/ymfm_adpcm.cpp \
//       $YMFM/ymfm_ssg.cpp  $YMFM/ymfm_opn.cpp \
//       $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp \
//       $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
//       $IMGUI/backends/imgui_impl_sdl2.cpp \
//       $IMGUI/backends/imgui_impl_opengl3.cpp \
//       demo4.cpp \
//       -framework Cocoa -framework IOKit -framework CoreVideo \
//       -framework CoreAudio -framework AudioToolbox \
//       -framework ForceFeedback -framework Carbon \
//       -framework Metal -framework GameController -framework CoreHaptics \
//       -lobjc -o demo4 && ./demo4
//
// ── EMSCRIPTEN ───────────────────────────────────────────────────────────────
//
//   IMGUI=../imgui
//   YMFM=../my-ym2612-plugin/build/_deps/ymfm-src/src
//
//   em++ -std=c++17 -O2 \
//       -I$YMFM -I$IMGUI \
//       $YMFM/ymfm_misc.cpp $YMFM/ymfm_adpcm.cpp \
//       $YMFM/ymfm_ssg.cpp  $YMFM/ymfm_opn.cpp \
//       $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp \
//       $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp \
//       $IMGUI/backends/imgui_impl_sdl2.cpp \
//       $IMGUI/backends/imgui_impl_opengl3.cpp \
//       demo4.cpp \
//       -s USE_SDL=2 \
//       -s FULL_ES3=1 \
//       -s MIN_WEBGL_VERSION=2 \
//       -s MAX_WEBGL_VERSION=2 \
//       -s ALLOW_MEMORY_GROWTH=1 \
//       -s ASYNCIFY \
//       --shell-file shell4.html \
//       -o demo4.html
//
//   python3 -m http.server  →  http://localhost:8000/demo4.html
// =============================================================================
