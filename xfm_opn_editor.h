#pragma once
// =============================================================================
// xfm_opn_editor.h — YM2612/OPN2 patch editor ImGui widget for xfm API
// Uses IngameFMSerializer from ingamefm_patch_serializer.h for code export/import
// =============================================================================

#include "imgui.h"
#include "xfm_api.h"
#include "ingamefm_patch_serializer.h"
#include <cstdio>
#include <cstring>
#include <string>

// Helper to convert between xfm API (xfm_patch_opn) and old API (YM2612Patch)
inline YM2612Patch toOldPatch(const xfm_patch_opn& p)
{
    YM2612Patch old = {};
    old.ALG = p.ALG;
    old.FB  = p.FB;
    old.AMS = 0;
    old.FMS = 0;
    for (int i = 0; i < 4; i++) {
        old.op[i].DT   = p.op[i].DT;
        old.op[i].MUL  = p.op[i].MUL;
        old.op[i].TL   = p.op[i].TL;
        old.op[i].RS   = p.op[i].RS;
        old.op[i].AR   = p.op[i].AR;
        old.op[i].AM   = p.op[i].AM;
        old.op[i].DR   = p.op[i].DR;
        old.op[i].SR   = p.op[i].SR;
        old.op[i].SL   = p.op[i].SL;
        old.op[i].RR   = p.op[i].RR;
        old.op[i].SSG  = p.op[i].SSG;
    }
    return old;
}

inline xfm_patch_opn toNewPatch(const YM2612Patch& old)
{
    xfm_patch_opn p = {};
    p.ALG = old.ALG;
    p.FB  = old.FB;
    p.LFO = 0;
    for (int i = 0; i < 4; i++) {
        p.op[i].DT   = old.op[i].DT;
        p.op[i].MUL  = old.op[i].MUL;
        p.op[i].TL   = old.op[i].TL;
        p.op[i].RS   = old.op[i].RS;
        p.op[i].AR   = old.op[i].AR;
        p.op[i].AM   = old.op[i].AM;
        p.op[i].DR   = old.op[i].DR;
        p.op[i].SR   = old.op[i].SR;
        p.op[i].SL   = old.op[i].SL;
        p.op[i].RR   = old.op[i].RR;
        p.op[i].SSG  = old.op[i].SSG;
    }
    return p;
}

