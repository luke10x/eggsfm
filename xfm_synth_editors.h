#pragma once
// =============================================================================
// xfm_synth_editors.h — Multi-chip FM Synthesizer Editors
// =============================================================================

#include "imgui.h"
#include "xfm_api.h"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace synth_edit {

// =============================================================================
// Helper drawing functions
// =============================================================================

inline void drawWaveform(ImDrawList* dl, ImVec2 pos, ImVec2 size, int waveType, ImU32 color) {
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(20, 20, 30, 180));
    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(60, 60, 80, 200), 3.f);
    
    float cx = pos.x + 5.f, cy = pos.y + size.y * 0.5f;
    float w = size.x - 10.f, h = size.y * 0.35f;
    dl->AddLine(ImVec2(pos.x, cy), ImVec2(pos.x + size.x, cy), IM_COL32(40, 40, 60, 150), 1.f);
    
    switch (waveType) {
    case 0: { ImVec2 pts[100];
        for (int i = 0; i < 100; i++) { float t = (float)i / 99.f * 3.14159f * 2.f; pts[i] = ImVec2(cx + (w / 99.f) * i, cy - sinf(t) * h); }
        dl->AddPolyline(pts, 100, color, 0, 2.f); } break;
    case 1: { ImVec2 pts[100];
        for (int i = 0; i < 100; i++) { float t = (float)i / 99.f * 3.14159f; pts[i] = ImVec2(cx + (w / 99.f) * i, cy - (i < 50 ? sinf(t) * h : 0.f)); }
        dl->AddPolyline(pts, 100, color, 0, 2.f); } break;
    case 2: { ImVec2 pts[] = { ImVec2(cx, cy-h), ImVec2(cx+w*0.25f, cy-h), ImVec2(cx+w*0.25f, cy+h), ImVec2(cx+w*0.75f, cy+h), ImVec2(cx+w*0.75f, cy-h), ImVec2(cx+w, cy-h) };
        dl->AddPolyline(pts, 6, color, 0, 2.f); } break;
    case 3: { ImVec2 pts[] = { ImVec2(cx, cy+h), ImVec2(cx+w*0.25f, cy-h), ImVec2(cx+w*0.5f, cy+h), ImVec2(cx+w*0.75f, cy-h), ImVec2(cx+w, cy+h) };
        dl->AddPolyline(pts, 5, color, 0, 2.f); } break;
    case 4: { ImVec2 pts[] = { ImVec2(cx, cy+h), ImVec2(cx+w, cy-h), ImVec2(cx+w, cy+h) };
        dl->AddPolyline(pts, 3, color, 0, 2.f); } break;
    default: { ImVec2 pts[100];
        for (int i = 0; i < 100; i++) { float t = (float)i / 99.f * 3.14159f * 2.f; pts[i] = ImVec2(cx + (w / 99.f) * i, cy - sinf(t) * h); }
        dl->AddPolyline(pts, 100, color, 0, 2.f); }
    }
}

inline void drawAlgoIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz, int algo, bool isOPL = false, bool is4op = false) {
    float bx = p0.x, by = p0.y, uw = sz.x, uh = sz.y;
    const float opSz = 10.0f;
    const ImU32 lineCol = IM_COL32(180, 180, 180, 200), outCol = IM_COL32(100, 255, 160, 255);
    
    auto ln = [&](float x1, float y1, float x2, float y2) { dl->AddLine(ImVec2(x1,y1), ImVec2(x2,y2), lineCol, 1.5f); };
    auto out = [&](float x1, float y1, float x2, float) { dl->AddLine(ImVec2(x1,y1), ImVec2(x2,y1), outCol, 1.5f); dl->AddCircleFilled(ImVec2(x2,y1), 3.5f, outCol); };
    auto drawOp = [&](float cx, float cy, int num, bool first = false) {
        ImU32 col = (num==1)?IM_COL32(100,200,255,255):(num==2)?IM_COL32(120,255,140,255):(num==3)?IM_COL32(255,210,80,255):IM_COL32(255,110,110,255);
        dl->AddRectFilled(ImVec2(cx-opSz,cy-opSz), ImVec2(cx+opSz,cy+opSz), IM_COL32(30,30,40,220));
        dl->AddRect(ImVec2(cx-opSz,cy-opSz), ImVec2(cx+opSz,cy+opSz), first?IM_COL32(255,255,255,180):col, 2.f, 0, 1.5f);
        char buf[2] = {(char)('0'+num), 0};
        ImVec2 ts = ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(cx-ts.x*0.5f, cy-ts.y*0.5f), col, buf);
    };
    
    if (isOPL) {
        if (is4op) {
            // OPL3 4-op algorithms (similar to OPN but with 4 ops)
            switch (algo) {
            case 0: { float sp=uw/4.8f,y=by+uh/2,x1=bx+sp*0.5f,x2=bx+sp*1.5f,x3=bx+sp*2.5f,x4=bx+sp*3.5f,x5=bx+sp*4.2f;
                ln(x1+opSz,y,x2-opSz,y); ln(x2+opSz,y,x3-opSz,y); ln(x3+opSz,y,x4-opSz,y); out(x4+opSz,y,x5+opSz,y);
                drawOp(x1,y,1,true); drawOp(x2,y,2); drawOp(x3,y,3); drawOp(x4,y,4); } break;
            case 1: { float x1=bx+uw*0.25f,x2=bx+uw*0.5f,x3=bx+uw*0.75f,y1=by+uh*0.25f,y2=by+uh*0.5f,y3=by+uh*0.75f;
                ln(x1+opSz,y2,x2-opSz,y1); out(x2+opSz,y1,x3+opSz,y2); ln(x1+opSz,y2,x2-opSz,y2); out(x2+opSz,y2,x3+opSz,y2); ln(x1+opSz,y2,x2-opSz,y3); out(x2+opSz,y3,x3+opSz,y2);
                drawOp(x1,y2,1,true); drawOp(x2,y1,2); drawOp(x2,y2,3); drawOp(x2,y3,4); } break;
            default: { float x1=bx+uw*0.3f, x2=bx+uw*0.7f, y=by+uh*0.5f;
                ln(x1+opSz,y,x2-opSz,y); out(x2+opSz,y,bx+uw*0.9f,y);
                drawOp(x1,y,1,true); drawOp(x2,y,2); } break;
            }
        } else {
            // OPL 2-op algorithms
            switch (algo) {
            case 0: { // OP1 -> OP2 -> Out
                float x1=bx+uw*0.3f, x2=bx+uw*0.7f, y=by+uh*0.5f;
                ln(x1+opSz,y,x2-opSz,y); out(x2+opSz,y,bx+uw*0.9f,y);
                drawOp(x1,y,1,true); drawOp(x2,y,2); } break;
            case 1: { // OP1 + OP2 -> Out (parallel)
                float x1=bx+uw*0.3f, x2=bx+uw*0.7f, y1=by+uh*0.35f, y2=by+uh*0.65f, yout=by+uh*0.5f;
                ln(x1+opSz,y1,x2-opSz,yout); ln(x1+opSz,y2,x2-opSz,yout); out(x2+opSz,yout,bx+uw*0.9f,yout);
                drawOp(x1,y1,1,true); drawOp(x1,y2,2); } break;
            }
        }
    } else {
        // OPN/OPM algorithms (4 operators)
        switch (algo) {
        case 0: { float sp=uw/4.8f,y=by+uh/2,x1=bx+sp*0.5f,x2=bx+sp*1.5f,x3=bx+sp*2.5f,x4=bx+sp*3.5f,x5=bx+sp*4.2f;
            ln(x1+opSz,y,x2-opSz,y); ln(x2+opSz,y,x3-opSz,y); ln(x3+opSz,y,x4-opSz,y); out(x4+opSz,y,x5+opSz,y);
            drawOp(x1,y,1,true); drawOp(x2,y,2); drawOp(x3,y,3); drawOp(x4,y,4); } break;
        case 1: { float x1=bx+uw*0.20f,x3=bx+uw*0.40f,x4=bx+uw*0.60f,x5=bx+uw*0.75f,y1=by+uh*0.3f,y2=by+uh*0.7f,ym=by+uh*0.5f;
            ln(x1+opSz,y1,x3-opSz,ym); ln(x1+opSz,y2,x3-opSz,ym); ln(x3+opSz,ym,x4-opSz,ym); out(x4+opSz,ym,x5+opSz,ym);
            drawOp(x1,y1,1,true); drawOp(x1,y2,2); drawOp(x3,ym,3); drawOp(x4,ym,4); } break;
        case 2: { float x1=bx+uw*0.20f,x3=bx+uw*0.40f,x4=bx+uw*0.60f,x5=bx+uw*0.75f,y1=by+uh*0.3f,y2=by+uh*0.7f,ym=by+uh*0.5f;
            ln(x1+opSz,y1,x3+opSz,y1); ln(x3+opSz,y1,x4-opSz,ym); ln(x1+opSz,y2,x3-opSz,y2); ln(x3+opSz,y2,x4-opSz,ym); out(x4+opSz,ym,x5+opSz,ym);
            drawOp(x1,y1,1,true); drawOp(x1,y2,2); drawOp(x3,y2,3); drawOp(x4,ym,4); } break;
        case 3: { float x1=bx+uw*0.2f,x2=bx+uw*0.4f,x4=bx+uw*0.6f,x5=bx+uw*0.75f,y1=by+uh*0.3f,y3=by+uh*0.7f,ym=by+uh*0.5f;
            ln(x1+opSz,y1,x2-opSz,y1); ln(x2+opSz,y1,x4-opSz,ym); ln(x1+opSz,y3,x2+opSz,y3); ln(x2+opSz,y3,x4-opSz,ym); out(x4+opSz,ym,x5+opSz,ym);
            drawOp(x1,y1,1,true); drawOp(x2,y1,2); drawOp(x1,y3,3); drawOp(x4,ym,4); } break;
        case 4: { float x1=bx+uw*0.2f,x2=bx+uw*0.5f,x3=bx+uw*0.75f,y1=by+uh*0.33f,ym=by+uh*0.5f,y2=by+uh*0.66f;
            ln(x1+opSz,y1,x2-opSz,y1); out(x2+opSz,y1,x3+opSz,ym); ln(x1+opSz,y2,x2-opSz,y2); out(x2+opSz,y2,x3+opSz,ym);
            drawOp(x1,y1,1,true); drawOp(x2,y1,2); drawOp(x1,y2,3); drawOp(x2,y2,4); } break;
        case 5: { float x1=bx+uw*0.25f,x2=bx+uw*0.5f,x3=bx+uw*0.75f,y1=by+uh*0.25f,y2=by+uh*0.5f,y3=by+uh*0.75f;
            ln(x1+opSz,y2,x2-opSz,y1); out(x2+opSz,y1,x3+opSz,y2); ln(x1+opSz,y2,x2-opSz,y2); out(x2+opSz,y2,x3+opSz,y2); ln(x1+opSz,y2,x2-opSz,y3); out(x2+opSz,y3,x3+opSz,y2);
            drawOp(x1,y2,1,true); drawOp(x2,y1,2); drawOp(x2,y2,3); drawOp(x2,y3,4); } break;
        case 6: { float x1=bx+uw*0.25f,x2=bx+uw*0.5f,x3=bx+uw*0.75f,y1=by+uh*0.25f,y2=by+uh*0.5f,y3=by+uh*0.75f;
            ln(x1+opSz,y1,x2-opSz,y1); out(x2+opSz,y1,x3+opSz,y2); out(x2+opSz,y2,x3+opSz,y2); out(x2+opSz,y3,x3+opSz,y2);
            drawOp(x1,y1,1,true); drawOp(x2,y1,2); drawOp(x2,y2,3); drawOp(x2,y3,4); } break;
        case 7: { float sp=uw/5.f,y=by+uh*0.4f,yOut=by+uh*0.75f,xOut=bx+uw*0.5f;
            float x1=bx+sp,x2=bx+sp*2,x3=bx+sp*3,x4=bx+sp*4;
            dl->AddLine(ImVec2(x1,y+opSz),ImVec2(xOut,yOut),lineCol,1.5f);
            dl->AddLine(ImVec2(x2,y+opSz),ImVec2(xOut,yOut),lineCol,1.5f);
            dl->AddLine(ImVec2(x3,y+opSz),ImVec2(xOut,yOut),lineCol,1.5f);
            dl->AddLine(ImVec2(x4,y+opSz),ImVec2(xOut,yOut),lineCol,1.5f);
            dl->AddCircleFilled(ImVec2(xOut,yOut),4.f,outCol);
            drawOp(x1,y,1,true); drawOp(x2,y,2); drawOp(x3,y,3); drawOp(x4,y,4); } break;
        }
    }
}