namespace opn_editor_detail {

static const ImU32 OP_COLORS[4] = {
    IM_COL32(100, 200, 255, 255),
    IM_COL32(120, 255, 140, 255),
    IM_COL32(255, 210,  80, 255),
    IM_COL32(255, 110, 110, 255),
};

inline void drawAlgoIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz, int algo)
{
    float bx = p0.x, by = p0.y, uw = sz.x, uh = sz.y;
    const float opSz = 10.0f;
    const ImU32 lineCol = IM_COL32(180,180,180,200);
    const ImU32 outCol  = IM_COL32(100,255,160,255);
    const float th = 1.5f;

    auto ln = [&](float x1,float y1,float x2,float y2){
        dl->AddLine(ImVec2(x1,y1),ImVec2(x2,y2),lineCol,th); };
    auto out = [&](float x1,float y1,float x2,float){
        dl->AddLine(ImVec2(x1,y1),ImVec2(x2,y1),outCol,th);
        dl->AddCircleFilled(ImVec2(x2,y1),3.5f,outCol); };
    auto drawOp = [&](float cx,float cy,int num,bool first=false){
        ImU32 col=OP_COLORS[num-1], bg=IM_COL32(30,30,40,220);
        dl->AddRectFilled(ImVec2(cx-opSz,cy-opSz),ImVec2(cx+opSz,cy+opSz),bg);
        dl->AddRect(ImVec2(cx-opSz,cy-opSz),ImVec2(cx+opSz,cy+opSz),
                    first?IM_COL32(255,255,255,180):col,2.f,0,1.5f);
        char buf[2]={(char)('0'+num),0};
        ImVec2 ts=ImGui::CalcTextSize(buf);
        dl->AddText(ImVec2(cx-ts.x*.5f,cy-ts.y*.5f),col,buf); };

    switch(algo){
    case 0:{float sp=uw/4.8f,y=by+uh/2,x1=bx+sp*.5f,x2=bx+sp*1.5f,x3=bx+sp*2.5f,x4=bx+sp*3.5f,x5=bx+sp*4.2f;
        ln(x1+opSz,y,x2-opSz,y);ln(x2+opSz,y,x3-opSz,y);ln(x3+opSz,y,x4-opSz,y);out(x4+opSz,y,x5+opSz,y);
        drawOp(x1,y,1,true);drawOp(x2,y,2);drawOp(x3,y,3);drawOp(x4,y,4);break;}
    case 1:{float x1=bx+uw*.20f,x3=bx+uw*.40f,x4=bx+uw*.60f,x5=bx+uw*0.75f,y1=by+uh*.3f,y2=by+uh*.7f,ym=by+uh*.5f;
        ln(x1+opSz,y1,x3-opSz,ym);ln(x1+opSz,y2,x3-opSz,ym);ln(x3+opSz,ym,x4-opSz,ym);out(x4+opSz,ym,x5+opSz,ym);
        drawOp(x1,y1,1,true);drawOp(x1,y2,2);drawOp(x3,ym,3);drawOp(x4,ym,4);break;}
    case 2:{float x1=bx+uw*.20f,x3=bx+uw*.40f,x4=bx+uw*.60f,x5=bx+uw*.75f,y1=by+uh*.3f,y2=by+uh*.7f,ym=by+uh*.5f;
        ln(x1+opSz,y1,x3+opSz,y1);ln(x3+opSz,y1,x4-opSz,ym);ln(x1+opSz,y2,x3-opSz,y2);ln(x3+opSz,y2,x4-opSz,ym);out(x4+opSz,ym,x5+opSz,ym);
        drawOp(x1,y1,1,true);drawOp(x1,y2,2);drawOp(x3,y2,3);drawOp(x4,ym,4);break;}
    case 3:{float x1=bx+uw*.2f,x2=bx+uw*.4f,x4=bx+uw*.6f,x5=bx+uw*.75f,y1=by+uh*.3f,y3=by+uh*.7f,ym=by+uh*.5f;
        ln(x1+opSz,y1,x2-opSz,y1);ln(x2+opSz,y1,x4-opSz,ym);ln(x1+opSz,y3,x2+opSz,y3);ln(x2+opSz,y3,x4-opSz,ym);out(x4+opSz,ym,x5+opSz,ym);
        drawOp(x1,y1,1,true);drawOp(x2,y1,2);drawOp(x1,y3,3);drawOp(x4,ym,4);break;}
    case 4:{float x1=bx+uw*.2f,x2=bx+uw*.5f,x3=bx+uw*.75f,y1=by+uh*.33f,ym=by+uh*.5f,y2=by+uh*.66f;
        ln(x1+opSz,y1,x2-opSz,y1);out(x2+opSz,y1,x3+opSz,ym);ln(x1+opSz,y2,x2-opSz,y2);out(x2+opSz,y2,x3+opSz,ym);
        drawOp(x1,y1,1,true);drawOp(x2,y1,2);drawOp(x1,y2,3);drawOp(x2,y2,4);break;}
    case 5:{float x1=bx+uw*.25f,x2=bx+uw*.5f,x3=bx+uw*.75f,y1=by+uh*.25f,y2=by+uh*.5f,y3=by+uh*.75f;
        ln(x1+opSz,y2,x2-opSz,y1);out(x2+opSz,y1,x3+opSz,y2);ln(x1+opSz,y2,x2-opSz,y2);out(x2+opSz,y2,x3+opSz,y2);
        ln(x1+opSz,y2,x2-opSz,y3);out(x2+opSz,y3,x3+opSz,y2);
        drawOp(x1,y2,1,true);drawOp(x2,y1,2);drawOp(x2,y2,3);drawOp(x2,y3,4);break;}
    case 6:{float x1=bx+uw*.25f,x2=bx+uw*.5f,x3=bx+uw*.75f,y1=by+uh*.25f,y2=by+uh*.5f,y3=by+uh*.75f;
        ln(x1+opSz,y1,x2-opSz,y1);out(x2+opSz,y1,x3+opSz,y2);out(x2+opSz,y2,x3+opSz,y2);out(x2+opSz,y3,x3+opSz,y2);
        drawOp(x1,y1,1,true);drawOp(x2,y1,2);drawOp(x2,y2,3);drawOp(x2,y3,4);break;}
    case 7:{float sp=uw/5.f,y=by+uh*.4f,yOut=by+uh*.75f,xOut=bx+uw*.5f;
        float x1=bx+sp,x2=bx+sp*2,x3=bx+sp*3,x4=bx+sp*4;
        dl->AddLine(ImVec2(x1,y+opSz),ImVec2(xOut,yOut),lineCol,th);
        dl->AddLine(ImVec2(x2,y+opSz),ImVec2(xOut,yOut),lineCol,th);
        dl->AddLine(ImVec2(x3,y+opSz),ImVec2(xOut,yOut),lineCol,th);
        dl->AddLine(ImVec2(x4,y+opSz),ImVec2(xOut,yOut),lineCol,th);
        dl->AddCircleFilled(ImVec2(xOut,yOut),4.f,outCol);
        drawOp(x1,y,1,true);drawOp(x2,y,2);drawOp(x3,y,3);drawOp(x4,y,4);break;}
    default:break;
    }
}