inline void drawEnvelopeIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz, uint8_t ar, uint8_t dr, uint8_t sr, uint8_t sl, uint8_t rr, ImU32 col) {
    float x0 = p0.x + 1, y0 = p0.y + 1, w = sz.x - 2, h = sz.y - 2;
    float yTop = y0 + 2, yBot = y0 + h - 2;
    dl->AddRectFilled(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), IM_COL32(20, 20, 30, 180));
    dl->AddRect(p0, ImVec2(p0.x + sz.x, p0.y + sz.y), (col & 0x00FFFFFFu) | IM_COL32(0, 0, 0, 80));
    
    auto rateW = [&](int rate, int maxRate, float base) -> float { float r = rate / (float)maxRate, c = r * r * r; return base * (1.f - c * 0.95f); };
    float slv = 1.f - sl / 15.f, srv = sr / 31.f;
    float ySL = yTop + (yBot - yTop) * (1.f - slv);
    float wAtk = rateW(ar, 31, w * 0.18f), wDec = rateW(dr, 31, w * 0.18f);
    float wSus = (w * 0.40f) * (1.f - srv) + 3.f * srv, wRel = rateW(rr, 15, w * 0.28f);
    
    float cx = x0; ImVec2 env[5];
    env[0] = ImVec2(cx, yBot); cx += wAtk; env[1] = ImVec2(cx, yTop); cx += wDec;
    env[2] = ImVec2(cx, ySL); cx += wSus; env[3] = ImVec2(cx, ySL); env[4] = ImVec2(cx + wRel, yBot);
    
    ImVec2 fill[8]; int fi = 0;
    for (int i = 0; i < 5; i++) fill[fi++] = env[i];
    fill[fi++] = ImVec2(env[4].x, yBot); fill[fi++] = ImVec2(x0, yBot);
    dl->AddConvexPolyFilled(fill, fi, (col & 0x00FFFFFF) | IM_COL32(0, 0, 0, 40));
    dl->AddPolyline(env, 5, col, 0, 1.5f);
    ImU32 dashCol = (col & 0x00FFFFFF) | IM_COL32(0, 0, 0, 70);
    for (float xd = x0; xd < x0 + w; xd += 6.f) dl->AddLine(ImVec2(xd, ySL), ImVec2(xd + 3.f, ySL), dashCol, 1.f);
    dl->AddText(ImVec2(x0 + 2, y0 + 1), IM_COL32(120, 120, 120, 180), "EG");
}