inline void drawSsgIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz, int ssg, ImU32 col)
{
    float x1=p0.x+2,x2=p0.x+sz.x-2,yBot=p0.y+sz.y-2,yTop=p0.y+2,w=x2-x1;
    if(ssg==0){float ym=p0.y+sz.y*.5f;dl->AddLine(ImVec2(x1,ym),ImVec2(x2,ym),IM_COL32(80,80,80,200),1.5f);return;}
    int mode=ssg;float sw=w/4.f;
    ImVec2 pts[16];int npts=0;
    auto P=[&](float x,float y){pts[npts++]=ImVec2(x,y);};
    switch(mode){
    case 1:P(x1,yBot);P(x1,yTop);P(x1+sw,yBot);P(x1+sw,yTop);P(x1+sw*2,yBot);P(x1+sw*2,yTop);P(x1+sw*3,yBot);P(x1+sw*3,yTop);P(x1+sw*4,yBot);break;
    case 2:P(x1,yBot);P(x1,yTop);P(x1+sw,yBot);P(x1+sw*4,yBot);break;
    case 3:P(x1,yBot);P(x1,yTop);P(x1+sw,yBot);P(x1+sw*2,yTop);P(x1+sw*3,yBot);P(x1+sw*4,yTop);break;
    case 4:P(x1,yBot);P(x1,yTop);P(x1+sw,yBot);P(x1+sw,yTop);P(x1+sw*4,yTop);break;
    case 5:P(x1,yBot);P(x1+sw,yTop);P(x1+sw,yBot);P(x1+sw*2,yTop);P(x1+sw*2,yBot);P(x1+sw*3,yTop);P(x1+sw*3,yBot);P(x1+sw*4,yTop);break;
    case 6:P(x1,yBot);P(x1+sw,yTop);P(x1+sw*4,yTop);break;
    case 7:P(x1,yBot);P(x1+sw,yTop);P(x1+sw*2,yBot);P(x1+sw*3,yTop);P(x1+sw*4,yBot);break;
    case 8:P(x1,yBot);P(x1+sw,yTop);P(x1+sw,yBot);P(x1+sw*4,yBot);break;
    }
    if(npts>=2)dl->AddPolyline(pts,npts,col,0,1.5f);
}

inline void drawEnvelopeIndicator(ImDrawList* dl, ImVec2 p0, ImVec2 sz,
                                   const xfm_patch_opn_operator& o, ImU32 col)
{
    float x0=p0.x+1,y0=p0.y+1,w=sz.x-2,h=sz.y-2;
    float yTop=y0+2,yBot=y0+h-2;
    dl->AddRectFilled(p0,ImVec2(p0.x+sz.x,p0.y+sz.y),IM_COL32(20,20,30,180));
    dl->AddRect(p0,ImVec2(p0.x+sz.x,p0.y+sz.y),(col&0x00FFFFFFu)|IM_COL32(0,0,0,80));
    auto rateW=[&](int rate,int maxRate,float base)->float{
        float r=rate/(float)maxRate,c=r*r*r;return base*(1.f-c*.95f);};
    float sl=1.f-o.SL/15.f,sr=o.SR/31.f;
    float ySL=yTop+(yBot-yTop)*(1.f-sl);
    float wAtk=rateW(o.AR,31,w*.18f),wDec=rateW(o.DR,31,w*.18f);
    float wSus=(w*.40f)*(1.f-sr)+3.f*sr,wRel=rateW(o.RR,15,w*.28f);
    float cx=x0;
    ImVec2 env[5];
    env[0]=ImVec2(cx,yBot);cx+=wAtk;env[1]=ImVec2(cx,yTop);cx+=wDec;
    env[2]=ImVec2(cx,ySL);cx+=wSus;env[3]=ImVec2(cx,ySL);env[4]=ImVec2(cx+wRel,yBot);
    ImVec2 fill[8];int fi=0;
    for(int i=0;i<5;i++)fill[fi++]=env[i];
    fill[fi++]=ImVec2(env[4].x,yBot);fill[fi++]=ImVec2(x0,yBot);
    dl->AddConvexPolyFilled(fill,fi,(col&0x00FFFFFF)|IM_COL32(0,0,0,40));
    dl->AddPolyline(env,5,col,0,1.5f);
    ImU32 dashCol=(col&0x00FFFFFF)|IM_COL32(0,0,0,70);
    for(float xd=x0;xd<x0+w;xd+=6.f)dl->AddLine(ImVec2(xd,ySL),ImVec2(xd+3.f,ySL),dashCol,1.f);
    dl->AddText(ImVec2(x0+2,y0+1),IM_COL32(120,120,120,180),"EG");
}

} // namespace opn_editor_detail