inline void drawSsgIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz, int ssg, ImU32 col) {
    float x1 = p0.x + 2, x2 = p0.x + sz.x - 2, yBot = p0.y + sz.y - 2, yTop = p0.y + 2, w = x2 - x1;
    if (ssg == 0) { float ym = p0.y + sz.y * 0.5f; dl->AddLine(ImVec2(x1, ym), ImVec2(x2, ym), IM_COL32(80, 80, 80, 200), 1.5f); return; }
    float sw = w / 4.f; ImVec2 pts[16]; int npts = 0;
    auto P = [&](float x, float y) { pts[npts++] = ImVec2(x, y); };
    switch (ssg) {
    case 1: P(x1,yBot); P(x1,yTop); P(x1+sw,yBot); P(x1+sw,yTop); P(x1+sw*2,yBot); P(x1+sw*2,yTop); P(x1+sw*3,yBot); P(x1+sw*3,yTop); P(x1+sw*4,yBot); break;
    case 2: P(x1,yBot); P(x1,yTop); P(x1+sw,yBot); P(x1+sw*4,yBot); break;
    case 3: P(x1,yBot); P(x1,yTop); P(x1+sw,yBot); P(x1+sw*2,yTop); P(x1+sw*3,yBot); P(x1+sw*4,yTop); break;
    case 4: P(x1,yBot); P(x1,yTop); P(x1+sw,yBot); P(x1+sw,yTop); P(x1+sw*4,yTop); break;
    case 5: P(x1,yBot); P(x1+sw,yTop); P(x1+sw,yBot); P(x1+sw*2,yTop); P(x1+sw*2,yBot); P(x1+sw*3,yTop); P(x1+sw*3,yBot); P(x1+sw*4,yTop); break;
    case 6: P(x1,yBot); P(x1+sw,yTop); P(x1+sw*4,yTop); break;
    case 7: P(x1,yBot); P(x1+sw,yTop); P(x1+sw*2,yBot); P(x1+sw*3,yTop); P(x1+sw*4,yBot); break;
    case 8: P(x1,yBot); P(x1+sw,yTop); P(x1+sw,yBot); P(x1+sw*4,yBot); break;
    }
    if (npts >= 2) dl->AddPolyline(pts, npts, col, 0, 1.5f);
}

// =============================================================================
// OPN Editor (YM2612/YM3438)
// =============================================================================

struct OPNEditor {
    xfm_patch_opn patch;
    bool open = false;
    char patchName[64] = "PATCH";
    
    void init(const char* name, const xfm_patch_opn& p) {
        patch = p;
        snprintf(patchName, sizeof(patchName), "%s", name);
    }
    
    bool draw() {
        bool changed = false;
        ImGui::Text("Global"); ImGui::Spacing();
        
        float avail = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float lw = ImGui::CalcTextSize("FMS").x + sp;
        float sw = (avail - lw * 4.f - sp * 3.f) / 4.f; if (sw < 30) sw = 30;
        
        int alg = patch.ALG, fb = patch.FB, ams = patch.AMS, fms = patch.FMS;
        ImGui::Text("ALG"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##ALG", &alg, 0, 7)) { patch.ALG = (uint8_t)alg; changed = true; }
        ImGui::SameLine(); ImGui::Text("FB"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##FB", &fb, 0, 7)) { patch.FB = (uint8_t)fb; changed = true; }
        ImGui::SameLine(); ImGui::Text("AMS"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##AMS", &ams, 0, 3)) { patch.AMS = (uint8_t)ams; changed = true; }
        ImGui::SameLine(); ImGui::Text("FMS"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##FMS", &fms, 0, 7)) { patch.FMS = (uint8_t)fms; changed = true; }
        
        // Algorithm visualizer
        ImVec2 cSize(ImGui::GetContentRegionAvail().x, 72.f);
        ImVec2 cPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##algcanvas", cSize);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(cPos, ImVec2(cPos.x + cSize.x, cPos.y + cSize.y), IM_COL32(20, 20, 30, 180), 4.f);
        char buf[16]; snprintf(buf, sizeof(buf), "ALG %d", patch.ALG);
        dl->AddText(ImVec2(cPos.x + 4, cPos.y + 3), IM_COL32(180, 180, 180, 200), buf);
        drawAlgoIndicator(dl, ImVec2(cPos.x, cPos.y + 14.f), ImVec2(cSize.x, cSize.y - 14.f), patch.ALG, false);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        // Operators
        ImGui::Text("Operators"); ImGui::Spacing();
        static const ImU32 OP_COLORS[4] = { IM_COL32(100,200,255,255), IM_COL32(120,255,140,255), IM_COL32(255,210,80,255), IM_COL32(255,110,110,255) };
        
        if (ImGui::BeginTable("##ops", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("OP 1"); ImGui::TableSetupColumn("OP 2");
            ImGui::TableSetupColumn("OP 3"); ImGui::TableSetupColumn("OP 4");
            ImGui::TableHeadersRow(); ImGui::TableNextRow();
            
            for (int op = 0; op < 4; op++) {
                ImGui::TableSetColumnIndex(op);
                xfm_patch_opn_operator& o = patch.op[op];
                char s[16]; ImU32 opCol = OP_COLORS[op];
                
                float envH = 42.f, envW = ImGui::GetContentRegionAvail().x;
                ImVec2 envPos = ImGui::GetCursorScreenPos();
                snprintf(s, sizeof(s), "##env%d", op);
                ImGui::InvisibleButton(s, ImVec2(envW, envH));
                drawEnvelopeIndicator(ImGui::GetWindowDrawList(), envPos, ImVec2(envW, envH), o.AR, o.DR, o.SR, (uint8_t)o.SL, o.RR, opCol);
                
                int tl = o.TL, ssg = o.SSG, ar = o.AR, dr = o.DR, sr = o.SR, sl = o.SL, rr = o.RR;
                int mul = o.MUL, dt = o.DT, rs = o.RS, am = o.AM;
                
                ImGui::Text("TL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##TL%d", op);
                if (ImGui::SliderInt(s, &tl, 0, 127)) { o.TL = (uint8_t)tl; changed = true; }
                ImGui::Text("SSG"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SSG%d", op);
                if (ImGui::SliderInt(s, &ssg, 0, 8)) { o.SSG = (uint8_t)ssg; changed = true; }
                float ssgH = 18.f, ssgW = ImGui::GetContentRegionAvail().x;
                ImVec2 ssgPos = ImGui::GetCursorScreenPos();
                snprintf(s, sizeof(s), "##ssg%d", op);
                ImGui::InvisibleButton(s, ImVec2(ssgW, ssgH));
                drawSsgIndicator(ImGui::GetWindowDrawList(), ssgPos, ImVec2(ssgW, ssgH), o.SSG, opCol);
                ImGui::Text("AR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##AR%d", op);
                if (ImGui::SliderInt(s, &ar, 0, 31)) { o.AR = (uint8_t)ar; changed = true; }
                ImGui::Text("DR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DR%d", op);
                if (ImGui::SliderInt(s, &dr, 0, 31)) { o.DR = (uint8_t)dr; changed = true; }
                ImGui::Text("SR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SR%d", op);
                if (ImGui::SliderInt(s, &sr, 0, 31)) { o.SR = (uint8_t)sr; changed = true; }
                ImGui::Text("SL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SL%d", op);
                if (ImGui::SliderInt(s, &sl, 0, 15)) { o.SL = (uint8_t)sl; changed = true; }
                ImGui::Text("RR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##RR%d", op);
                if (ImGui::SliderInt(s, &rr, 0, 15)) { o.RR = (uint8_t)rr; changed = true; }
                ImGui::Text("MUL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##MUL%d", op);
                if (ImGui::SliderInt(s, &mul, 0, 15)) { o.MUL = (uint8_t)mul; changed = true; }
                ImGui::Text("DT"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DT%d", op);
                if (ImGui::SliderInt(s, &dt, -3, 3)) { o.DT = (int8_t)dt; changed = true; }
                ImGui::Text("RS"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##RS%d", op);
                if (ImGui::SliderInt(s, &rs, 0, 3)) { o.RS = (uint8_t)rs; changed = true; }
                ImGui::Text("AM"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##AM%d", op);
                if (ImGui::SliderInt(s, &am, 0, 1)) { o.AM = (uint8_t)am; changed = true; }
            }
            ImGui::EndTable();
        }
        return changed;
    }
    
    bool drawWindow(const char* title, ImVec2 size = ImVec2(520, 680)) {
        if (!open) return false;
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        bool stillOpen = true;
        bool changed = false;
        if (ImGui::Begin(title, &stillOpen)) changed = draw();
        ImGui::End();
        if (!stillOpen) open = false;
        return changed;
    }
};

// =============================================================================
// OPL3 Editor (YMF262)
// =============================================================================

struct OPL3Editor {
    xfm_patch_opl patch;
    bool is4op = false;
    bool open = false;
    char patchName[64] = "PATCH";
    
    void init(const char* name, const xfm_patch_opl& p, bool op4 = false) {
        patch = p;
        is4op = op4;
        snprintf(patchName, sizeof(patchName), "%s", name);
    }
    
    bool draw() {
        bool changed = false;
        ImGui::Text("Global"); ImGui::Spacing();
        
        float avail = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float sw = (avail - sp) / 2.f; if (sw < 30) sw = 30;
        
        // 4-op mode toggle
        if (ImGui::Checkbox("4-Operator Mode", &is4op)) { changed = true; }
        ImGui::Spacing();
        
        int algMax = is4op ? 1 : 1;  // OPL3 has limited algos
        int alg = patch.alg, fb = patch.fb;
        ImGui::Text("ALG"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##ALG", &alg, 0, algMax)) { patch.alg = (uint8_t)alg; changed = true; }
        ImGui::SameLine(); ImGui::Text("FB"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##FB", &fb, 0, 7)) { patch.fb = (uint8_t)fb; changed = true; }
        
        // Algorithm visualizer
        ImVec2 cSize(ImGui::GetContentRegionAvail().x, 60.f);
        ImVec2 cPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##algcanvas", cSize);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(cPos, ImVec2(cPos.x + cSize.x, cPos.y + cSize.y), IM_COL32(20, 20, 30, 180), 4.f);
        char buf[16]; snprintf(buf, sizeof(buf), "OPL3 ALG %d%s", patch.alg, is4op ? " (4-op)" : "");
        dl->AddText(ImVec2(cPos.x + 4, cPos.y + 3), IM_COL32(180, 180, 180, 200), buf);
        drawAlgoIndicator(dl, ImVec2(cPos.x, cPos.y + 10.f), ImVec2(cSize.x, cSize.y - 10.f), patch.alg, true, is4op);
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        // Operators
        int numOps = is4op ? 4 : 2;
        ImGui::Text("Operators"); ImGui::Spacing();
        static const ImU32 OP_COLORS[4] = { IM_COL32(100,200,255,255), IM_COL32(120,255,140,255), IM_COL32(255,210,80,255), IM_COL32(255,110,110,255) };
        static const char* WAVE_LBLS[] = {"Sine", "Half-sine", "Square", "Triangle", "Sawtooth", "Pulse", "Double-saw", "Sine+noise"};
        
        if (ImGui::BeginTable("##ops", numOps, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
            for (int i = 0; i < numOps; i++) {
                char colName[16]; snprintf(colName, sizeof(colName), "OP %d%s", i+1, is4op ? "" : (i==0?" (Mod)":" (Car)"));
                ImGui::TableSetupColumn(colName);
            }
            ImGui::TableHeadersRow(); ImGui::TableNextRow();
            
            for (int op = 0; op < numOps; op++) {
                ImGui::TableSetColumnIndex(op);
                xfm_patch_opl_operator& o = patch.op[op];
                char s[16]; ImU32 opCol = OP_COLORS[op];
                
                // Waveform selector PER OPERATOR
                ImGui::Text("Wave"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##wave%d", op);
                int waveIdx = o.wave & 7;
                if (ImGui::Combo(s, &waveIdx, WAVE_LBLS, 8)) { o.wave = (uint8_t)waveIdx; changed = true; }
                
                // Waveform preview
                ImVec2 wvSize(ImGui::GetContentRegionAvail().x, 35.f);
                ImVec2 wvPos = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##waveform", wvSize);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddRectFilled(wvPos, ImVec2(wvPos.x + wvSize.x, wvPos.y + wvSize.y), IM_COL32(20, 20, 30, 180));
                drawWaveform(dl, ImVec2(wvPos.x + 3, wvPos.y + 5), ImVec2(wvSize.x - 6, wvSize.y - 10), o.wave & 7, opCol);
                
                float envH = 42.f, envW = ImGui::GetContentRegionAvail().x;
                ImVec2 envPos = ImGui::GetCursorScreenPos();
                snprintf(s, sizeof(s), "##env%d", op);
                ImGui::InvisibleButton(s, ImVec2(envW, envH));
                drawEnvelopeIndicator(ImGui::GetWindowDrawList(), envPos, ImVec2(envW, envH), o.ar, o.dr, 0, (uint8_t)o.sl, o.rr, opCol);
                
                int tl = o.tl, ar = o.ar, dr = o.dr, sl = o.sl, rr = o.rr;
                int mul = o.mul, am = o.am, vib = o.vib, eg = o.eg, ksr = o.ksr, ksl = o.ksl;
                
                ImGui::Text("TL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##TL%d", op);
                if (ImGui::SliderInt(s, &tl, 0, 63)) { o.tl = (uint8_t)tl; changed = true; }
                ImGui::Text("AR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##AR%d", op);
                if (ImGui::SliderInt(s, &ar, 0, 15)) { o.ar = (uint8_t)ar; changed = true; }
                ImGui::Text("DR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DR%d", op);
                if (ImGui::SliderInt(s, &dr, 0, 15)) { o.dr = (uint8_t)dr; changed = true; }
                ImGui::Text("SL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SL%d", op);
                if (ImGui::SliderInt(s, &sl, 0, 15)) { o.sl = (uint8_t)sl; changed = true; }
                ImGui::Text("RR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##RR%d", op);
                if (ImGui::SliderInt(s, &rr, 0, 15)) { o.rr = (uint8_t)rr; changed = true; }
                ImGui::Text("MUL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##MUL%d", op);
                if (ImGui::SliderInt(s, &mul, 0, 15)) { o.mul = (uint8_t)mul; changed = true; }
                ImGui::Text("AM"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##AM%d", op);
                if (ImGui::SliderInt(s, &am, 0, 1)) { o.am = (uint8_t)am; changed = true; }
                ImGui::Text("VIB"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##VIB%d", op);
                if (ImGui::SliderInt(s, &vib, 0, 1)) { o.vib = (uint8_t)vib; changed = true; }
                ImGui::Text("EG"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##EG%d", op);
                if (ImGui::SliderInt(s, &eg, 0, 1)) { o.eg = (uint8_t)eg; changed = true; }
                ImGui::Text("KSR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##KSR%d", op);
                if (ImGui::SliderInt(s, &ksr, 0, 1)) { o.ksr = (uint8_t)ksr; changed = true; }
                ImGui::Text("KSL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##KSL%d", op);
                if (ImGui::SliderInt(s, &ksl, 0, 3)) { o.ksl = (uint8_t)ksl; changed = true; }
            }
            ImGui::EndTable();
        }
        return changed;
    }
    
    bool drawWindow(const char* title, ImVec2 size = ImVec2(520, 680)) {
        if (!open) return false;
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        bool stillOpen = true;
        bool changed = false;
        if (ImGui::Begin(title, &stillOpen)) changed = draw();
        ImGui::End();
        if (!stillOpen) open = false;
        return changed;
    }
};

// =============================================================================
// OPM Editor (YM2151)
// =============================================================================

struct OPMEditor {
    xfm_patch_opm patch;
    bool open = false;
    char patchName[64] = "PATCH";
    
    void init(const char* name, const xfm_patch_opm& p) {
        patch = p;
        snprintf(patchName, sizeof(patchName), "%s", name);
    }
    
    bool draw() {
        bool changed = false;
        ImGui::Text("Global"); ImGui::Spacing();
        
        float avail = ImGui::GetContentRegionAvail().x;
        float sp = ImGui::GetStyle().ItemSpacing.x;
        float lw = ImGui::CalcTextSize("LFO Wave").x + sp;
        float sw = (avail - lw * 3.f - sp * 2.f) / 3.f; if (sw < 30) sw = 30;
        
        int alg = patch.alg, fb = patch.fb, pan = patch.pan;
        ImGui::Text("ALG"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##ALG", &alg, 0, 7)) { patch.alg = (uint8_t)alg; changed = true; }
        ImGui::SameLine(); ImGui::Text("FB"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##FB", &fb, 0, 7)) { patch.fb = (uint8_t)fb; changed = true; }
        ImGui::SameLine(); ImGui::Text("PAN"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        if (ImGui::SliderInt("##PAN", &pan, 0, 3)) { patch.pan = (uint8_t)pan; changed = true; }
        
        // Algorithm visualizer
        ImVec2 cSize(ImGui::GetContentRegionAvail().x, 72.f);
        ImVec2 cPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##algcanvas", cSize);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(cPos, ImVec2(cPos.x + cSize.x, cPos.y + cSize.y), IM_COL32(20, 20, 30, 180), 4.f);
        char buf[16]; snprintf(buf, sizeof(buf), "OPM ALG %d", patch.alg);
        dl->AddText(ImVec2(cPos.x + 4, cPos.y + 3), IM_COL32(180, 180, 180, 200), buf);
        drawAlgoIndicator(dl, ImVec2(cPos.x, cPos.y + 14.f), ImVec2(cSize.x, cSize.y - 14.f), patch.alg, false);
        ImGui::Spacing();
        
        // LFO
        ImGui::Text("LFO Freq"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        int lfoFreqInt = patch.lfo_freq;
        if (ImGui::SliderInt("##lfoFreq", &lfoFreqInt, 0, 7)) { patch.lfo_freq = (uint8_t)lfoFreqInt; changed = true; }
        ImGui::SameLine(); ImGui::Text("LFO Wave"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
        static const char* LFO_WAVES[] = {"Triangle", "Saw down", "Square", "Noise"};
        int lfoWaveInt = patch.lfo_wave & 3;
        if (ImGui::Combo("##lfoWave", &lfoWaveInt, LFO_WAVES, 4)) { patch.lfo_wave = (uint8_t)lfoWaveInt; changed = true; }
        
        // LFO waveform preview
        ImVec2 wvSize(ImGui::GetContentRegionAvail().x, 50.f);
        ImVec2 wvPos = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##lfoWaveform", wvSize);
        ImDrawList* dl2 = ImGui::GetWindowDrawList();
        dl2->AddRectFilled(wvPos, ImVec2(wvPos.x + wvSize.x, wvPos.y + wvSize.y), IM_COL32(20, 20, 30, 180), 4.f);
        char buf2[32]; snprintf(buf2, sizeof(buf2), "LFO Wave %d", patch.lfo_wave & 3);
        dl2->AddText(ImVec2(wvPos.x + 4, wvPos.y + 3), IM_COL32(180, 180, 180, 200), buf2);
        float cx = wvPos.x + 5.f, cy = wvPos.y + wvSize.y * 0.5f;
        float w = wvSize.x - 10.f, h = wvSize.y * 0.35f;
        ImU32 lfoCol = IM_COL32(200, 160, 255, 255);
        switch (patch.lfo_wave & 3) {
        case 0: dl2->AddLine(ImVec2(cx,cy),ImVec2(cx+w*0.5f,cy-h),lfoCol,2.f); dl2->AddLine(ImVec2(cx+w*0.5f,cy-h),ImVec2(cx+w,cy),lfoCol,2.f); break;
        case 1: dl2->AddLine(ImVec2(cx,cy-h),ImVec2(cx+w,cy+h),lfoCol,2.f); break;
        case 2: dl2->AddLine(ImVec2(cx,cy-h),ImVec2(cx+w*0.5f,cy-h),lfoCol,2.f); dl2->AddLine(ImVec2(cx+w*0.5f,cy+h),ImVec2(cx+w,cy+h),lfoCol,2.f); break;
        case 3: for (int i = 0; i < 50; i++) { float x = cx + (w / 50.f) * i; float y = cy + ((i % 7) - 3) * h * 0.3f; dl2->AddCircleFilled(ImVec2(x, y), 2.f, lfoCol); } break;
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        
        // Operators
        ImGui::Text("Operators"); ImGui::Spacing();
        static const ImU32 OP_COLORS[4] = { IM_COL32(100,200,255,255), IM_COL32(120,255,140,255), IM_COL32(255,210,80,255), IM_COL32(255,110,110,255) };
        
        if (ImGui::BeginTable("##ops", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
            ImGui::TableSetupColumn("OP 1"); ImGui::TableSetupColumn("OP 2");
            ImGui::TableSetupColumn("OP 3"); ImGui::TableSetupColumn("OP 4");
            ImGui::TableHeadersRow(); ImGui::TableNextRow();
            
            for (int op = 0; op < 4; op++) {
                ImGui::TableSetColumnIndex(op);
                xfm_patch_opm_operator& o = patch.op[op];
                char s[16]; ImU32 opCol = OP_COLORS[op];
                
                float envH = 42.f, envW = ImGui::GetContentRegionAvail().x;
                ImVec2 envPos = ImGui::GetCursorScreenPos();
                snprintf(s, sizeof(s), "##env%d", op);
                ImGui::InvisibleButton(s, ImVec2(envW, envH));
                drawEnvelopeIndicator(ImGui::GetWindowDrawList(), envPos, ImVec2(envW, envH), (uint8_t)o.ar, (uint8_t)o.dr, (uint8_t)o.sr, (uint8_t)o.sl, (uint8_t)o.rr, opCol);
                
                int tl = o.tl, ar = o.ar, dr = o.dr, sr = o.sr, sl = o.sl, rr = o.rr;
                int mul = o.mul, dt1 = o.dt1, dt2 = o.dt2, ks = o.ks, ssg = o.ssg;
                
                ImGui::Text("TL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##TL%d", op);
                if (ImGui::SliderInt(s, &tl, 0, 127)) { o.tl = (uint8_t)tl; changed = true; }
                ImGui::Text("DT1"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DT1%d", op);
                if (ImGui::SliderInt(s, &dt1, 0, 3)) { o.dt1 = (uint8_t)dt1; changed = true; }
                ImGui::Text("DT2"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DT2%d", op);
                if (ImGui::SliderInt(s, &dt2, 0, 3)) { o.dt2 = (uint8_t)dt2; changed = true; }
                ImGui::Text("MUL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##MUL%d", op);
                if (ImGui::SliderInt(s, &mul, 0, 15)) { o.mul = (uint8_t)mul; changed = true; }
                ImGui::Text("KS"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##KS%d", op);
                if (ImGui::SliderInt(s, &ks, 0, 3)) { o.ks = (uint8_t)ks; changed = true; }
                ImGui::Text("AR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##AR%d", op);
                if (ImGui::SliderInt(s, &ar, 0, 31)) { o.ar = (uint8_t)ar; changed = true; }
                ImGui::Text("DR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##DR%d", op);
                if (ImGui::SliderInt(s, &dr, 0, 31)) { o.dr = (uint8_t)dr; changed = true; }
                ImGui::Text("SR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SR%d", op);
                if (ImGui::SliderInt(s, &sr, 0, 31)) { o.sr = (uint8_t)sr; changed = true; }
                ImGui::Text("SL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SL%d", op);
                if (ImGui::SliderInt(s, &sl, 0, 15)) { o.sl = (uint8_t)sl; changed = true; }
                ImGui::Text("RR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##RR%d", op);
                if (ImGui::SliderInt(s, &rr, 0, 15)) { o.rr = (uint8_t)rr; changed = true; }
                ImGui::Text("SSG"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                snprintf(s, sizeof(s), "##SSG%d", op);
                if (ImGui::SliderInt(s, &ssg, 0, 7)) { o.ssg = (uint8_t)ssg; changed = true; }
            }
            ImGui::EndTable();
        }
        return changed;
    }
    
    bool drawWindow(const char* title, ImVec2 size = ImVec2(520, 680)) {
        if (!open) return false;
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        bool stillOpen = true;
        bool changed = false;
        if (ImGui::Begin(title, &stillOpen)) changed = draw();
        ImGui::End();
        if (!stillOpen) open = false;
        return changed;
    }
};

} // namespace synth_edit