struct OPNPatchEditor
{
    xfm_patch_opn  patch;
    int         block      = 0;
    bool        lfoEnable  = false;
    int         lfoFreq    = 0;
    bool        open       = false;
    int         activeTab  = 0;

    static constexpr int CODE_BUF_SIZE = 8192;
    char codeBuf[CODE_BUF_SIZE] = {};

    xfm_patch_opn  savedPatch;
    bool        savedLfoEnable = false;
    int         savedLfoFreq   = 0;
    bool        hasSaved       = false;
    bool        codeHasError   = false;
    char        codeError[256] = {};
    int         codeErrLine    = 0;
    int         codeErrCol     = 0;
    char        patchName[64]  = "PATCH";

    void init(const char* name, const xfm_patch_opn& p,
              bool lfoEn = false, int lfoFr = 0, int blk = 0)
    {
        patch = p; lfoEnable = lfoEn; lfoFreq = lfoFr; block = blk;
        activeTab = 0; hasSaved = false; codeHasError = false;
        snprintf(patchName, sizeof(patchName), "%s", name);
    }

    bool draw()
    {
        using namespace opn_editor_detail;
        bool changed = false;

        const ImVec4 tabAct = ImGui::GetStyleColorVec4(ImGuiCol_TabActive);
        const ImVec4 tabInact = ImGui::GetStyleColorVec4(ImGuiCol_Tab);
        auto drawTab = [&](const char* lbl, int idx) -> bool {
            bool active = (activeTab==idx);
            ImGui::PushStyleColor(ImGuiCol_Button, active?tabAct:tabInact);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_TabHovered));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_TabActive));
            bool clicked = ImGui::Button(lbl);
            ImGui::PopStyleColor(3);
            return clicked;
        };
        if(drawTab("Designer##tab", 0)) activeTab = 0;
        ImGui::SameLine();
        if(drawTab("Code##tab", 1)) {
            activeTab = 1;
            refreshCodeBuf();  // Populate code when switching to Code tab
        }
        ImGui::Separator(); ImGui::Spacing();

        if(activeTab==0) {
            ImGui::Text("Global"); ImGui::Spacing();
            {
                float avail=ImGui::GetContentRegionAvail().x;
                float sp=ImGui::GetStyle().ItemSpacing.x;
                float lw=ImGui::CalcTextSize("LFO").x+sp;
                float sw=(avail-lw*3.f-sp*2.f)/3.f; if(sw<30)sw=30;
                int alg = patch.ALG, fb = patch.FB, lfo = patch.LFO;
                ImGui::Text("ALG"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
                if(ImGui::SliderInt("##ALG",&alg,0,7)) { patch.ALG = (uint8_t)alg; changed=true; }
                ImGui::SameLine();
                ImGui::Text("FB"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
                if(ImGui::SliderInt("##FB", &fb, 0,7)) { patch.FB = (uint8_t)fb; changed=true; }
                ImGui::SameLine();
                ImGui::Text("LFO"); ImGui::SameLine(); ImGui::SetNextItemWidth(sw);
                if(ImGui::SliderInt("##LFO",&lfo,0,255)) { patch.LFO = (uint8_t)lfo; changed=true; }
            }
            {
                ImVec2 cSize(ImGui::GetContentRegionAvail().x,72.f);
                ImVec2 cPos=ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##algcanvas",cSize);
                ImDrawList* dl=ImGui::GetWindowDrawList();
                dl->AddRectFilled(cPos,ImVec2(cPos.x+cSize.x,cPos.y+cSize.y),IM_COL32(20,20,30,180),4.f);
                char buf[16]; snprintf(buf,sizeof(buf),"ALG %d",patch.ALG);
                dl->AddText(ImVec2(cPos.x+4,cPos.y+3),IM_COL32(180,180,180,200),buf);
                drawAlgoIndicator(dl,ImVec2(cPos.x,cPos.y+14.f),ImVec2(cSize.x,cSize.y-14.f),patch.ALG);
            }
            ImGui::Spacing();
            {
                static const char* LFO_LBLS[]={"3.82 Hz","5.33 Hz","5.77 Hz","6.11 Hz","6.60 Hz","9.23 Hz","46.11 Hz","69.22 Hz"};
                if(ImGui::Checkbox("LFO",&lfoEnable)) changed=true;
                ImGui::SameLine();
                ImGui::BeginDisabled(!lfoEnable);
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                if(ImGui::BeginCombo("##lfoFreq",LFO_LBLS[lfoFreq])){
                    for(int i=0;i<8;i++){
                        bool sel=(i==lfoFreq);
                        if(ImGui::Selectable(LFO_LBLS[i],sel)){lfoFreq=i;changed=true;}
                        if(sel)ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::EndDisabled();
            }
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("Block (octave offset)"); ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if(ImGui::SliderInt("##block",&block,-2,4)) changed=true;
            ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
            ImGui::Text("Operators"); ImGui::Spacing();
            if(ImGui::BeginTable("##ops",4,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_SizingStretchSame))
            {
                ImGui::TableSetupColumn("OP 1"); ImGui::TableSetupColumn("OP 2");
                ImGui::TableSetupColumn("OP 3"); ImGui::TableSetupColumn("OP 4");
                ImGui::TableHeadersRow();
                ImGui::TableNextRow();
                for(int op=0;op<4;op++) {
                    ImGui::TableSetColumnIndex(op);
                    xfm_patch_opn_operator& o=patch.op[op];
                    char s[16];
                    ImU32 opCol=OP_COLORS[op];
                    {float envH=42.f,envW=ImGui::GetContentRegionAvail().x;
                     ImVec2 envPos=ImGui::GetCursorScreenPos();
                     snprintf(s,sizeof(s),"##env%d",op);
                     ImGui::InvisibleButton(s,ImVec2(envW,envH));
                     drawEnvelopeIndicator(ImGui::GetWindowDrawList(),envPos,ImVec2(envW,envH),o,opCol);}
                    int tl=o.TL,ssg=o.SSG,ar=o.AR,dr=o.DR,sr=o.SR,sl=o.SL,rr=o.RR;
                    int mul=o.MUL,dt=o.DT,rs=o.RS,am=o.AM;
                    ImGui::Text("TL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##TL%d",op);
                    if(ImGui::SliderInt(s,&tl,0,127)) { o.TL=(uint8_t)tl; changed=true; }
                    ImGui::Text("SSG"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##SSG%d",op);
                    if(ImGui::SliderInt(s,&ssg,0,8)) { o.SSG=(uint8_t)ssg; changed=true; }
                    {float ssgH=18.f,ssgW=ImGui::GetContentRegionAvail().x;
                     ImVec2 ssgPos=ImGui::GetCursorScreenPos();
                     snprintf(s,sizeof(s),"##ssg%d",op);
                     ImGui::InvisibleButton(s,ImVec2(ssgW,ssgH));
                     drawSsgIndicator(ImGui::GetWindowDrawList(),ssgPos,ImVec2(ssgW,ssgH),o.SSG,opCol);}
                    ImGui::Text("AR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##AR%d",op);
                    if(ImGui::SliderInt(s,&ar,0,31)) { o.AR=(uint8_t)ar; changed=true; }
                    ImGui::Text("DR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##DR%d",op);
                    if(ImGui::SliderInt(s,&dr,0,31)) { o.DR=(uint8_t)dr; changed=true; }
                    ImGui::Text("SR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##SR%d",op);
                    if(ImGui::SliderInt(s,&sr,0,31)) { o.SR=(uint8_t)sr; changed=true; }
                    ImGui::Text("SL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##SL%d",op);
                    if(ImGui::SliderInt(s,&sl,0,15)) { o.SL=(uint8_t)sl; changed=true; }
                    ImGui::Text("RR"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##RR%d",op);
                    if(ImGui::SliderInt(s,&rr,0,15)) { o.RR=(uint8_t)rr; changed=true; }
                    ImGui::Text("MUL"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##MUL%d",op);
                    if(ImGui::SliderInt(s,&mul,0,15)) { o.MUL=(uint8_t)mul; changed=true; }
                    ImGui::Text("DT"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##DT%d",op);
                    if(ImGui::SliderInt(s,&dt,-3,3)) { o.DT=(int8_t)dt; changed=true; }
                    ImGui::Text("RS"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##RS%d",op);
                    if(ImGui::SliderInt(s,&rs,0,3)) { o.RS=(uint8_t)rs; changed=true; }
                    ImGui::Text("AM"); ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
                    snprintf(s,sizeof(s),"##AM%d",op);
                    if(ImGui::SliderInt(s,&am,0,1)) { o.AM=(uint8_t)am; changed=true; }
                }
                ImGui::EndTable();
            }
        }

        if(activeTab==1) {
            const float bottomH=ImGui::GetFrameHeightWithSpacing()*2.f+8.f;
            ImVec2 avail=ImGui::GetContentRegionAvail();
            float textH=avail.y-bottomH; if(textH<60.f)textH=60.f;
            ImVec2 textPos=ImGui::GetCursorScreenPos();
            ImGui::InputTextMultiline("##code",codeBuf,CODE_BUF_SIZE,
                ImVec2(avail.x,textH),ImGuiInputTextFlags_AllowTabInput);
            if(codeHasError&&codeErrLine>0) {
                float lineH=ImGui::GetTextLineHeight();
                float lineY=textPos.y+(codeErrLine-1)*lineH;
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(textPos.x,lineY),ImVec2(textPos.x+avail.x,lineY+lineH),
                    IM_COL32(200,40,40,60));
            }
            ImGui::Spacing();
            float btnW=(avail.x-ImGui::GetStyle().ItemSpacing.x)*.5f;
            if(ImGui::Button("Apply & Close",ImVec2(btnW,0))) {
                if(tryApplyCode()){activeTab=0;changed=true;}
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(!hasSaved);
            if(ImGui::Button("Restore",ImVec2(btnW,0))) {
                restoreToSaved(); activeTab=0; changed=true;
            }
            ImGui::EndDisabled();
            if(codeHasError) {
                ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(255,90,90,255));
                if(codeErrLine>0){char loc[64];snprintf(loc,sizeof(loc),"Line %d col %d: ",codeErrLine,codeErrCol);ImGui::TextUnformatted(loc);ImGui::SameLine();}
                ImGui::TextWrapped("%s",codeError);
                ImGui::PopStyleColor();
            } else {
                ImGui::TextDisabled("Edit C++ patch code, then Apply & Close.");
            }
        }
        return changed;
    }

    bool drawWindow(const char* title, ImVec2 size=ImVec2(480,600))
    {
        if(!open) return false;
        ImGui::SetNextWindowSize(size, ImGuiCond_Once);
        bool stillOpen = true;
        bool changed = false;
        if(ImGui::Begin(title, &stillOpen))
            changed = draw();
        ImGui::End();
        if(!stillOpen) open=false;
        return changed;
    }

private:
    void refreshCodeBuf() {
        YM2612Patch old = toOldPatch(patch);
        std::string code = IngameFMSerializer::serialize(old, patchName, block, lfoEnable?1:0, lfoFreq);
        snprintf(codeBuf, CODE_BUF_SIZE, "%s", code.c_str());
        codeHasError=false; codeError[0]='\0'; codeErrLine=0; codeErrCol=0;
    }
    bool tryApplyCode() {
        YM2612Patch outOld = {};
        int outBlock=0, outLfoEn=0, outLfoFreq=0;
        std::string err;
        int errLine=0, errCol=0;
        bool ok = IngameFMSerializer::parse(std::string(codeBuf), outOld, outBlock, outLfoEn, outLfoFreq, err, errLine, errCol);
        if (ok) {
            patch = toNewPatch(outOld);
            block = outBlock;
            lfoEnable = (outLfoEn != 0);
            lfoFreq = outLfoFreq;
            savedPatch = patch;
            savedLfoEnable = lfoEnable;
            savedLfoFreq = lfoFreq;
            hasSaved = true;
            codeHasError = false;
            codeError[0] = '\0';
            return true;
        } else {
            codeHasError = true;
            snprintf(codeError, sizeof(codeError), "%s", err.c_str());
            codeErrLine = errLine;
            codeErrCol = errCol;
            return false;
        }
    }
    void restoreToSaved() {
        if(!hasSaved) return;
        patch = savedPatch;
        lfoEnable = savedLfoEnable;
        lfoFreq = savedLfoFreq;
        refreshCodeBuf();
        savedPatch = patch;
        savedLfoEnable = lfoEnable;
        savedLfoFreq = lfoFreq;
    }
};
