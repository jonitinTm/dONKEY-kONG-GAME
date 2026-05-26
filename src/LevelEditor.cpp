// ============================================================
//  LevelEditor.cpp  —  full file with upgraded cinematic sequencer
// ============================================================
#include "LevelEditor.h"
#include "CinematicData.h"
#include "raymath.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Metadata
// ─────────────────────────────────────────────────────────────────────────────
const char* LevelEditor::ToolName(EditorTool t) {
    switch (t) {
    case EditorTool::SELECT:return"SELECT"; case EditorTool::PLAYER_SPAWN:return"PLAYER";
    case EditorTool::REGULUS:return"REGULUS"; case EditorTool::CAVE:return"CAVE";
    case EditorTool::PLATFORM:return"PLATFORM"; case EditorTool::LADDER:return"LADDER";
    case EditorTool::BEAM:return"BEAM"; case EditorTool::PATH_NODE:return"PATH NODE";
    case EditorTool::NUKE_SPAWN:return"NUKE"; case EditorTool::BEATRICE_SPAWN:return"BEATRICE";
    case EditorTool::ENEMY_SPAWN:return"ENEMY";
    case EditorTool::ELEVATOR:return"ELEVATOR";
    case EditorTool::WIN_ZONE:return"WIN ZONE";
    case EditorTool::KILL_ZONE:return"KILL ZONE";
    case EditorTool::CONVEYOR:return"CONVEYOR";
    case EditorTool::POINT_LIGHT:return"POINT LIGHT";
    case EditorTool::SPOT_LIGHT:return"SPOT LIGHT";
    case EditorTool::SKY_LIGHT:return"SKY LIGHT";
    default:return"???";
    }
}
Color LevelEditor::ToolColor(EditorTool t) {
    switch (t) {
    case EditorTool::SELECT:return LIGHTGRAY; case EditorTool::PLAYER_SPAWN:return GREEN;
    case EditorTool::REGULUS:return{ 160,32,240,255 }; case EditorTool::CAVE:return ORANGE;
    case EditorTool::PLATFORM:return{ 80,120,255,255 }; case EditorTool::LADDER:return YELLOW;
    case EditorTool::BEAM:return{ 140,140,140,255 }; case EditorTool::PATH_NODE:return WHITE;
    case EditorTool::NUKE_SPAWN:return SKYBLUE; case EditorTool::BEATRICE_SPAWN:return MAGENTA;
    case EditorTool::ENEMY_SPAWN:return RED;
    case EditorTool::ELEVATOR:return{ 255,140,50,255 };
    case EditorTool::WIN_ZONE:return{ 80,255,140,255 };
    case EditorTool::KILL_ZONE:return{ 255,60,60,255 };
    case EditorTool::CONVEYOR:return{ 255,200,60,255 };
    case EditorTool::POINT_LIGHT:return{ 255,235,120,255 };
    case EditorTool::SPOT_LIGHT:return{ 200,255,180,255 };
    case EditorTool::SKY_LIGHT:return{ 130,200,255,255 };
    case EditorTool::PROP:       return{ 180,100,220,255 };
    default:return GRAY;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Init
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::Init(int sw, int sh) {
    _sw = sw; _sh = sh;
    _canvasH = (float)(_sh - TOOLBAR_H - BROWSER_H);
    _canvasW = _sw - RIGHT_W;
    _zoom = _canvasH / (float)_sh;
    _cam.offset = { 0.f,(float)TOOLBAR_H };
    _cam.target = { 0.f,0.f };
    _cam.rotation = 0.f;
    _cam.zoom = _zoom;

    // Lighting system sized to canvas region
    _lighting.Init(_canvasW, (int)_canvasH, LightingSystem::Quality::MEDIUM);
    _lighting.SetGlobalAmbient(0.06f);    // very dim ambient floor
    _lighting.SetGlobalDarkness(1.00f);   // 1.0 = use ambient as full floor
    _lighting.SetAmbientColor({ 25, 30, 50, 255 });

    // Preview ON by default — tap F8 (or F8:LIGHT button) to toggle off
    _lightingPreview = true;

    LoadGameSettings(_levelRangeMin, _levelRangeMax, _volMusic, _volSFX, _volAbility, _volUI, _volAmbient, _highScore, _highLevels);
    LoadLevel(1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coordinate helpers
// ─────────────────────────────────────────────────────────────────────────────
Vector2 LevelEditor::WorldMouse() const
{
    return GetScreenToWorld2D(GetMousePosition(), _cam);
}

Vector2 LevelEditor::Snap(Vector2 v) const {
    if (!_gridOn) return v;
    float gs = (float)GRID_SZ / _gridDiv;
    return { roundf(v.x / gs) * gs,roundf(v.y / gs) * gs };
}

Rectangle LevelEditor::RightPanelRect()  const { return { (float)_canvasW,(float)TOOLBAR_H,(float)RIGHT_W,(float)(_sh - TOOLBAR_H) }; }
Rectangle LevelEditor::OutlinerRect()    const { int h = (_sh - TOOLBAR_H - BROWSER_H) / 2; return { (float)_canvasW,(float)TOOLBAR_H,(float)RIGHT_W,(float)h }; }
Rectangle LevelEditor::DataPanelRect()   const {
    int oh = (_sh - TOOLBAR_H - BROWSER_H) / 2;
    return { (float)_canvasW,(float)(TOOLBAR_H + oh),(float)RIGHT_W,(float)(_sh - TOOLBAR_H - BROWSER_H - oh) };
}

bool LevelEditor::InCanvas()    const { Vector2 m = GetMousePosition(); int bottom = _seqOpen ? (_sh - SEQ_H) : (_sh - BROWSER_H); return m.x < _canvasW && m.y >= TOOLBAR_H + 13 && m.y < bottom; }
bool LevelEditor::InBrowser()   const { if (_seqOpen) return false; return GetMousePosition().y >= (_sh - BROWSER_H); }
bool LevelEditor::InToolbar()   const { return GetMousePosition().y < TOOLBAR_H; }
bool LevelEditor::InRightPanel()const { Vector2 m = GetMousePosition(); return m.x >= _canvasW && m.y >= TOOLBAR_H && m.y < (_sh - BROWSER_H); }

// ─────────────────────────────────────────────────────────────────────────────
//  Entity geometry
// ─────────────────────────────────────────────────────────────────────────────
Rectangle LevelEditor::PlatRect(const PlatformData& p) const {
    float h = (p.h > 0.f) ? p.h : 12.f;
    float tr = p.tilt * DEG2RAD, yr = p.w * tanf(tr);
    float ymin = p.y + fminf(0.f, yr);
    return { p.x,ymin,p.w,h + fabsf(yr) };
}
bool LevelEditor::PointInPlatform(Vector2 pt, const PlatformData& pd) const {
    float h = (pd.h > 0.f) ? pd.h : 12.f, tr = pd.tilt * DEG2RAD, yr = pd.w * tanf(tr);
    Vector2 TL = { pd.x,pd.y }, TR = { pd.x + pd.w,pd.y + yr }, BL = { pd.x,pd.y + h }, BR = { pd.x + pd.w,pd.y + yr + h };
    auto Side = [](Vector2 p, Vector2 a, Vector2 b)->float { return (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y); };
    auto InT = [&Side](Vector2 p, Vector2 a, Vector2 b, Vector2 c)->bool {
        float d1 = Side(p, a, b), d2 = Side(p, b, c), d3 = Side(p, c, a);
        return !((d1 < 0 || d2 < 0 || d3 < 0) && (d1 > 0 || d2 > 0 || d3 > 0));
        };
    return InT(pt, TL, TR, BL) || InT(pt, TR, BR, BL);
}
Rectangle LevelEditor::LadRect(const LadderData& l)    const { return { l.x,l.y,l.w,l.h }; }
Rectangle LevelEditor::BeamRect(const BeamData& b)            const {
    // Use the variant texture if available, else fall back to the default beam texture
    Texture2D* tex = nullptr;
    if (b.texVariant >= 1 && b.texVariant <= 12 && _beamVariantTex[b.texVariant - 1] && _beamVariantTex[b.texVariant - 1]->id > 0)
        tex = _beamVariantTex[b.texVariant - 1];
    else if (_beamTex && _beamTex->id > 0)
        tex = _beamTex;
    if (tex) { float s = 4.f; return { b.x, b.y, tex->width * s, tex->height * s }; }
    return { b.x - 8.f, b.y - 4.f, 16.f, 8.f };
}
Vector2 LevelEditor::PlatformCenter(const PlatformData& p) const {
    float h = (p.h > 0.f) ? p.h : 12.f, yr = p.w * tanf(p.tilt * DEG2RAD);
    return { p.x + p.w * .5f,p.y + yr * .5f + h * .5f };
}
Vector2 LevelEditor::EntityCenter(const SelectedEnt& e) const {
    if (!e.valid()) return {};
    int i = e.index;
    switch ((EditorTool)e.type) {
    case EditorTool::PLATFORM: { const auto& p = _level.platforms[i]; return { p.x, p.y }; }
    case EditorTool::LADDER: { auto& l = _level.ladders[i]; return { l.x + l.w * .5f,l.y + l.h * .5f }; }
    case EditorTool::BEAM: { Rectangle r = BeamRect(_level.beams[i]); return { r.x + r.width * .5f,r.y + r.height * .5f }; }
    case EditorTool::WIN_ZONE: { auto r = WinZoneRect(_level.winZone);       return { r.x + r.width * .5f, r.y + r.height * .5f }; }
    case EditorTool::KILL_ZONE: { auto r = KillZoneRect(_level.killZones[i]); return { r.x + r.width * .5f, r.y + r.height * .5f }; }
    case EditorTool::CONVEYOR: { auto r = ConveyorRect(_level.conveyors[i]);  return { r.x + r.width * .5f, r.y + r.height * .5f }; }
    case EditorTool::PROP: { if (i < (int)_level.props.size()) return { _level.props[i].x, _level.props[i].y }; break; }
    case EditorTool::POINT_LIGHT:
    case EditorTool::SPOT_LIGHT:
    case EditorTool::SKY_LIGHT:
        if (i < (int)_level.lights.size())
            return { _level.lights[i].x, _level.lights[i].y };
        return {};
    default: return GetEntPos(e);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entity position accessors
// ─────────────────────────────────────────────────────────────────────────────
Vector2 LevelEditor::GetEntPos(const SelectedEnt& e) const {
    if (!e.valid()) return {};
    int i = e.index;
    switch ((EditorTool)e.type) {
    case EditorTool::PLAYER_SPAWN:   return _level.playerSpawn;
    case EditorTool::REGULUS:        return _level.regulusPos;
    case EditorTool::CAVE:           return _level.cavePos;
    case EditorTool::PLATFORM:       return { _level.platforms[i].x,_level.platforms[i].y };
    case EditorTool::LADDER:         return { _level.ladders[i].x,_level.ladders[i].y };
    case EditorTool::BEAM:           return { _level.beams[i].x, _level.beams[i].y };
    case EditorTool::PATH_NODE:      return { _level.pathNodes[i].x,_level.pathNodes[i].y };
    case EditorTool::NUKE_SPAWN:     return _level.nukeSpawns[i];
    case EditorTool::BEATRICE_SPAWN: return _level.beatriceSpawns[i];
    case EditorTool::ENEMY_SPAWN:    return _level.enemySpawns[i];
    case EditorTool::ELEVATOR:       return { _level.elevators[i].x, _level.elevators[i].y };
    case EditorTool::WIN_ZONE:       return { _level.winZone.x, _level.winZone.y };
    case EditorTool::KILL_ZONE:      return { _level.killZones[i].x, _level.killZones[i].y };
    case EditorTool::CONVEYOR:       return { _level.conveyors[i].x, _level.conveyors[i].y };
    case EditorTool::PROP:           if (i < (int)_level.props.size()) return { _level.props[i].x, _level.props[i].y }; break;
    case EditorTool::POINT_LIGHT:
    case EditorTool::SPOT_LIGHT:
    case EditorTool::SKY_LIGHT:
        if (i < (int)_level.lights.size())
            return { _level.lights[i].x, _level.lights[i].y };
        return {};
    default: return {};
    }
}
void LevelEditor::SetEntPos(const SelectedEnt& e, Vector2 p) {
    if (!e.valid()) return;
    int i = e.index;
    switch ((EditorTool)e.type) {
    case EditorTool::PLAYER_SPAWN:   _level.playerSpawn = p; break;
    case EditorTool::REGULUS:        _level.regulusPos = p;  break;
    case EditorTool::CAVE:           _level.cavePos = p;     break;
    case EditorTool::PLATFORM:       _level.platforms[i].x = p.x; _level.platforms[i].y = p.y; break;
    case EditorTool::LADDER:         _level.ladders[i].x = p.x;   _level.ladders[i].y = p.y;   break;
    case EditorTool::BEAM:           _level.beams[i].x = p.x; _level.beams[i].y = p.y; break;
    case EditorTool::PATH_NODE:      _level.pathNodes[i].x = p.x; _level.pathNodes[i].y = p.y; break;
    case EditorTool::NUKE_SPAWN:     _level.nukeSpawns[i] = p; break;
    case EditorTool::BEATRICE_SPAWN: _level.beatriceSpawns[i] = p; break;
    case EditorTool::ENEMY_SPAWN:    _level.enemySpawns[i] = p; break;
    case EditorTool::ELEVATOR:       _level.elevators[i].x = p.x; _level.elevators[i].y = p.y; break;
    case EditorTool::WIN_ZONE:       _level.winZone.x = p.x; _level.winZone.y = p.y; break;
    case EditorTool::KILL_ZONE:      _level.killZones[i].x = p.x; _level.killZones[i].y = p.y; break;
    case EditorTool::CONVEYOR:       _level.conveyors[i].x = p.x; _level.conveyors[i].y = p.y; break;
    case EditorTool::PROP:           if (i < (int)_level.props.size()) { _level.props[i].x = p.x; _level.props[i].y = p.y; } break;
    case EditorTool::POINT_LIGHT:
    case EditorTool::SPOT_LIGHT:
    case EditorTool::SKY_LIGHT:
        if (i < (int)_level.lights.size()) {
            _level.lights[i].x = p.x; _level.lights[i].y = p.y;
        }
        break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Picking
// ─────────────────────────────────────────────────────────────────────────────
bool LevelEditor::PickEntity(Vector2 p) {
    _sel.clear();
    const float R = 13.f;
    for (int i = 0; i < (int)_level.pathNodes.size(); i++) {
        Vector2 np = { _level.pathNodes[i].x,_level.pathNodes[i].y };
        if (CheckCollisionPointCircle(p, np, 10.f)) { _sel = { (int)EditorTool::PATH_NODE,i }; return true; }
    }
    if (_level.hasPlayerSpawn && CheckCollisionPointCircle(p, _level.playerSpawn, R)) { _sel = { (int)EditorTool::PLAYER_SPAWN,0 }; return true; }
    if (_level.hasRegulus && CheckCollisionPointCircle(p, _level.regulusPos, R)) { _sel = { (int)EditorTool::REGULUS,0 }; return true; }
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.nukeSpawns[i], R)) { _sel = { (int)EditorTool::NUKE_SPAWN,i }; return true; }
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.beatriceSpawns[i], R)) { _sel = { (int)EditorTool::BEATRICE_SPAWN,i }; return true; }
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.enemySpawns[i], R)) { _sel = { (int)EditorTool::ENEMY_SPAWN,i }; return true; }
    for (int i = 0; i < (int)_level.elevators.size(); i++)
        if (CheckCollisionPointRec(p, ElevRect(_level.elevators[i]))) { _sel = { (int)EditorTool::ELEVATOR,i }; return true; }
    for (int i = 0; i < (int)_level.killZones.size(); i++)
        if (CheckCollisionPointRec(p, KillZoneRect(_level.killZones[i]))) { _sel = { (int)EditorTool::KILL_ZONE,i }; return true; }
    if (_level.hasWinZone && CheckCollisionPointRec(p, WinZoneRect(_level.winZone))) { _sel = { (int)EditorTool::WIN_ZONE,0 }; return true; }
    for (int i = 0; i < (int)_level.conveyors.size(); i++)
        if (CheckCollisionPointRec(p, ConveyorRect(_level.conveyors[i]))) { _sel = { (int)EditorTool::CONVEYOR,i }; return true; }
    for (int i = 0; i < (int)_level.lights.size(); i++) {
        Vector2 lp = { _level.lights[i].x, _level.lights[i].y };
        float dx = p.x - lp.x, dy = p.y - lp.y;
        if (dx * dx + dy * dy < 14.f * 14.f) {
            EditorTool t = (_level.lights[i].type == LightType::POINT) ? EditorTool::POINT_LIGHT
                : (_level.lights[i].type == LightType::SPOT) ? EditorTool::SPOT_LIGHT
                : EditorTool::SKY_LIGHT;
            _sel = { (int)t, i };
            return true;
        }
    }
    if (_level.hasCave) {
        float cw = (_caveTex && _caveTex->id > 0) ? 64.f * 3.5f : 50.f;
        float ch = (_caveTex && _caveTex->id > 0) ? 32.f * 3.5f : 50.f;
        if (CheckCollisionPointRec(p, { _level.cavePos.x,_level.cavePos.y,cw,ch })) { _sel = { (int)EditorTool::CAVE,0 }; return true; }
    }
    for (int i = 0; i < (int)_level.beams.size(); i++)
        if (CheckCollisionPointRec(p, BeamRect(_level.beams[i]))) { _sel = { (int)EditorTool::BEAM,i }; return true; }
    for (int i = 0; i < (int)_level.ladders.size(); i++)
        if (CheckCollisionPointRec(p, LadRect(_level.ladders[i]))) { _sel = { (int)EditorTool::LADDER,i }; return true; }
    for (int i = 0; i < (int)_level.platforms.size(); i++)
        if (PointInPlatform(p, _level.platforms[i])) { _sel = { (int)EditorTool::PLATFORM,i }; return true; }
    for (int i = (int)_level.props.size() - 1; i >= 0; i--)
        if (PointInProp(p, _level.props[i])) { _sel = { (int)EditorTool::PROP,i }; return true; }
    return false;
}

bool LevelEditor::IsInMultiSel(const SelectedEnt& e) const
{
    for (const auto& s : _multiSel) if (s == e) return true; return false;
}

void LevelEditor::BoxSelectEntities(Rectangle box) {
    _multiSel.clear();
    if (box.width < 0) { box.x += box.width; box.width = -box.width; }
    if (box.height < 0) { box.y += box.height; box.height = -box.height; }
    auto AC = [&](EditorTool t, Vector2 v, int i) { if (CheckCollisionPointRec(v, box)) _multiSel.push_back({ (int)t,i }); };
    for (int i = 0; i < (int)_level.platforms.size(); i++) if (CheckCollisionRecs(PlatRect(_level.platforms[i]), box)) _multiSel.push_back({ (int)EditorTool::PLATFORM,i });
    for (int i = 0; i < (int)_level.ladders.size(); i++)   if (CheckCollisionRecs(LadRect(_level.ladders[i]), box))   _multiSel.push_back({ (int)EditorTool::LADDER,i });
    for (int i = 0; i < (int)_level.elevators.size(); i++) if (CheckCollisionRecs(ElevRect(_level.elevators[i]), box)) _multiSel.push_back({ (int)EditorTool::ELEVATOR,i });
    for (int i = 0; i < (int)_level.killZones.size(); i++) if (CheckCollisionRecs(KillZoneRect(_level.killZones[i]), box)) _multiSel.push_back({ (int)EditorTool::KILL_ZONE,i });
    if (_level.hasWinZone && CheckCollisionRecs(WinZoneRect(_level.winZone), box)) _multiSel.push_back({ (int)EditorTool::WIN_ZONE,0 });
    for (int i = 0; i < (int)_level.conveyors.size(); i++) if (CheckCollisionRecs(ConveyorRect(_level.conveyors[i]), box)) _multiSel.push_back({ (int)EditorTool::CONVEYOR,i });
    for (int i = 0; i < (int)_level.props.size(); i++)     if (CheckCollisionRecs(PropRect(_level.props[i]), box))          _multiSel.push_back({ (int)EditorTool::PROP,i });
    for (int i = 0; i < (int)_level.beams.size(); i++)     if (CheckCollisionRecs(BeamRect(_level.beams[i]), box))    _multiSel.push_back({ (int)EditorTool::BEAM,i });
    for (int i = 0; i < (int)_level.pathNodes.size(); i++)     AC(EditorTool::PATH_NODE, { _level.pathNodes[i].x,_level.pathNodes[i].y }, i);
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)    AC(EditorTool::NUKE_SPAWN, _level.nukeSpawns[i], i);
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++)AC(EditorTool::BEATRICE_SPAWN, _level.beatriceSpawns[i], i);
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)   AC(EditorTool::ENEMY_SPAWN, _level.enemySpawns[i], i);
    if (_level.hasPlayerSpawn && CheckCollisionPointRec(_level.playerSpawn, box)) _multiSel.push_back({ (int)EditorTool::PLAYER_SPAWN,0 });
    if (_level.hasRegulus && CheckCollisionPointRec(_level.regulusPos, box))      _multiSel.push_back({ (int)EditorTool::REGULUS,0 });
    if (_level.hasCave && CheckCollisionPointRec(_level.cavePos, box))            _multiSel.push_back({ (int)EditorTool::CAVE,0 });
    for (int i = 0; i < (int)_level.lights.size(); i++) {
        Vector2 lp = { _level.lights[i].x, _level.lights[i].y };
        if (CheckCollisionPointRec(lp, box)) {
            EditorTool t = (_level.lights[i].type == LightType::POINT) ? EditorTool::POINT_LIGHT
                : (_level.lights[i].type == LightType::SPOT) ? EditorTool::SPOT_LIGHT
                : EditorTool::SKY_LIGHT;
            _multiSel.push_back({ (int)t, i });
        }
    }
    if (!_multiSel.empty()) _sel = _multiSel[0];
    SetStatus(TextFormat("%d selected.", (int)_multiSel.size()));
}

void LevelEditor::SelectEnt(SelectedEnt e) {
    _sel = e; _multiSel.clear();
    for (int i = 0; i < (int)_outline.size(); i++) {
        if (_outline[i].ent == e) {
            int vis = ((int)OutlinerRect().height - 20) / OUTLINE_ROW;
            if (i < _outlineScroll) _outlineScroll = i;
            else if (i >= _outlineScroll + vis) _outlineScroll = i - vis + 1; break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Select all of same type (K key)
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::SelectAllOfSameType() {
    if (!_sel.valid()) return;
    _multiSel.clear();
    int t = _sel.type;
    auto et = (EditorTool)t;
    if (et == EditorTool::PLATFORM)
        for (int i = 0; i < (int)_level.platforms.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::LADDER)
        for (int i = 0; i < (int)_level.ladders.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::BEAM)
        for (int i = 0; i < (int)_level.beams.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::KILL_ZONE)
        for (int i = 0; i < (int)_level.killZones.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::CONVEYOR)
        for (int i = 0; i < (int)_level.conveyors.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::ELEVATOR)
        for (int i = 0; i < (int)_level.elevators.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::PROP)
        for (int i = 0; i < (int)_level.props.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::PATH_NODE)
        for (int i = 0; i < (int)_level.pathNodes.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::NUKE_SPAWN)
        for (int i = 0; i < (int)_level.nukeSpawns.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::BEATRICE_SPAWN)
        for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::ENEMY_SPAWN)
        for (int i = 0; i < (int)_level.enemySpawns.size(); i++) _multiSel.push_back({t,i});
    else if (et == EditorTool::POINT_LIGHT || et == EditorTool::SPOT_LIGHT || et == EditorTool::SKY_LIGHT)
        for (int i = 0; i < (int)_level.lights.size(); i++) {
            EditorTool lt = (_level.lights[i].type == LightType::POINT) ? EditorTool::POINT_LIGHT
                : (_level.lights[i].type == LightType::SPOT) ? EditorTool::SPOT_LIGHT : EditorTool::SKY_LIGHT;
            _multiSel.push_back({(int)lt, i});
        }
    SetStatus(TextFormat("K: selected all %d of type", (int)_multiSel.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Sync non-positional properties from primary selection to rest of multi-sel
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::SyncPropertiesFromPrimary() {
    if (_multiSel.size() <= 1 || !_sel.valid()) return;
    auto et = (EditorTool)_sel.type;
    int  pi = _sel.index;
    for (auto& e : _multiSel) {
        if (e.index == pi && e.type == _sel.type) continue;
        if (e.type != _sel.type) continue;
        int ii = e.index;
        if (et == EditorTool::BEAM && pi < (int)_level.beams.size() && ii < (int)_level.beams.size()) {
            auto& dst = _level.beams[ii]; const auto& src = _level.beams[pi];
            dst.texVariant = src.texVariant; dst.renderLayer = src.renderLayer;
            dst.flipX = src.flipX; dst.transparent = src.transparent; dst.soundMaterial = src.soundMaterial;
        } else if (et == EditorTool::PLATFORM && pi < (int)_level.platforms.size() && ii < (int)_level.platforms.size()) {
            auto& dst = _level.platforms[ii]; const auto& src = _level.platforms[pi];
            dst.w = src.w; dst.h = src.h; dst.tilt = src.tilt;
        } else if (et == EditorTool::LADDER && pi < (int)_level.ladders.size() && ii < (int)_level.ladders.size()) {
            auto& dst = _level.ladders[ii]; const auto& src = _level.ladders[pi];
            dst.w = src.w; dst.h = src.h;
        } else if (et == EditorTool::KILL_ZONE && pi < (int)_level.killZones.size() && ii < (int)_level.killZones.size()) {
            auto& dst = _level.killZones[ii]; const auto& src = _level.killZones[pi];
            dst.w = src.w; dst.h = src.h; dst.rotation = src.rotation; dst.texId = src.texId; dst.renderLayer = src.renderLayer;
        } else if (et == EditorTool::CONVEYOR && pi < (int)_level.conveyors.size() && ii < (int)_level.conveyors.size()) {
            auto& dst = _level.conveyors[ii]; const auto& src = _level.conveyors[pi];
            dst.length = src.length; dst.speed = src.speed; dst.direction = src.direction;
            dst.rotation = src.rotation; dst.endCapW = src.endCapW; dst.beltH = src.beltH;
        } else if (et == EditorTool::ELEVATOR && pi < (int)_level.elevators.size() && ii < (int)_level.elevators.size()) {
            auto& dst = _level.elevators[ii]; const auto& src = _level.elevators[pi];
            dst.w = src.w; dst.h = src.h; dst.speed = src.speed; dst.direction = src.direction;
        } else if (et == EditorTool::PROP && pi < (int)_level.props.size() && ii < (int)_level.props.size()) {
            auto& dst = _level.props[ii]; const auto& src = _level.props[pi];
            dst.width = src.width; dst.height = src.height; dst.rotation = src.rotation;
            dst.hasCollision = src.hasCollision; dst.lightAffect = src.lightAffect;
            dst.renderLayer = src.renderLayer; dst.texVariant = src.texVariant;
            dst.bobAmp = src.bobAmp; dst.bobSpeed = src.bobSpeed;
            dst.flickerIntens = src.flickerIntens; dst.flickerSpeed = src.flickerSpeed;
            dst.swayAngle = src.swayAngle; dst.swaySpeed = src.swaySpeed;
        } else if ((et == EditorTool::POINT_LIGHT || et == EditorTool::SPOT_LIGHT || et == EditorTool::SKY_LIGHT)
            && pi < (int)_level.lights.size() && ii < (int)_level.lights.size()) {
            float ox = _level.lights[ii].x, oy = _level.lights[ii].y;
            _level.lights[ii] = _level.lights[pi];
            _level.lights[ii].x = ox; _level.lights[ii].y = oy;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Delete
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::DeleteSelected() {
    if (!_sel.valid()) return;
    PushUndo();
    auto Er = [](auto& v, int i) { if (i >= 0 && i < (int)v.size()) v.erase(v.begin() + i); };
    EditorTool t = (EditorTool)_sel.type; int i = _sel.index;
    switch (t) {
    case EditorTool::PLAYER_SPAWN:   _level.hasPlayerSpawn = false; break;
    case EditorTool::REGULUS:        _level.hasRegulus = false; break;
    case EditorTool::CAVE:           _level.hasCave = false; break;
    case EditorTool::PLATFORM:       Er(_level.platforms, i); break;
    case EditorTool::LADDER:         Er(_level.ladders, i); break;
    case EditorTool::BEAM:           Er(_level.beams, i); break;
    case EditorTool::PATH_NODE: {
        for (auto& n : _level.pathNodes) {
            for (int k = 0; k < 3; k++) {
                if (n.next[k] == i) { n.next[k] = -1; n.edgeType[k] = 0; }
                else if (n.next[k] > i) n.next[k]--;
            }
        }
        _selEdge.clear();
        Er(_level.pathNodes, i); break;
    }
    case EditorTool::NUKE_SPAWN:     Er(_level.nukeSpawns, i); break;
    case EditorTool::BEATRICE_SPAWN: Er(_level.beatriceSpawns, i); break;
    case EditorTool::ENEMY_SPAWN:    Er(_level.enemySpawns, i); break;
    case EditorTool::ELEVATOR:       DeleteRelationsFor(_sel); Er(_level.elevators, i); break;
    case EditorTool::WIN_ZONE:       _level.hasWinZone = false; break;
    case EditorTool::KILL_ZONE:      Er(_level.killZones, i); break;
    case EditorTool::CONVEYOR:       Er(_level.conveyors, i); break;
    case EditorTool::PROP:           Er(_level.props, i); break;
    case EditorTool::POINT_LIGHT:
    case EditorTool::SPOT_LIGHT:
    case EditorTool::SKY_LIGHT:      Er(_level.lights, i); break;
    default: break;
    }
    _sel.clear(); _multiSel.clear(); _gizmoDragging = false; _directOp = DirectOp::NONE;
    SetStatus("Deleted.");
}
void LevelEditor::DeleteMultiSelected() {
    if (_multiSel.empty() && !_sel.valid()) return;
    PushUndo();
    auto Er = [](auto& v, int i) { if (i >= 0 && i < (int)v.size()) v.erase(v.begin() + i); };
    auto DT = [&](EditorTool t, auto& vec) {
        std::vector<int> idx;
        for (const auto& e : _multiSel) if (e.type == (int)t) idx.push_back(e.index);
        if (_sel.valid() && _sel.type == (int)t && std::find(idx.begin(), idx.end(), _sel.index) == idx.end()) idx.push_back(_sel.index);
        std::sort(idx.rbegin(), idx.rend()); for (int x : idx) Er(vec, x);
        };
    DT(EditorTool::PLATFORM, _level.platforms); DT(EditorTool::LADDER, _level.ladders);
    DT(EditorTool::BEAM, _level.beams); DT(EditorTool::NUKE_SPAWN, _level.nukeSpawns);
    DT(EditorTool::BEATRICE_SPAWN, _level.beatriceSpawns); DT(EditorTool::ENEMY_SPAWN, _level.enemySpawns);
    DT(EditorTool::ELEVATOR, _level.elevators); DT(EditorTool::KILL_ZONE, _level.killZones);
    DT(EditorTool::CONVEYOR, _level.conveyors); DT(EditorTool::PROP, _level.props);
    // All three light tool types share _level.lights — collect all indices
    {
        std::vector<int> li;
        for (const auto& e : _multiSel) {
            if (e.type == (int)EditorTool::POINT_LIGHT ||
                e.type == (int)EditorTool::SPOT_LIGHT ||
                e.type == (int)EditorTool::SKY_LIGHT) li.push_back(e.index);
        }
        if (_sel.valid() &&
            (_sel.type == (int)EditorTool::POINT_LIGHT ||
                _sel.type == (int)EditorTool::SPOT_LIGHT ||
                _sel.type == (int)EditorTool::SKY_LIGHT) &&
            std::find(li.begin(), li.end(), _sel.index) == li.end())
            li.push_back(_sel.index);
        std::sort(li.rbegin(), li.rend());
        for (int x : li) Er(_level.lights, x);
    }
    std::vector<int> pi;
    for (const auto& e : _multiSel) if (e.type == (int)EditorTool::PATH_NODE) pi.push_back(e.index);
    if (_sel.valid() && _sel.type == (int)EditorTool::PATH_NODE && std::find(pi.begin(), pi.end(), _sel.index) == pi.end()) pi.push_back(_sel.index);
    std::sort(pi.rbegin(), pi.rend());
    for (int x : pi) { for (auto& n : _level.pathNodes) { for (int k=0;k<3;k++){if(n.next[k]==x){n.next[k]=-1;n.edgeType[k]=0;}else if(n.next[k]>x)n.next[k]--;} } Er(_level.pathNodes, x); }
    _selEdge.clear();
    for (const auto& e : _multiSel) {
        if (e.type == (int)EditorTool::PLAYER_SPAWN) _level.hasPlayerSpawn = false;
        if (e.type == (int)EditorTool::REGULUS)      _level.hasRegulus = false;
        if (e.type == (int)EditorTool::CAVE)         _level.hasCave = false;
        if (e.type == (int)EditorTool::WIN_ZONE)     _level.hasWinZone = false;
        if (e.type == (int)EditorTool::KILL_ZONE && e.index < (int)_level.killZones.size())  Er(_level.killZones, e.index);
        if (e.type == (int)EditorTool::CONVEYOR && e.index < (int)_level.conveyors.size())  Er(_level.conveyors, e.index);
        if (e.type == (int)EditorTool::PROP && e.index < (int)_level.props.size())          Er(_level.props, e.index);
    }
    _sel.clear(); _multiSel.clear(); _gizmoDragging = false; _directOp = DirectOp::NONE;
    SetStatus("Deleted selection.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Copy / Paste / Duplicate
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::CopySelected() {
    _clipboard.clear();
    auto AddEnt = [&](const SelectedEnt& e) {
        if (!e.valid()) return;
        ClipboardEntry ce; ce.type = e.type; int i = e.index;
        switch ((EditorTool)e.type) {
        case EditorTool::PLATFORM:       ce.plat = _level.platforms[i]; break;
        case EditorTool::LADDER:         ce.lad = _level.ladders[i]; break;
        case EditorTool::PATH_NODE:      ce.node = _level.pathNodes[i];
            ce.node.next[0] = -1; ce.node.next[1] = -1; ce.node.next[2] = -1;
            ce.node.edgeType[0] = 0; ce.node.edgeType[1] = 0; ce.node.edgeType[2] = 0; break;
        case EditorTool::BEAM:           ce.beam = _level.beams[i]; break;
        case EditorTool::NUKE_SPAWN:     ce.pos = _level.nukeSpawns[i]; break;
        case EditorTool::BEATRICE_SPAWN: ce.pos = _level.beatriceSpawns[i]; break;
        case EditorTool::ENEMY_SPAWN:    ce.pos = _level.enemySpawns[i]; break;
        case EditorTool::PLAYER_SPAWN:   ce.pos = _level.playerSpawn; break;
        case EditorTool::REGULUS:        ce.pos = _level.regulusPos; break;
        case EditorTool::CAVE:           ce.pos = _level.cavePos; break;
        case EditorTool::POINT_LIGHT:
        case EditorTool::SPOT_LIGHT:
        case EditorTool::SKY_LIGHT:
            if (i < (int)_level.lights.size()) ce.light = _level.lights[i];
            else return;
            break;
        case EditorTool::PROP:
            if (i < (int)_level.props.size()) ce.prop = _level.props[i];
            else return;
            break;
        default: return;
        }
        _clipboard.push_back(ce);
        };
    if (_multiSel.size() > 1)
        for (const auto& e : _multiSel) AddEnt(e);
    else if (_sel.valid())
        AddEnt(_sel);
    SetStatus(TextFormat("Copied %d entity/entities.", (int)_clipboard.size()));
}

void LevelEditor::PasteClipboard(bool grabAfter) {
    if (_clipboard.empty()) { SetStatus("Clipboard is empty."); return; }
    PushUndo();
    const float OFF = 32.f;
    std::vector<SelectedEnt> pasted;
    for (auto& ce : _clipboard) {
        SelectedEnt ne; ne.type = ce.type;
        switch ((EditorTool)ce.type) {
        case EditorTool::PLATFORM: {
            PlatformData p = ce.plat; p.x += OFF; p.y += OFF;
            ne.index = (int)_level.platforms.size();
            _level.platforms.push_back(p); break;
        }
        case EditorTool::LADDER: {
            LadderData l = ce.lad; l.x += OFF; l.y += OFF;
            ne.index = (int)_level.ladders.size();
            _level.ladders.push_back(l); break;
        }
        case EditorTool::PATH_NODE: {
            PathNodeData n = ce.node; n.x += OFF; n.y += OFF;
            ne.index = (int)_level.pathNodes.size();
            _level.pathNodes.push_back(n); break;
        }
        case EditorTool::BEAM: {
            ne.index = (int)_level.beams.size();
            BeamData b = ce.beam; b.x += OFF; b.y += OFF;
            _level.beams.push_back(b); break;
        }
        case EditorTool::NUKE_SPAWN: {
            ne.index = (int)_level.nukeSpawns.size();
            _level.nukeSpawns.push_back({ ce.pos.x + OFF,ce.pos.y + OFF }); break;
        }
        case EditorTool::BEATRICE_SPAWN: {
            ne.index = (int)_level.beatriceSpawns.size();
            _level.beatriceSpawns.push_back({ ce.pos.x + OFF,ce.pos.y + OFF }); break;
        }
        case EditorTool::ENEMY_SPAWN: {
            ne.index = (int)_level.enemySpawns.size();
            _level.enemySpawns.push_back({ ce.pos.x + OFF,ce.pos.y + OFF }); break;
        }
        case EditorTool::PLAYER_SPAWN:
            _level.hasPlayerSpawn = true;
            _level.playerSpawn = { ce.pos.x + OFF,ce.pos.y + OFF };
            ne.index = 0; break;
        case EditorTool::REGULUS:
            _level.hasRegulus = true;
            _level.regulusPos = { ce.pos.x + OFF,ce.pos.y + OFF };
            ne.index = 0; break;
        case EditorTool::CAVE:
            _level.hasCave = true;
            _level.cavePos = { ce.pos.x + OFF,ce.pos.y + OFF };
            ne.index = 0; break;
        case EditorTool::POINT_LIGHT:
        case EditorTool::SPOT_LIGHT:
        case EditorTool::SKY_LIGHT: {
            LightData L = ce.light;
            L.x += OFF; L.y += OFF;
            ne.index = (int)_level.lights.size();
            // Keep the tool-type in sync with the stored LightType
            ne.type = (L.type == LightType::POINT) ? (int)EditorTool::POINT_LIGHT
                : (L.type == LightType::SPOT) ? (int)EditorTool::SPOT_LIGHT
                : (int)EditorTool::SKY_LIGHT;
            _level.lights.push_back(L);
            break;
        }
        case EditorTool::PROP: {
            PropData pr = ce.prop; pr.x += OFF; pr.y += OFF;
            ne.index = (int)_level.props.size();
            _level.props.push_back(pr); break;
        }
        default: continue;
        }
        pasted.push_back(ne);
    }
    if (pasted.empty()) return;
    _sel = pasted[0]; _multiSel.clear();
    if (pasted.size() > 1) _multiSel = pasted;
    if (grabAfter) {
        StartDirectOp(DirectOp::MOVE);
        SetStatus(TextFormat("Pasted %d — drag to place, Enter=confirm, ESC=cancel", (int)pasted.size()));
    }
    else {
        SetStatus(TextFormat("Pasted %d entity/entities.", (int)pasted.size()));
    }
}

void LevelEditor::DuplicateSelected() {
    if (!_sel.valid() && _multiSel.empty()) { SetStatus("Nothing to duplicate."); return; }
    CopySelected();
    PasteClipboard(true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load / Save / Undo
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::LoadLevel(int id) {
    _levelId = id;
    if (!::LoadLevel(_level, id)) {
        if (id == 1) { _level = GetDefaultLevel1(); SetStatus("Loaded built-in level 1."); }
        else { _level = LevelData{}; _level.id = id; _level.valid = true; SetStatus("New empty level."); }
    }
    else SetStatus(TextFormat("Loaded level %d.", id));
    _sel.clear(); _multiSel.clear(); _gizmoDragging = false; _directOp = DirectOp::NONE;
    _placingPlatform = _placingLadder = _dragging = _multiDragging = _boxSelecting = false;
    _connectMode = ConnectMode::NONE; _selEdge.clear(); _outlineScroll = 0;
    _undoStack.clear(); _redoStack.clear();
}
void LevelEditor::SaveCurrentLevel() {
    _level.id = _levelId;
    SaveLevel(_level) ? SetStatus(TextFormat("Level %d saved.", _levelId)) : SetStatus("Save FAILED!");
}
void LevelEditor::PushUndo() {
    _undoStack.push_back(_level);
    if ((int)_undoStack.size() > MAX_UNDO) _undoStack.erase(_undoStack.begin());
    _redoStack.clear();
}
void LevelEditor::Undo() {
    if (_undoStack.empty()) { SetStatus("Nothing to undo."); return; }
    _redoStack.push_back(_level);
    _level = _undoStack.back(); _undoStack.pop_back();
    _sel.clear(); _multiSel.clear(); _gizmoDragging = false; _directOp = DirectOp::NONE;
    SetStatus("Undo.");
}
void LevelEditor::Redo() {
    if (_redoStack.empty()) { SetStatus("Nothing to redo."); return; }
    _undoStack.push_back(_level);
    _level = _redoStack.back(); _redoStack.pop_back();
    _sel.clear(); _multiSel.clear(); _gizmoDragging = false; _directOp = DirectOp::NONE;
    SetStatus("Redo.");
}
void LevelEditor::SetStatus(const char* msg, float dur)
{
    strncpy(_status, msg, sizeof(_status) - 1); _statusTimer = dur;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Grab (Blender G / R / S)
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::StartDirectOp(DirectOp op) {
    if (!_sel.valid() && _multiSel.empty()) return;
    if (op == DirectOp::ROTATE && _sel.type != (int)EditorTool::PLATFORM) return;
    if (op == DirectOp::SCALE && _sel.type != (int)EditorTool::PLATFORM
        && _sel.type != (int)EditorTool::LADDER
        && _sel.type != (int)EditorTool::CONVEYOR) return;
    PushUndo();
    _directOp = op; _grabAxisX = false; _grabAxisY = false;
    _grabMouseStart = WorldMouse();
    _grabSingleOrigin = GetSelPos();
    _grabOrigins.clear(); _grabValOrigins.clear();
    if (_sel.valid()) {
        if (op == DirectOp::ROTATE && _sel.type == (int)EditorTool::PLATFORM)
            _grabValOrigin = _level.platforms[_sel.index].tilt;
        else if (op == DirectOp::SCALE && _sel.type == (int)EditorTool::PLATFORM)
            _grabValOrigin = _level.platforms[_sel.index].w;
        else if (op == DirectOp::SCALE && _sel.type == (int)EditorTool::LADDER)
            _grabValOrigin = _level.ladders[_sel.index].h;
        else if (op == DirectOp::SCALE && _sel.type == (int)EditorTool::CONVEYOR)
            _grabValOrigin = _level.conveyors[_sel.index].length;
        else _grabValOrigin = 0.f;
    }
    if (_multiSel.size() > 1) {
        for (const auto& e : _multiSel) {
            _grabOrigins.push_back(GetEntPos(e));
            float v = 0.f;
            if (op == DirectOp::ROTATE && e.type == (int)EditorTool::PLATFORM && e.index < (int)_level.platforms.size()) v = _level.platforms[e.index].tilt;
            if (op == DirectOp::SCALE && e.type == (int)EditorTool::PLATFORM && e.index < (int)_level.platforms.size()) v = _level.platforms[e.index].w;
            if (op == DirectOp::SCALE && e.type == (int)EditorTool::LADDER && e.index < (int)_level.ladders.size())   v = _level.ladders[e.index].h;
            if (op == DirectOp::SCALE && e.type == (int)EditorTool::CONVEYOR && e.index < (int)_level.conveyors.size()) v = _level.conveyors[e.index].length;
            _grabValOrigins.push_back(v);
        }
    }
    const char* hints[] = { "G: move | X/Y=lock | Enter/LMB=OK | ESC/RMB=cancel",
                         "R: rotate | drag horizontal | Enter/LMB=OK | ESC/RMB=cancel",
                         "S: scale | drag horizontal | X=width Y=height | Enter/LMB=OK | ESC/RMB=cancel" };
    SetStatus(hints[(int)op - 1]);
}

void LevelEditor::UpdateDirectOp() {
    if (_directOp == DirectOp::NONE) return;
    if (_directOp != DirectOp::ROTATE) {
        if (IsKeyPressed(KEY_X)) { _grabAxisX = true; _grabAxisY = false; SetStatus("X axis locked"); }
        if (IsKeyPressed(KEY_Y)) { _grabAxisY = true; _grabAxisX = false; SetStatus("Y axis locked"); }
    }
    Vector2 wm = WorldMouse();
    Vector2 delta = Vector2Subtract(wm, _grabMouseStart);
    if (_directOp == DirectOp::MOVE) {
        if (_grabAxisX) delta.y = 0.f;
        if (_grabAxisY) delta.x = 0.f;
        if (_multiSel.size() > 1)
            for (int i = 0; i < (int)_multiSel.size(); i++)
                SetEntPos(_multiSel[i], Snap(Vector2Add(_grabOrigins[i], delta)));
        else if (_sel.valid())
            SetSelPos(Snap(Vector2Add(_grabSingleOrigin, delta)));
    }
    else if (_directOp == DirectOp::ROTATE) {
        float newTilt = fmaxf(-89.f, fminf(89.f, _grabValOrigin + delta.x * 0.3f));
        if (_sel.valid() && _sel.type == (int)EditorTool::PLATFORM)
            _level.platforms[_sel.index].tilt = newTilt;
        for (int i = 0; i < (int)_multiSel.size(); i++) {
            const auto& e = _multiSel[i];
            if (e.type == (int)EditorTool::PLATFORM && e.index < (int)_level.platforms.size())
                _level.platforms[e.index].tilt = fmaxf(-89.f, fminf(89.f, _grabValOrigins[i] + delta.x * 0.3f));
        }
    }
    else if (_directOp == DirectOp::SCALE) {
        bool doW = (!_grabAxisX && !_grabAxisY) || _grabAxisX;
        bool doH = (!_grabAxisX && !_grabAxisY) || _grabAxisY;
        if (_sel.valid()) {
            if (_sel.type == (int)EditorTool::PLATFORM && doW)
                _level.platforms[_sel.index].w = fmaxf((float)GRID_SZ, _grabValOrigin + delta.x);
            if (_sel.type == (int)EditorTool::LADDER && doH)
                _level.ladders[_sel.index].h = fmaxf((float)GRID_SZ, _grabValOrigin - delta.y);
            if (_sel.type == (int)EditorTool::CONVEYOR && doW)
                _level.conveyors[_sel.index].length = fmaxf(64.f, _grabValOrigin + delta.x);
        }
        for (int i = 0; i < (int)_multiSel.size(); i++) {
            const auto& e = _multiSel[i];
            if (e.type == (int)EditorTool::PLATFORM && doW && e.index < (int)_level.platforms.size())
                _level.platforms[e.index].w = fmaxf((float)GRID_SZ, _grabValOrigins[i] + delta.x);
            if (e.type == (int)EditorTool::LADDER && doH && e.index < (int)_level.ladders.size())
                _level.ladders[e.index].h = fmaxf((float)GRID_SZ, _grabValOrigins[i] - delta.y);
            if (e.type == (int)EditorTool::CONVEYOR && doW && e.index < (int)_level.conveyors.size())
                _level.conveyors[e.index].length = fmaxf(64.f, _grabValOrigins[i] + delta.x);
        }
    }
    if (IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) ConfirmDirectOp();
    if (IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) CancelDirectOp();
}

void LevelEditor::ConfirmDirectOp() {
    const char* msgs[] = { "Moved.","Rotated.","Scaled." };
    SetStatus(msgs[(int)_directOp - 1]);
    _directOp = DirectOp::NONE; _grabAxisX = false; _grabAxisY = false;
}

void LevelEditor::CancelDirectOp() {
    if (_directOp == DirectOp::MOVE) {
        if (_multiSel.size() > 1)
            for (int i = 0; i < (int)_multiSel.size(); i++) SetEntPos(_multiSel[i], _grabOrigins[i]);
        else if (_sel.valid()) SetSelPos(_grabSingleOrigin);
    }
    else {
        if (!_undoStack.empty()) { _level = _undoStack.back(); _undoStack.pop_back(); }
    }
    _directOp = DirectOp::NONE; _grabAxisX = false; _grabAxisY = false;
    SetStatus("Cancelled.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Gizmo
// ─────────────────────────────────────────────────────────────────────────────
GizmoAxis LevelEditor::GizmoHitTest(Vector2 center, Vector2 wm) const {
    const float R = GIZMO_R, tip = 10.f, cs = 9.f;
    if (CheckCollisionPointRec(wm, { center.x - cs,center.y - cs,cs * 2,cs * 2 })) return GizmoAxis::FREE;
    if (_gizmo == GizmoMode::ROTATE) {
        float d = Vector2Distance(center, wm);
        return (d >= R - 10.f && d <= R + 10.f) ? GizmoAxis::RING : GizmoAxis::NONE;
    }
    if (_gizmo == GizmoMode::MOVE || _gizmo == GizmoMode::SCALE) {
        if (CheckCollisionPointCircle(wm, { center.x + R,center.y }, tip)) return GizmoAxis::X;
        if (CheckCollisionPointCircle(wm, { center.x,center.y - R }, tip)) return GizmoAxis::Y;
    }
    return GizmoAxis::NONE;
}

void LevelEditor::UpdateGizmo() {
    if (!_sel.valid() || _gizmo == GizmoMode::SELECT || _directOp != DirectOp::NONE) return;
    Vector2 wm = WorldMouse(), center = EntityCenter(_sel); (void)wm;
    bool lP = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool lD = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool lR = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    _gizmoHot = GizmoHitTest(center, wm);
    if (!_gizmoDragging) {
        if (lP && _gizmoHot != GizmoAxis::NONE) {
            PushUndo();
            _gizmoDragging = true; _gizmoDragAxis = _gizmoHot; _gizmoDragStart = wm;
            _gizmoPosStart = GetEntPos(_sel);
            _gizmoValStart = (_sel.type == (int)EditorTool::PLATFORM) ? _level.platforms[_sel.index].tilt
                : (_sel.type == (int)EditorTool::LADDER) ? _level.ladders[_sel.index].h : 0.f;
        }
    }
    else {
        if (lD) {
            Vector2 d = Vector2Subtract(wm, _gizmoDragStart);
            if (_gizmo == GizmoMode::MOVE) {
                Vector2 np = _gizmoPosStart;
                if (_gizmoDragAxis == GizmoAxis::X || _gizmoDragAxis == GizmoAxis::FREE) np.x = Snap({ _gizmoPosStart.x + d.x,0 }).x;
                if (_gizmoDragAxis == GizmoAxis::Y || _gizmoDragAxis == GizmoAxis::FREE) np.y = Snap({ 0,_gizmoPosStart.y + d.y }).y;
                SetEntPos(_sel, np);
            }
            else if (_gizmo == GizmoMode::ROTATE && _sel.type == (int)EditorTool::PLATFORM) {
                float t = fmaxf(-89.f, fminf(89.f, _gizmoValStart + d.x * .3f));
                _level.platforms[_sel.index].tilt = t;
            }
            else if (_gizmo == GizmoMode::SCALE) {
                if (_sel.type == (int)EditorTool::PLATFORM && (_gizmoDragAxis == GizmoAxis::X || _gizmoDragAxis == GizmoAxis::FREE))
                    _level.platforms[_sel.index].w = fmaxf((float)GRID_SZ, _gizmoValStart + d.x);
                if (_sel.type == (int)EditorTool::LADDER && (_gizmoDragAxis == GizmoAxis::Y || _gizmoDragAxis == GizmoAxis::FREE))
                    _level.ladders[_sel.index].h = fmaxf((float)GRID_SZ, _gizmoValStart - d.y);
            }
        }
        if (lR) { _gizmoDragging = false; _gizmoDragAxis = GizmoAxis::NONE; }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Outliner
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::BuildOutline() {
    _outline.clear();

    auto hasParent = [&](SelectedEnt e)->bool {
        for (const auto& r : _level.relations) if (r.child == e) return true;
        return false;
        };

    auto EmitRoot = [&](SelectedEnt e) { if (!hasParent(e)) BuildOutlineTree(e, 0); };

    if (_level.hasPlayerSpawn) EmitRoot({ (int)EditorTool::PLAYER_SPAWN, 0 });
    if (_level.hasRegulus)     EmitRoot({ (int)EditorTool::REGULUS, 0 });
    if (_level.hasCave)        EmitRoot({ (int)EditorTool::CAVE, 0 });
    if (_level.hasWinZone)     EmitRoot({ (int)EditorTool::WIN_ZONE,0 });
    for (int i = 0; i < (int)_level.elevators.size(); i++)    EmitRoot({ (int)EditorTool::ELEVATOR,i });
    for (int i = 0; i < (int)_level.platforms.size(); i++)    EmitRoot({ (int)EditorTool::PLATFORM,i });
    for (int i = 0; i < (int)_level.ladders.size(); i++)      EmitRoot({ (int)EditorTool::LADDER,i });
    for (int i = 0; i < (int)_level.beams.size(); i++)        EmitRoot({ (int)EditorTool::BEAM,i });
    for (int i = 0; i < (int)_level.pathNodes.size(); i++)    EmitRoot({ (int)EditorTool::PATH_NODE,i });
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)   EmitRoot({ (int)EditorTool::NUKE_SPAWN,i });
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++) EmitRoot({ (int)EditorTool::BEATRICE_SPAWN,i });
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)  EmitRoot({ (int)EditorTool::ENEMY_SPAWN,i });
    for (int i = 0; i < (int)_level.killZones.size(); i++)    EmitRoot({ (int)EditorTool::KILL_ZONE,i });
    for (int i = 0; i < (int)_level.conveyors.size(); i++)    EmitRoot({ (int)EditorTool::CONVEYOR,i });
    for (int i = 0; i < (int)_level.props.size(); i++)        EmitRoot({ (int)EditorTool::PROP,i });
    for (int i = 0; i < (int)_level.lights.size(); i++) {
        EditorTool t = (_level.lights[i].type == LightType::POINT) ? EditorTool::POINT_LIGHT
            : (_level.lights[i].type == LightType::SPOT) ? EditorTool::SPOT_LIGHT
            : EditorTool::SKY_LIGHT;
        EmitRoot({ (int)t, i });
    }

    int vis = (int)(OutlinerRect().height - 20) / OUTLINE_ROW;
    int maxScroll = (int)_outline.size() - vis;
    if (_outlineScroll > maxScroll) _outlineScroll = std::max(0, maxScroll);
}

// ─────────────────────────────────────────────────────────────────────────────
//  NumField widget
// ─────────────────────────────────────────────────────────────────────────────
bool LevelEditor::NumField(const char* label, float& val, float sens, float minV, float maxV, float x, float y, float fw) {
    DrawText(label, (int)x, (int)y + 3, 11, { 160,165,180,255 });
    float lw = MeasureText(label, 11) + 6;
    Rectangle vr = { x + lw,y,fw - lw,16 };
    Vector2 mp = GetMousePosition();
    bool hov = CheckCollisionPointRec(mp, vr);
    bool changed = false;

    // ── Active text-editing mode ──────────────────────────────────────────────
    bool isTyping = (_fieldTyping && _fieldTypingPtr == &val);
    if (isTyping) {
        // Draw active editing box
        DrawRectangleRec(vr, { 25,45,80,255 });
        DrawRectangleLinesEx(vr, 2.f, { 80,160,255,255 });

        // Handle keyboard input
        int key = GetCharPressed();
        while (key > 0) {
            int len = (int)strlen(_fieldTypeBuf);
            if (len < 30) {
                bool valid = (key >= '0' && key <= '9') || key == '-' || key == '.' || key == ',';
                if (valid) {
                    char ch = (char)(key == ',' ? '.' : key);
                    _fieldTypeBuf[len] = ch;
                    _fieldTypeBuf[len + 1] = '\0';
                }
            }
            key = GetCharPressed();
        }
        if (IsKeyPressed(KEY_BACKSPACE)) {
            int len = (int)strlen(_fieldTypeBuf);
            if (len > 0) _fieldTypeBuf[len - 1] = '\0';
        }

        // Confirm on Enter or Tab or clicking outside
        bool confirm = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_TAB);
        bool cancel = IsKeyPressed(KEY_ESCAPE);
        bool clickOut = IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !hov;
        if (confirm || clickOut) {
            float parsed = (float)atof(_fieldTypeBuf);
            val = fmaxf(minV, fminf(maxV, parsed));
            changed = true;
            _fieldTyping = false;
            _fieldTypingPtr = nullptr;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
        if (cancel) {
            _fieldTyping = false;
            _fieldTypingPtr = nullptr;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }

        // Draw the typed text + blinking cursor
        DrawText(_fieldTypeBuf, (int)(vr.x + 4), (int)vr.y + 3, 11, WHITE);
        if ((int)(GetTime() * 2) % 2 == 0) {
            int tw = MeasureText(_fieldTypeBuf, 11);
            DrawLine((int)(vr.x + 4 + tw), (int)vr.y + 2, (int)(vr.x + 4 + tw), (int)(vr.y + 13), WHITE);
        }
        return changed;
    }

    // ── Normal drag mode ──────────────────────────────────────────────────────
    Color bg = hov ? Color{ 55,58,75,255 } : Color{ 38,40,54,255 };
    DrawRectangleRec(vr, bg);
    DrawRectangleLinesEx(vr, 1, hov ? Color{ 90,100,140,255 } : Color{ 55,60,80,255 });
    char vbuf[32]; snprintf(vbuf, sizeof(vbuf), "%.2f", val);
    int vtw = MeasureText(vbuf, 11);
    DrawText(vbuf, (int)(vr.x + vr.width / 2 - vtw / 2), (int)vr.y + 3, 11, WHITE);
    if (hov) DrawText("\xe2\x86\x94", (int)(vr.x + 2), (int)vr.y + 1, 11, { 100,200,255,180 });

    if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !_fieldDrag && !_fieldTyping) {
        double now = GetTime();
        bool dblClick = (now - _fieldLastClick) < 0.35;
        _fieldLastClick = now;
        if (dblClick) {
            // Activate text-entry mode
            _fieldTyping = true;
            _fieldTypingPtr = &val;
            snprintf(_fieldTypeBuf, sizeof(_fieldTypeBuf), "%.2f", val);
            SetMouseCursor(MOUSE_CURSOR_IBEAM);
        }
        else {
            // Start drag mode
            _fieldDrag = true; _fieldPtr = &val;
            _fieldDragStartX = mp.x; _fieldDragStartVal = val;
            _fieldDragSens = sens; _fieldMin = minV; _fieldMax = maxV;
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
        }
    }
    if (_fieldDrag && _fieldPtr == &val) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float dx = mp.x - _fieldDragStartX;
            val = fmaxf(minV, fminf(maxV, _fieldDragStartVal + dx * sens));
            changed = true;
        }
        else {
            _fieldDrag = false; _fieldPtr = nullptr;
            SetMouseCursor(MOUSE_CURSOR_DEFAULT);
        }
    }
    return changed;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Update sub-routines
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::UpdateToolbar() {
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    // While a NumField is in text-entry mode, suppress ALL editor hotkeys so
    // typed characters (G, S, R, Delete, Backspace, Tab, etc.) go to the field.
    if (!_fieldTyping) {
        if (ctrl && IsKeyPressed(KEY_S)) SaveCurrentLevel();
        if (ctrl && IsKeyPressed(KEY_Z)) Undo();
        if (ctrl && IsKeyPressed(KEY_Y)) Redo();
        if (ctrl && IsKeyPressed(KEY_C)) CopySelected();
        if (ctrl && IsKeyPressed(KEY_V)) PasteClipboard(true);
        if (ctrl && IsKeyPressed(KEY_D)) DuplicateSelected();

        if (IsKeyPressed(KEY_H)) { _gridOn = !_gridOn; SetStatus(_gridOn ? "Grid ON" : "Grid OFF"); }
        if (IsKeyPressed(KEY_G) && _tool == EditorTool::SELECT && _directOp == DirectOp::NONE) StartDirectOp(DirectOp::MOVE);

        if (IsKeyPressed(KEY_ONE) || IsKeyPressed(KEY_Q)) { _gizmo = GizmoMode::SELECT; _tool = EditorTool::SELECT; SetStatus("1/Q: SELECT mode"); }
        if (IsKeyPressed(KEY_TWO) || IsKeyPressed(KEY_W)) { _gizmo = GizmoMode::MOVE;  _tool = EditorTool::SELECT; SetStatus("2/W: MOVE gizmo"); }
        if (IsKeyPressed(KEY_THREE)) { _gizmo = GizmoMode::ROTATE; _tool = EditorTool::SELECT; SetStatus("3: ROTATE gizmo"); }
        if (IsKeyPressed(KEY_E)) {
            // E on a selected path node = extrude (add connected child node)
            if (_sel.valid() && _sel.type == (int)EditorTool::PATH_NODE) {
                int selIdx = _sel.index;
                int freeSlot = -1;
                for (int k = 0; k < 3; k++) {
                    if (_level.pathNodes[selIdx].next[k] < 0) { freeSlot = k; break; }
                }
                if (freeSlot >= 0) {
                    PushUndo();
                    float ox = _level.pathNodes[selIdx].x;
                    float oy = _level.pathNodes[selIdx].y;
                    PathNodeData newNode; newNode.x = ox + 80.f; newNode.y = oy + 40.f;
                    int newIdx = (int)_level.pathNodes.size();
                    _level.pathNodes[selIdx].next[freeSlot] = newIdx;
                    _level.pathNodes.push_back(newNode);
                    _sel = { (int)EditorTool::PATH_NODE, newIdx };
                    SetStatus(TextFormat("Extruded: node %d slot %d -> node %d", selIdx, freeSlot, newIdx));
                } else {
                    SetStatus("Node already has 3 branches.");
                }
            } else {
                _gizmo = GizmoMode::ROTATE; _tool = EditorTool::SELECT; SetStatus("E: ROTATE gizmo");
            }
        }
        if (IsKeyPressed(KEY_FOUR)) { _gizmo = GizmoMode::SCALE; _tool = EditorTool::SELECT; SetStatus("4: SCALE gizmo"); }
        if (IsKeyPressed(KEY_R) && _directOp == DirectOp::NONE) StartDirectOp(DirectOp::ROTATE);

        if (IsKeyPressed(KEY_S) && !ctrl && _directOp == DirectOp::NONE) {
            if (_seqOpen && _activeSeq >= 0 && _sel.valid()) SeqAddKeyframe();
            else StartDirectOp(DirectOp::SCALE);
        }

        if (_directOp == DirectOp::NONE) {
            if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE))
                _multiSel.empty() ? DeleteSelected() : DeleteMultiSelected();
            if (IsKeyPressed(KEY_ESCAPE)) {
                _connectMode = ConnectMode::NONE; _selEdge.clear();
                _tool = EditorTool::SELECT; _gizmo = GizmoMode::SELECT;
                _sel.clear(); _multiSel.clear(); _boxSelecting = false; _gizmoDragging = false;
                SetStatus("Cancelled.");
            }
            if (IsKeyPressed(KEY_TAB)) {
                _seqOpen = !_seqOpen;
                if (!_seqOpen) SeqPreviewStop();
                SeqLoad();
                SetStatus(_seqOpen ? "Sequencer open (Tab to close)  SPACE=play  S=keyframe  RMB=pan  MWheel=zoom" : "Sequencer closed");
            }
            if (IsKeyPressed(KEY_LEFT_BRACKET)) { _gridDiv = (_gridDiv == 1) ? 4 : (_gridDiv == 4 ? 2 : 1); SetStatus(TextFormat("Grid ÷%d", _gridDiv)); }
            if (IsKeyPressed(KEY_RIGHT_BRACKET)) { _gridDiv = (_gridDiv == 1) ? 2 : (_gridDiv == 2 ? 4 : 1); SetStatus(TextFormat("Grid ÷%d", _gridDiv)); }
            if (!ctrl) {
                if (IsKeyPressed(KEY_LEFT) && _levelId > 1) { SaveCurrentLevel(); LoadLevel(_levelId - 1); }
                if (IsKeyPressed(KEY_RIGHT) && _levelId < 10) { SaveCurrentLevel(); LoadLevel(_levelId + 1); }
            }
            if (IsKeyPressed(KEY_B)) { _wantsEmote = true; SetStatus("Emote!"); }
            if (IsKeyPressed(KEY_N)) { _wantsMenu = true; }
            if (IsKeyPressed(KEY_K) && _sel.valid()) SelectAllOfSameType();
        }
    } // end !_fieldTyping

    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || !InToolbar()) return;
    Vector2 mouse = GetMousePosition();
    auto TBtn = [&](int c)->Rectangle {float w = (float)_sw / 8.f; return{ c * w,0,w - 2,(float)TOOLBAR_H - 2 }; };
    if (CheckCollisionPointRec(mouse, TBtn(0)) && _levelId > 1) { SaveCurrentLevel(); LoadLevel(--_levelId); }
    if (CheckCollisionPointRec(mouse, TBtn(2)) && _levelId < 10) { SaveCurrentLevel(); LoadLevel(++_levelId); }
    if (CheckCollisionPointRec(mouse, TBtn(3))) { _gridOn = !_gridOn; SetStatus(_gridOn ? "Grid ON" : "Grid OFF"); }
    if (CheckCollisionPointRec(mouse, TBtn(4))) { _gridDiv = (_gridDiv == 1) ? 2 : (_gridDiv == 2) ? 4 : 1; SetStatus(TextFormat("Grid ÷%d", _gridDiv)); }
    if (CheckCollisionPointRec(mouse, TBtn(5))) SaveCurrentLevel();
    if (CheckCollisionPointRec(mouse, TBtn(6))) { SaveCurrentLevel(); _wantsPlay = true; }
    if (CheckCollisionPointRec(mouse, TBtn(7))) _wantsEmote = true;
}

Rectangle LevelEditor::BrowserBtn(int row, int col, int cols) const {
    float bw = (float)_canvasW / cols, bh = 36.f;
    float by = (float)(_sh - BROWSER_H) + 4 + row * (bh + 4);
    return { col * bw + 2,by,bw - 4,bh };
}
void LevelEditor::UpdateBrowser() {
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || !InBrowser()) return;
    Vector2 mouse = GetMousePosition();
    if (mouse.x >= _canvasW) return;
    const int r0[] = { 0,1,2,3,4,5 };
    const int r1[] = { 6,7,8,9,10,11 };
    const int r2[] = { 12,13,14,15,16,17,18 };  // ...,POINT_LIGHT,SPOT_LIGHT,SKY_LIGHT,PROP
    for (int c = 0; c < 6; c++) if (CheckCollisionPointRec(mouse, BrowserBtn(0, c, 6)))
    {
        _tool = (EditorTool)r0[c]; _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE; SetStatus(TextFormat("Tool: %s", ToolName(_tool))); return;
    }
    for (int c = 0; c < 6; c++) if (CheckCollisionPointRec(mouse, BrowserBtn(1, c, 6)))
    {
        _tool = (EditorTool)r1[c]; _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE; SetStatus(TextFormat("Tool: %s", ToolName(_tool))); return;
    }
    for (int c = 0; c < 7; c++) if (r2[c] >= 0 && CheckCollisionPointRec(mouse, BrowserBtn(2, c, 7)))
    {
        _tool = (EditorTool)r2[c]; _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE; SetStatus(TextFormat("Tool: %s", ToolName(_tool))); return;
    }
}

void LevelEditor::UpdateRightPanel() {
    if (!InRightPanel()) return;
    Rectangle or_ = OutlinerRect();
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();
    if (CheckCollisionPointRec(mouse, or_) && wheel != 0) {
        _outlineScroll = (int)(_outlineScroll - wheel);
        _outlineScroll = std::max(0, _outlineScroll);
    }
    Rectangle dr = DataPanelRect();
    if (CheckCollisionPointRec(mouse, dr) && wheel != 0) {
        _dataPanelScroll += (int)(-wheel * 20.f);
        _dataPanelScroll = std::max(0, _dataPanelScroll);
        int maxS = std::max(0, (int)(_dataPanelContentH - dr.height + 22.f));
        if (_dataPanelScroll > maxS) _dataPanelScroll = maxS;
    }
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouse, or_)) {
        int vis = (int)(or_.height - 20) / OUTLINE_ROW;
        for (int r = 0; r < vis; r++) {
            int idx = r + _outlineScroll;
            if (idx >= (int)_outline.size()) break;
            Rectangle rr = { or_.x + 2,or_.y + 18 + (float)r * OUTLINE_ROW,or_.width - 4,(float)OUTLINE_ROW };
            if (CheckCollisionPointRec(mouse, rr)) {
                SelectEnt(_outline[idx].ent);
                _tool = EditorTool::SELECT;
                SetStatus(TextFormat("Selected %s", _outline[idx].name));
                break;
            }
        }
    }
}

void LevelEditor::UpdateCanvas() {
    if (_directOp != DirectOp::NONE) { UpdateDirectOp(); return; }
    if (_camPanning) return;   // RMB drag pan is handled in master Update
    Vector2 wm = WorldMouse(), swm = Snap(wm);
    bool lP = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool lD = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool lR = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

    if (_connectMode != ConnectMode::NONE) {
        if (lP) {
            for (int i = 0; i < (int)_level.pathNodes.size(); i++) {
                Vector2 np = { _level.pathNodes[i].x,_level.pathNodes[i].y };
                if (CheckCollisionPointCircle(wm, np, 12.f)) {
                    PushUndo();
                    auto& src = _level.pathNodes[_connectFrom];
                    if (_connectMode == ConnectMode::NEXT0) src.next[0] = i;
                    else if (_connectMode == ConnectMode::NEXT1) src.next[1] = i;
                    else src.next[2] = i;
                    _connectMode = ConnectMode::NONE; _selEdge.clear();
                    SetStatus(TextFormat("Node %d connected.", i)); return;
                }
            }
        }
        return;
    }

    if (_tool == EditorTool::SELECT) {
        if (_sel.valid() && _gizmo != GizmoMode::SELECT) {
            UpdateGizmo();
            if (_gizmoDragging) return;
        }
        if (lP) {
            if (_sel.valid() && _gizmo != GizmoMode::SELECT) {
                Vector2 center = EntityCenter(_sel);
                GizmoAxis hit = GizmoHitTest(center, wm);
                if (hit != GizmoAxis::NONE) return;
            }
            // Edge click: check proximity to path node edge midpoints
            bool edgeHit = false;
            for (int ni = 0; ni < (int)_level.pathNodes.size() && !edgeHit; ni++) {
                const auto& pn = _level.pathNodes[ni];
                Vector2 from = { pn.x, pn.y };
                for (int k = 0; k < 3 && !edgeHit; k++) {
                    int nb = pn.next[k];
                    if (nb < 0 || nb >= (int)_level.pathNodes.size()) continue;
                    Vector2 to = { _level.pathNodes[nb].x, _level.pathNodes[nb].y };
                    Vector2 mid = Vector2Lerp(from, to, 0.5f);
                    if (CheckCollisionPointCircle(wm, mid, 10.f)) {
                        _selEdge = { ni, k };
                        _sel.clear(); _multiSel.clear();
                        edgeHit = true;
                    }
                }
            }
            if (edgeHit) return;

            bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            bool hit = PickEntity(wm);
            if (hit) {
                _selEdge.clear();
                if (shift) {
                    SelectedEnt clicked = _sel;
                    bool already = IsInMultiSel(clicked);
                    if (already) {
                        _multiSel.erase(std::remove_if(_multiSel.begin(), _multiSel.end(),
                            [&clicked](const SelectedEnt& e) {return e == clicked; }), _multiSel.end());
                        _sel = _multiSel.empty() ? SelectedEnt{} : _multiSel.back();
                    }
                    else {
                        if (_sel.valid() && !IsInMultiSel(_sel)) _multiSel.push_back(_sel);
                        _multiSel.push_back(clicked);
                        _sel = clicked;
                    }
                    SetStatus(TextFormat("%d selected.", (int)_multiSel.size()));
                }
                else if (_multiSel.size() > 1 && IsInMultiSel(_sel)) {
                    PushUndo(); _multiDragging = true; _multiDragAnchor = wm;
                    _multiDragOrigins.clear(); for (const auto& e : _multiSel)_multiDragOrigins.push_back(GetEntPos(e));
                }
                else {
                    _multiSel.clear(); _boxSelecting = false;
                    PushUndo(); _dragging = true; _dragOffset = Vector2Subtract(wm, GetSelPos());
                }
            }
            else {
                _selEdge.clear();
                if (shift) {
                    _boxSelecting = true; _boxStart = _boxEnd = wm;
                }
                else {
                    _sel.clear(); _multiSel.clear();
                    _dragging = false; _multiDragging = false;
                    _boxSelecting = true; _boxStart = _boxEnd = wm;
                }
            }
        }
        if (lD) {
            if (_dragging) SetSelPos(Snap(Vector2Subtract(wm, _dragOffset)));
            if (_multiDragging) {
                Vector2 d = Vector2Subtract(wm, _multiDragAnchor);
                for (int i = 0; i < (int)_multiSel.size(); i++) SetEntPos(_multiSel[i], Snap(Vector2Add(_multiDragOrigins[i], d)));
            }
            if (_boxSelecting) _boxEnd = wm;
        }
        if (lR) {
            if (_boxSelecting) {
                Rectangle box = { fminf(_boxStart.x,_boxEnd.x),fminf(_boxStart.y,_boxEnd.y),
                                fabsf(_boxEnd.x - _boxStart.x),fabsf(_boxEnd.y - _boxStart.y) };
                bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                if (box.width > 4 || box.height > 4) {
                    if (shift) {
                        std::vector<SelectedEnt> prev = _multiSel;
                        BoxSelectEntities(box);
                        for (const auto& e : prev)
                            if (!IsInMultiSel(e)) _multiSel.push_back(e);
                        if (!_multiSel.empty()) _sel = _multiSel[0];
                        SetStatus(TextFormat("%d selected.", (int)_multiSel.size()));
                    }
                    else {
                        BoxSelectEntities(box);
                    }
                }
                else {
                    if (!shift) { _sel.clear(); _multiSel.clear(); }
                }
                _boxSelecting = false;
            }
            _dragging = false; _multiDragging = false;
        }
        return;
    }

    if (_tool == EditorTool::PLATFORM) {
        if (lP) { _placingPlatform = true; _platStart = swm; }
        if (_placingPlatform && lR) {
            float w = swm.x - _platStart.x;
            if (fabsf(w) < GRID_SZ) w = (float)(GRID_SZ * 4);
            PushUndo();
            PlatformData p; p.x = (w >= 0) ? _platStart.x : swm.x; p.w = fabsf(w); p.y = _platStart.y; p.h = 0.f; p.tilt = 0.f;
            _level.platforms.push_back(p); _placingPlatform = false; SetStatus("Platform placed.");
        }
        return;
    }
    if (_tool == EditorTool::LADDER) {
        if (lP) { _placingLadder = true; _ladStart = swm; }
        if (_placingLadder && lR) {
            float h = swm.y - _ladStart.y;
            if (fabsf(h) < GRID_SZ) h = (float)(GRID_SZ * 4);
            PushUndo();
            LadderData l; l.x = _ladStart.x; l.y = (h >= 0) ? _ladStart.y : swm.y; l.w = 40.f; l.h = fabsf(h);
            _level.ladders.push_back(l); _placingLadder = false; SetStatus("Ladder placed.");
        }
        return;
    }
    if (_tool == EditorTool::ELEVATOR) {
        if (lP) { _placingLadder = true; _ladStart = swm; }
        if (_placingLadder && lR) {
            float h = swm.y - _ladStart.y;
            if (fabsf(h) < GRID_SZ * 2) h = (float)(GRID_SZ * 8);
            PushUndo();
            ElevatorData el;
            el.x = _ladStart.x; el.y = (h >= 0) ? _ladStart.y : swm.y;
            el.w = 48.f; el.h = fabsf(h); el.speed = 60.f; el.direction = 1;
            _level.elevators.push_back(el);
            _sel = { (int)EditorTool::ELEVATOR, (int)_level.elevators.size() - 1 };
            _placingLadder = false;
            SetStatus(TextFormat("Elevator placed (h=%.0f). Switch to SELECT to adjust.", el.h));
        }
        return;
    }
    if (_tool == EditorTool::WIN_ZONE) {
        if (lP) { _placingPlatform = true; _platStart = swm; }
        if (_placingPlatform && lR) {
            float w = swm.x - _platStart.x, h = swm.y - _platStart.y;
            if (fabsf(w) < GRID_SZ) w = 80.f;
            if (fabsf(h) < GRID_SZ) h = 80.f;
            PushUndo();
            _level.winZone.x = (w >= 0) ? _platStart.x : swm.x;
            _level.winZone.y = (h >= 0) ? _platStart.y : swm.y;
            _level.winZone.w = fabsf(w);
            _level.winZone.h = fabsf(h);
            _level.hasWinZone = true;
            _placingPlatform = false;
            _sel = { (int)EditorTool::WIN_ZONE, 0 };
            SetStatus("Win zone placed. Switch to SELECT to adjust.");
        }
        return;
    }
    if (_tool == EditorTool::KILL_ZONE) {
        if (lP) { _placingPlatform = true; _platStart = swm; }
        if (_placingPlatform && lR) {
            float w = swm.x - _platStart.x, h = swm.y - _platStart.y;
            if (fabsf(w) < GRID_SZ) w = 64.f;
            if (fabsf(h) < GRID_SZ) h = 64.f;
            PushUndo();
            KillZoneData kz;
            kz.x = (w >= 0) ? _platStart.x : swm.x;
            kz.y = (h >= 0) ? _platStart.y : swm.y;
            kz.w = fabsf(w); kz.h = fabsf(h);
            kz.texId = KillZoneTexture::NONE;
            _level.killZones.push_back(kz);
            _placingPlatform = false;
            _sel = { (int)EditorTool::KILL_ZONE, (int)_level.killZones.size() - 1 };
            SetStatus("Kill zone placed. Switch to SELECT to adjust.");
        }
        return;
    }
    if (_tool == EditorTool::CONVEYOR) {
        if (lP) {
            PushUndo();
            ConveyorData cv;
            cv.x = swm.x; cv.y = swm.y;
            _level.conveyors.push_back(cv);
            _sel = { (int)EditorTool::CONVEYOR, (int)_level.conveyors.size() - 1 };
            SetStatus("Conveyor placed. Adjust length/direction in properties.");
        }
        return;
    }
    if (_tool == EditorTool::POINT_LIGHT || _tool == EditorTool::SPOT_LIGHT ||
        _tool == EditorTool::SKY_LIGHT) {
        if (lP) {
            PushUndo();
            LightData L;
            L.x = swm.x; L.y = swm.y;
            if (_tool == EditorTool::SPOT_LIGHT) {
                L.type = LightType::SPOT;
                L.angle = 60.f;
                L.direction = 90.f;
                L.radius = 320.f;
            }
            else if (_tool == EditorTool::SKY_LIGHT) {
                L.type = LightType::SKY;
                L.direction = 270.f;
                L.radius = 1000.f;
                L.intensity = 0.6f;
                L.r = 0.7f; L.g = 0.85f; L.b = 1.0f;
            }
            else {
                L.type = LightType::POINT;
            }
            _level.lights.push_back(L);
            _sel = { (int)_tool, (int)_level.lights.size() - 1 };
            SetStatus("Light placed.");
        }
        return;
    }
    if (_tool == EditorTool::PATH_NODE) {
        if (lP) {
            PushUndo(); PathNodeData n; n.x = swm.x; n.y = swm.y;
            _level.pathNodes.push_back(n);
            _sel = { (int)EditorTool::PATH_NODE,(int)_level.pathNodes.size() - 1 };
            SetStatus(TextFormat("Node %d placed.", _sel.index));
        }
        return;
    }
    if (_tool == EditorTool::PROP) {
        if (lP) {
            PushUndo();
            PropData pr; pr.x = swm.x; pr.y = swm.y;
            _level.props.push_back(pr);
            _sel = { (int)EditorTool::PROP, (int)_level.props.size() - 1 };
            SetStatus("Prop placed. Adjust in Properties panel.");
        }
        return;
    }
    if (lP) {
        PushUndo();
        switch (_tool) {
        case EditorTool::PLAYER_SPAWN:   _level.hasPlayerSpawn = true; _level.playerSpawn = swm; SetStatus("Player spawn set."); break;
        case EditorTool::REGULUS:        _level.hasRegulus = true; _level.regulusPos = swm; SetStatus("Regulus set."); break;
        case EditorTool::CAVE:           _level.hasCave = true; _level.cavePos = swm; SetStatus("Cave set."); break;
        case EditorTool::BEAM:           _level.beams.push_back({ swm.x, swm.y }); SetStatus("Beam placed."); break;
        case EditorTool::NUKE_SPAWN:     _level.nukeSpawns.push_back(swm); SetStatus("Nuke spawn."); break;
        case EditorTool::BEATRICE_SPAWN: _level.beatriceSpawns.push_back(swm); SetStatus("Beatrice spawn."); break;
        case EditorTool::ENEMY_SPAWN:    _level.enemySpawns.push_back(swm); SetStatus("Enemy spawn."); break;
        default: break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Master Update
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::Update(float dt) {
    if (_statusTimer > 0.f) _statusTimer -= dt;
    BuildOutline();

    if (_fieldDrag && !IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        _fieldDrag = false; _fieldPtr = nullptr;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }
    // Cancel text typing if canvas is clicked (handled inside NumField too, but safety guard)
    if (_fieldTyping && InCanvas() && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        _fieldTyping = false; _fieldTypingPtr = nullptr;
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    if (_seqOpen) UpdateSequencer();
    if (_wantsMenu || _wantsPlay || _wantsEmote) return;
    if (_directOp != DirectOp::NONE) { UpdateDirectOp(); return; }

    if (IsKeyPressed(KEY_F8)) {
        _lightingPreview = !_lightingPreview;
        SetStatus(_lightingPreview ? "Lighting preview ON (F8)" : "Lighting preview OFF (F8)");
    }

    UpdateToolbar();
    if (_wantsMenu || _wantsPlay || _wantsEmote) return;

    UpdateBrowser();
    UpdateRightPanel();

    // ── Camera pan (RMB drag) & zoom (mouse wheel) ─────────────────────────────
    if (InCanvas()) {
        bool rP = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
        bool rR = IsMouseButtonReleased(MOUSE_RIGHT_BUTTON);
        Vector2 mp = GetMousePosition();
        if (rP) { _camPanning = true; _camPanStart = mp; _camTargetAtPanStart = _cam.target; _camPanMoved = false; }
        if (_camPanning && IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
            float dx = mp.x - _camPanStart.x, dy = mp.y - _camPanStart.y;
            if (!_camPanMoved && fabsf(dx) + fabsf(dy) > 4.f) _camPanMoved = true;
            if (_camPanMoved) { _cam.target.x = _camTargetAtPanStart.x - dx / _cam.zoom; _cam.target.y = _camTargetAtPanStart.y - dy / _cam.zoom; }
        }
        if (rR && _camPanning) {
            bool wasDrag = _camPanMoved; _camPanning = false; _camPanMoved = false;
            if (!wasDrag && _directOp == DirectOp::NONE) { Vector2 wm = GetScreenToWorld2D(mp, _cam); PickEntity(wm); if (_sel.valid()) DeleteSelected(); }
        }
        float whl = GetMouseWheelMove();
        if (whl != 0 && _directOp == DirectOp::NONE) {
            Vector2 worldBefore = GetScreenToWorld2D(mp, _cam);
            float f = (whl > 0) ? 1.12f : (1.f / 1.12f);
            _cam.zoom = Clamp(_cam.zoom * f, 0.08f, 8.f);
            Vector2 worldAfter = GetScreenToWorld2D(mp, _cam);
            _cam.target.x += worldBefore.x - worldAfter.x; _cam.target.y += worldBefore.y - worldAfter.y;
        }
    }

    if (InCanvas()) UpdateCanvas();
}

// =============================================================================
//  DRAW
// =============================================================================

void LevelEditor::DrawPlatEnt(const PlatformData& p, bool sel, bool msel) const {
    float h = (p.h > 0.f) ? p.h : 12.f, tr = p.tilt * DEG2RAD, yr = p.w * tanf(tr);
    Vector2 TL = { p.x,p.y }, TR = { p.x + p.w,p.y + yr }, BL = { p.x,p.y + h }, BR = { p.x + p.w,p.y + yr + h };
    Color fill = msel ? Color{ 255,200,0,35 } : (sel ? Color{ 255,220,0,28 } : Color{ 0,140,255,28 });
    DrawTriangle(TL, BL, TR, fill); DrawTriangle(BL, BR, TR, fill);
    Color bc = sel ? YELLOW : (msel ? Color{ 255,200,0,240 } : Color{ 0,160,255,200 });
    float lw = sel ? 2.5f : 1.5f;
    DrawLineEx(TL, TR, lw, bc); DrawLineEx(BL, BR, lw, bc); DrawLineEx(TL, BL, lw, bc); DrawLineEx(TR, BR, lw, bc);
    if (fabsf(p.tilt) > 0.1f) { char b[32]; snprintf(b, sizeof(b), "%.1f°", p.tilt); DrawText(b, (int)p.x + 2, (int)p.y - 12, 9, bc); }
}
void LevelEditor::DrawLadEnt(const LadderData& l, bool sel, bool msel) const {
    Rectangle r = LadRect(l);
    const float trim = 18.f;
    float climbH = r.height - trim;
    Color bc = sel ? YELLOW : (msel ? Color{ 255,200,0,240 } : Color{ 255,220,0,200 });
    if (_ladderTex && _ladderTex->id > 0) {
        const float sc = 4.f, tw = 16.f * sc, th = 16.f * sc;
        float dx = r.x + r.width * .5f - tw * .5f;
        for (float y = r.y; y < r.y + r.height; y += th) {
            float dh = fminf(th, r.y + r.height - y);
            DrawTexturePro(*_ladderTex, { 0,0,16.f,dh / sc }, { dx,y,tw,dh }, {}, 0.f, WHITE);
        }
    }
    else {
        DrawRectangleRec(r, { 255,220,0,50 });
    }
    DrawRectangleLinesEx({ r.x,r.y,r.width,climbH }, sel ? 2.f : 1.5f, bc);
    Rectangle trimR = { r.x,r.y + climbH,r.width,trim };
    DrawRectangleRec(trimR, { 0,0,0,80 });
    Color hc = { 255,220,0,60 };
    for (float ox = 0; ox < trimR.width + trimR.height; ox += 8.f) {
        float x1 = trimR.x + ox, y1 = trimR.y;
        float x2 = trimR.x + ox - trimR.height, y2 = trimR.y + trimR.height;
        x1 = fmaxf(x1, trimR.x); x2 = fmaxf(x2, trimR.x);
        x1 = fminf(x1, trimR.x + trimR.width); x2 = fminf(x2, trimR.x + trimR.width);
        DrawLineEx({ x1,y1 }, { x2,y2 }, 1.f, hc);
    }
    DrawRectangleLinesEx(trimR, 1.f, { 255,220,0,100 });
    DrawText("vis", (int)(trimR.x + 2), (int)(trimR.y + 2), 8, { 255,220,0,140 });
}
void LevelEditor::DrawCircEnt(Vector2 pos, float rad, Color c, bool sel, bool msel, const char* lbl) const {
    if (msel && !sel) DrawCircleV(pos, rad + 5.f, { 255,200,0,180 });
    DrawCircleV(pos, rad + (sel ? 3.f : 0.f), sel ? YELLOW : c);
    DrawCircleV(pos, rad, c);
    if (lbl) { int tw = MeasureText(lbl, 9); DrawText(lbl, (int)pos.x - tw / 2, (int)pos.y - 4, 9, BLACK); }
}
void LevelEditor::DrawBeamEnt(const BeamData& b, bool sel, bool msel) const {
    // Select the right texture: variant 1-12 first, then default, then placeholder
    Texture2D* tex = nullptr;
    if (b.texVariant >= 1 && b.texVariant <= 12 && _beamVariantTex[b.texVariant - 1] && _beamVariantTex[b.texVariant - 1]->id > 0)
        tex = _beamVariantTex[b.texVariant - 1];
    else if (_beamTex && _beamTex->id > 0)
        tex = _beamTex;

    if (tex) {
        float s = 4.f, w = tex->width * s, h = tex->height * s;
        // flipX: negative source width mirrors the texture horizontally
        float srcX = b.flipX ? (float)tex->width : 0.f;
        float srcW = b.flipX ? -(float)tex->width : (float)tex->width;
        DrawTexturePro(*tex, { srcX, 0, srcW, (float)tex->height }, { b.x, b.y, w, h }, {}, 0.f, WHITE);
        if (sel)            DrawRectangleLinesEx({ b.x, b.y, w, h }, 2.f, YELLOW);
        if (msel && !sel)   DrawRectangleLinesEx({ b.x, b.y, w, h }, 2.f, { 255,200,0,200 });
    }
    else {
        Rectangle r = BeamRect(b);
        DrawRectangleRec(r, { 120,120,120,80 });
        DrawRectangleLinesEx(r, 1.f, sel ? YELLOW : (msel ? Color{ 255,200,0,200 } : GRAY));
    }
}
void LevelEditor::DrawPlayerSpawn(Vector2 pos, bool sel, bool msel) const {
    if (_playerTex && _playerTex->id > 0) {
        // Match game draw: top-left at pos, y+10 offset, same scale as gameplay
        float s = 3.8f * 0.85f * 1.05f, w = _playerTex->width * s, h = _playerTex->height * s;
        Rectangle dest = { pos.x, pos.y + 10.f, w, h };
        DrawTexturePro(*_playerTex, { 0,0,(float)_playerTex->width,(float)_playerTex->height }, dest, {}, 0.f, (msel && !sel) ? Color{ 255,220,100,220 } : WHITE);
        if (sel || msel) DrawRectangleLinesEx(dest, 2.f, sel ? YELLOW : Color{ 255,200,0,200 });
    }
    else DrawCircEnt(pos, 12.f, GREEN, sel, msel, "P");
    DrawCircleV(pos, 4.f, sel ? YELLOW : GREEN);
    DrawText("SPAWN", (int)pos.x + 6, (int)pos.y - 4, 8, GREEN);
}
void LevelEditor::DrawRegulusEnt(Vector2 pos, bool sel, bool msel) const {
    if (_regulusTex && _regulusTex->id > 0) {
        // Match game draw: regX = pos.x + w*0.5f, regY = pos.y - h + 20
        float s = 3.5f * .7f * 1.2f, w = _regulusTex->width * s, h = _regulusTex->height * s;
        Rectangle dest = { pos.x + w * 0.5f, pos.y - h + 20.f, w, h };
        DrawTexturePro(*_regulusTex, { 0,0,(float)_regulusTex->width,(float)_regulusTex->height }, dest, {}, 0.f, (msel && !sel) ? Color{ 255,220,100,220 } : WHITE);
        if (sel || msel) DrawRectangleLinesEx(dest, 2.f, sel ? YELLOW : Color{ 255,200,0,200 });
    }
    else DrawCircEnt(pos, 16.f, { 160,32,240,255 }, sel, msel, "R");
    DrawCircleV(pos, 3.f, sel ? YELLOW : Color{ 160,32,240,255 });
}
void LevelEditor::DrawCaveEnt(Vector2 pos, bool sel, bool msel) const {
    if (!_level.caveVisible) {
        // Invisible cave: show spawn circle only
        Vector2 center = { pos.x + 112.f, pos.y + 56.f };
        float radius = 28.f;
        DrawCircleV(center, radius, { 255,165,0,40 });
        DrawCircleLinesV(center, radius, sel ? YELLOW : ORANGE);
        if (msel && !sel) DrawCircleLinesV(center, radius + 3.f, { 255,200,0,180 });
        int tw = MeasureText("SPAWN", 9);
        DrawText("SPAWN", (int)(center.x - tw * 0.5f), (int)(center.y - 5), 9, sel ? YELLOW : ORANGE);
        DrawText("(hidden)", (int)(center.x - 18), (int)(center.y + 6), 8, { 200,140,60,200 });
        return;
    }
    if (_caveTex && _caveTex->id > 0) {
        float w = 64.f * 3.5f, h = 32.f * 3.5f;
        DrawTexturePro(*_caveTex, { 0,0,64.f,32.f }, { pos.x,pos.y,w,h }, {}, 0.f, (msel && !sel) ? Color{ 255,220,100,220 } : WHITE);
        if (sel || msel) DrawRectangleLinesEx({ pos.x,pos.y,w,h }, 2.f, sel ? YELLOW : Color{ 255,200,0,200 });
    }
    else {
        Rectangle r = { pos.x,pos.y,224.f,112.f }; DrawRectangleRec(r, { 255,165,0,60 });
        DrawRectangleLinesEx(r, sel ? 2.f : 1.5f, sel ? YELLOW : ORANGE); DrawText("CAVE", (int)r.x + 4, (int)r.y + 18, 9, ORANGE);
    }
}
void LevelEditor::DrawPathNodes() {
    const auto& nodes = _level.pathNodes;
    // Per-slot edge colors: slot0=orange, slot1=skyblue, slot2=magenta
    static const Color EDGE_COL[3] = { {255,140,0,200},{0,200,255,200},{220,80,220,200} };
    static const Color EDGE_HOV[3] = { {255,200,0,255},{0,255,255,255},{255,120,255,255} };

    for (int i = 0; i < (int)nodes.size(); i++) {
        const auto& n = nodes[i]; Vector2 from = { n.x,n.y };
        for (int k = 0; k < 3; k++) {
            if (n.next[k] < 0 || n.next[k] >= (int)nodes.size()) continue;
            Vector2 to = { nodes[n.next[k]].x,nodes[n.next[k]].y };
            bool selEdge = (_selEdge.valid() && _selEdge.from == i && _selEdge.slot == k);
            Color lc = selEdge ? EDGE_HOV[k] : EDGE_COL[k];
            float lw = selEdge ? 4.f : 2.f;
            DrawLineEx(from, to, lw, lc);
            // Direction indicator at 65%
            DrawCircleV(Vector2Lerp(from, to, .65f), 3.f, lc);
            // Edge midpoint pick indicator (always, subtle; brighter when selected)
            Vector2 mid = Vector2Lerp(from, to, 0.5f);
            if (selEdge) {
                DrawCircleV(mid, 7.f, lc);
                // Show edge type label
                const char* etLabel = (n.edgeType[k] == 1) ? "LAD" : "NRM";
                DrawText(etLabel, (int)mid.x + 8, (int)mid.y - 5, 9, lc);
            } else {
                DrawCircleV(mid, 4.f, { lc.r,lc.g,lc.b,130 });
            }
        }
    }
    for (int i = 0; i < (int)nodes.size(); i++) {
        const auto& n = nodes[i];
        bool sel = (_sel.valid() && _sel.type == (int)EditorTool::PATH_NODE && _sel.index == i);
        bool ms = IsInMultiSel({ (int)EditorTool::PATH_NODE,i });
        // Terminal = all nexts -1
        bool terminal = (n.next[0] < 0 && n.next[1] < 0 && n.next[2] < 0);
        static const Color ENDER_COL[4] = {
            {0,200,220,255},   // 0=down   cyan
            {80,200,80,255},   // 1=right  lime
            {200,140,40,255},  // 2=left   amber
            {180,80,200,255},  // 3=none   purple
        };
        bool isEnder = terminal && n.enderDir >= 0 && n.enderDir <= 3;
        Color fill = (i == 0) ? WHITE
                   : isEnder  ? ENDER_COL[n.enderDir]
                   : terminal ? RED
                   : n.isSplitNode ? GREEN
                   : YELLOW;
        Vector2 pos = { n.x,n.y };
        if (ms && !sel) DrawCircleV(pos, 14.f, { 255,200,0,180 });
        if (sel) DrawCircleV(pos, 13.f, YELLOW);
        DrawCircleV(pos, 10.f, BLACK); DrawCircleV(pos, 8.f, fill);
        char lb[8]; snprintf(lb, sizeof(lb), "%d", i); int tw = MeasureText(lb, 8); DrawText(lb, (int)pos.x - tw / 2, (int)pos.y - 4, 8, BLACK);
    }
    if (_connectMode != ConnectMode::NONE && _connectFrom >= 0 && _connectFrom < (int)nodes.size()) {
        Vector2 src = { nodes[_connectFrom].x,nodes[_connectFrom].y };
        int slot = (_connectMode == ConnectMode::NEXT0) ? 0 : (_connectMode == ConnectMode::NEXT1) ? 1 : 2;
        DrawCircleLines((int)src.x, (int)src.y, 16, EDGE_COL[slot]);
        DrawLineEx(src, WorldMouse(), 1.5f, { EDGE_COL[slot].r,EDGE_COL[slot].g,EDGE_COL[slot].b,150 });
    }
}

void LevelEditor::DrawGizmo() const {
    if (!_sel.valid() || _gizmo == GizmoMode::SELECT || _directOp != DirectOp::NONE) return;
    Vector2 c = EntityCenter(_sel);
    const float R = GIZMO_R, tip = 9.f, cs = 8.f;
    Color ctrC = (_gizmoHot == GizmoAxis::FREE || _gizmoDragAxis == GizmoAxis::FREE) ? WHITE : Color{ 220,220,220,220 };
    DrawCircleV(c, cs, ctrC); DrawCircleLines((int)c.x, (int)c.y, (int)cs, { 60,60,60,255 });
    if (_gizmo == GizmoMode::ROTATE) {
        Color rc = (_gizmoHot == GizmoAxis::RING || _gizmoDragAxis == GizmoAxis::RING) ? WHITE : Color{ 220,150,0,220 };
        DrawCircleLines((int)c.x, (int)c.y, (int)R, rc);
        if (_sel.type == (int)EditorTool::PLATFORM) {
            float tilt = _level.platforms[_sel.index].tilt * DEG2RAD;
            Vector2 tip2 = { c.x + R * cosf(tilt),c.y + R * sinf(tilt) };
            DrawLineEx(c, tip2, 3.f, rc); DrawCircleV(tip2, tip, rc);
        }
        if (_gizmoDragging) DrawText("←drag→ = tilt", (int)c.x + 4, (int)c.y - (int)R - 14, 10, { 220,150,0,255 });
        return;
    }
    bool mv = (_gizmo == GizmoMode::MOVE);
    Vector2 xTip = { c.x + R,c.y };
    Color xc = (_gizmoHot == GizmoAxis::X || _gizmoDragAxis == GizmoAxis::X) ? WHITE : Color{ 230,60,60,255 };
    DrawLineEx(c, xTip, 3.f, xc);
    if (mv) DrawTriangle({ xTip.x - 9,xTip.y - 5 }, { xTip.x - 9,xTip.y + 5 }, { xTip.x + tip,xTip.y }, xc);
    else   DrawRectangle((int)(xTip.x - tip * .5f), (int)(xTip.y - tip * .5f), (int)tip, (int)tip, xc);
    DrawText("X", (int)xTip.x + tip + 2, (int)xTip.y - 6, 10, xc);
    Vector2 yTip = { c.x,c.y - R };
    Color yc = (_gizmoHot == GizmoAxis::Y || _gizmoDragAxis == GizmoAxis::Y) ? WHITE : Color{ 60,210,60,255 };
    DrawLineEx(c, yTip, 3.f, yc);
    if (mv) DrawTriangle({ yTip.x - 5,yTip.y + 9 }, { yTip.x + 5,yTip.y + 9 }, { yTip.x,yTip.y - tip }, yc);
    else   DrawRectangle((int)(yTip.x - tip * .5f), (int)(yTip.y - tip), (int)tip, (int)tip, yc);
    DrawText("Y", (int)yTip.x + tip, (int)yTip.y - 8, 10, yc);
    const char* ml = mv ? "W: MOVE" : "R: SCALE";
    DrawText(ml, (int)c.x + 4, (int)c.y - (int)R - 14, 10, mv ? Color{ 230,60,60,255 } : Color{ 60,210,60,255 });
}

void LevelEditor::DrawBackground() const {
    if (_bgTex && _bgTex->id > 0) {
        DrawTexturePro(*_bgTex, { 0,0,(float)_bgTex->width,(float)_bgTex->height }, { 0,0,(float)_sw,(float)_sh }, {}, 0.f, WHITE);
        DrawRectangle(0, 0, _sw, _sh, { 0,0,0,55 });
    }
    else DrawRectangle(0, 0, _sw, _sh, { 20,22,30,255 });
    DrawRectangleLinesEx({ 0,0,(float)_sw,(float)_sh }, 2.f, { 60,60,80,200 });
}
void LevelEditor::DrawGrid() const {
    if (!_gridOn) return;
    float gs = (float)GRID_SZ / _gridDiv;
    for (float x = 0; x <= (float)_sw; x += gs) { bool maj = (fmodf(x, (float)GRID_SZ) < .5f); DrawLine((int)x, 0, (int)x, _sh, maj ? Color{ 55,60,80,255 } : Color{ 35,38,52,255 }); }
    for (float y = 0; y <= (float)_sh; y += gs) { bool maj = (fmodf(y, (float)GRID_SZ) < .5f); DrawLine(0, (int)y, _sw, (int)y, maj ? Color{ 55,60,80,255 } : Color{ 35,38,52,255 }); }
}
void LevelEditor::DrawLevelEntities() {
    // Backwards-compat — calls both halves (used when lighting preview off).
    DrawLevelLitContent();
    DrawLevelOverlays();
}

void LevelEditor::DrawLevelLitContent() {
    auto IS = [&](EditorTool t, int i) { return _sel.valid() && _sel.type == (int)t && _sel.index == i; };
    auto IMS = [&](EditorTool t, int i) { return IsInMultiSel({ (int)t, i }); };

    // Elevators and conveyors first (always underneath everything)
    for (int i = 0; i < (int)_level.elevators.size(); i++)  DrawElevatorEnt(_level.elevators[i], IS(EditorTool::ELEVATOR, i), IMS(EditorTool::ELEVATOR, i));
    for (int i = 0; i < (int)_level.conveyors.size(); i++)  DrawConveyorEnt(_level.conveyors[i], IS(EditorTool::CONVEYOR, i), IMS(EditorTool::CONVEYOR, i));
    for (int i = 0; i < (int)_level.platforms.size(); i++)  DrawPlatEnt(_level.platforms[i], IS(EditorTool::PLATFORM, i), IMS(EditorTool::PLATFORM, i));
    for (int i = 0; i < (int)_level.ladders.size(); i++)    DrawLadEnt(_level.ladders[i], IS(EditorTool::LADDER, i), IMS(EditorTool::LADDER, i));

    // Props (drawn before beams so they can be behind them at layer 0)
    for (int i = 0; i < (int)_level.props.size(); i++)
        DrawPropEnt(_level.props[i], IS(EditorTool::PROP, i), IMS(EditorTool::PROP, i));

    // ── Layer-sorted beam + kill-zone drawing ────────────────────────────────
    struct LayerItem { int layer; int kind; int idx; };
    std::vector<LayerItem> items;
    items.reserve(_level.beams.size() + _level.killZones.size());
    for (int i = 0; i < (int)_level.beams.size(); i++) items.push_back({ _level.beams[i].renderLayer,     0, i });
    for (int i = 0; i < (int)_level.killZones.size(); i++) items.push_back({ _level.killZones[i].renderLayer, 1, i });
    std::stable_sort(items.begin(), items.end(), [](const LayerItem& a, const LayerItem& b) { return a.layer < b.layer; });
    for (const auto& it : items) {
        if (it.kind == 0) DrawBeamEnt(_level.beams[it.idx], IS(EditorTool::BEAM, it.idx), IMS(EditorTool::BEAM, it.idx));
        else              DrawKillZoneEnt(_level.killZones[it.idx], IS(EditorTool::KILL_ZONE, it.idx), IMS(EditorTool::KILL_ZONE, it.idx));
    }
}

void LevelEditor::DrawLevelOverlays() {
    auto IS = [&](EditorTool t, int i) { return _sel.valid() && _sel.type == (int)t && _sel.index == i; };
    auto IMS = [&](EditorTool t, int i) { return IsInMultiSel({ (int)t, i }); };

    // Game display boundary — red outline showing the 875×950 window area
    DrawRectangleLinesEx({ 0.f, 0.f, 875.f, 950.f }, 2.f / _cam.zoom, { 220, 40, 40, 200 });

    // Win zone outline (just an editor marker, not a real visible object)
    if (_level.hasWinZone) DrawWinZoneEnt(_level.winZone, IS(EditorTool::WIN_ZONE, 0), IMS(EditorTool::WIN_ZONE, 0));

    DrawPathNodes();
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++) DrawCircEnt(_level.nukeSpawns[i], 10.f, SKYBLUE, IS(EditorTool::NUKE_SPAWN, i), IMS(EditorTool::NUKE_SPAWN, i), "N");
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++) DrawCircEnt(_level.beatriceSpawns[i], 10.f, MAGENTA, IS(EditorTool::BEATRICE_SPAWN, i), IMS(EditorTool::BEATRICE_SPAWN, i), "B");
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++) DrawCircEnt(_level.enemySpawns[i], 10.f, RED, IS(EditorTool::ENEMY_SPAWN, i), IMS(EditorTool::ENEMY_SPAWN, i), "E");
    if (_level.hasPlayerSpawn) DrawPlayerSpawn(_level.playerSpawn, IS(EditorTool::PLAYER_SPAWN, 0), IMS(EditorTool::PLAYER_SPAWN, 0));
    if (_level.hasRegulus)     DrawRegulusEnt(_level.regulusPos, IS(EditorTool::REGULUS, 0), IMS(EditorTool::REGULUS, 0));
    if (_level.hasCave)        DrawCaveEnt(_level.cavePos, IS(EditorTool::CAVE, 0), IMS(EditorTool::CAVE, 0));

    // Light icons / gizmos
    for (int i = 0; i < (int)_level.lights.size(); i++) {
        EditorTool t = (_level.lights[i].type == LightType::POINT) ? EditorTool::POINT_LIGHT
            : (_level.lights[i].type == LightType::SPOT) ? EditorTool::SPOT_LIGHT
            : EditorTool::SKY_LIGHT;
        DrawLightEnt(_level.lights[i], i, IS(t, i), IMS(t, i));
    }

    if (_tool == EditorTool::SELECT) DrawGizmo();
}

void LevelEditor::DrawLightEnt(const LightData& L, int idx, bool sel, bool msel) const
{
    (void)idx;
    Color outline = { 255, 235, 120, 255 };
    if (L.type == LightType::SPOT) outline = { 200, 255, 180, 255 };
    if (L.type == LightType::SKY)  outline = { 130, 200, 255, 255 };

    Vector2 c = { L.x, L.y };
    Color body = { (unsigned char)(L.r * 255),
                   (unsigned char)(L.g * 255),
                   (unsigned char)(L.b * 255), 255 };

    // Outer reach circle (faint)
    DrawCircleLines((int)c.x, (int)c.y, L.radius, { outline.r, outline.g, outline.b, 60 });
    // Inner radius circle — shows flat-intensity zone
    if (L.innerRadius > 2.f)
        DrawCircleLines((int)c.x, (int)c.y, L.innerRadius, { outline.r, outline.g, outline.b, 130 });

    if (L.type == LightType::SPOT) {
        float dir = L.direction * (PI / 180.f);
        float half = (L.angle * 0.5f) * (PI / 180.f);
        Vector2 a = { c.x + cosf(dir - half) * L.radius,
                      c.y + sinf(dir - half) * L.radius };
        Vector2 b = { c.x + cosf(dir + half) * L.radius,
                      c.y + sinf(dir + half) * L.radius };
        DrawLineEx(c, a, 1.5f, { outline.r, outline.g, outline.b, 140 });
        DrawLineEx(c, b, 1.5f, { outline.r, outline.g, outline.b, 140 });
    }
    if (L.type == LightType::SKY) {
        float dir = L.direction * (PI / 180.f);
        Vector2 tip = { c.x - cosf(dir) * 30.f, c.y - sinf(dir) * 30.f };
        DrawLineEx(c, tip, 2.f, outline);
        DrawCircleV(tip, 3.f, outline);
    }

    DrawCircleV(c, 8.f, body);
    DrawCircleLines((int)c.x, (int)c.y, 8.f, outline);

    if (L.intensity > 0.1f) {
        unsigned char a = (unsigned char)fminf(120.f, L.intensity * 80.f);
        DrawCircleV(c, 14.f, { body.r, body.g, body.b, a });
    }

    if (sel)  DrawCircleLines((int)c.x, (int)c.y, 12.f, WHITE);
    if (msel) DrawCircleLines((int)c.x, (int)c.y, 11.f, ORANGE);

    const char* lbl = (L.type == LightType::POINT) ? "P"
        : (L.type == LightType::SPOT) ? "S" : "Y";
    DrawText(lbl, (int)c.x - 3, (int)c.y - 5, 10, BLACK);

    // Disabled cross
    if (!L.enabled) {
        DrawLineEx({ c.x - 10, c.y - 10 }, { c.x + 10, c.y + 10 }, 2.f, { 200, 60, 60, 220 });
        DrawLineEx({ c.x - 10, c.y + 10 }, { c.x + 10, c.y - 10 }, 2.f, { 200, 60, 60, 220 });
    }
}
void LevelEditor::DrawPlacementPreview() const {
    Vector2 wm = WorldMouse(), swm = Snap(wm);
    if (_boxSelecting) {
        float x = fminf(_boxStart.x, _boxEnd.x), y = fminf(_boxStart.y, _boxEnd.y);
        DrawRectangle((int)x, (int)y, (int)fabsf(_boxEnd.x - _boxStart.x), (int)fabsf(_boxEnd.y - _boxStart.y), { 0,200,255,18 });
        DrawRectangleLinesEx({ x,y,fabsf(_boxEnd.x - _boxStart.x),fabsf(_boxEnd.y - _boxStart.y) }, 1.5f, { 0,200,255,220 });
    }
    if (_placingPlatform) {
        float w = swm.x - _platStart.x, h = swm.y - _platStart.y;
        if (_tool == EditorTool::PLATFORM) {
            Rectangle r; r.y = _platStart.y; r.height = 12.f;
            r.x = (w >= 0) ? _platStart.x : swm.x; r.width = fabsf(w) > 0 ? fabsf(w) : 8.f;
            DrawRectangleLinesEx(r, 1.5f, { 80,120,255,180 });
            DrawText(TextFormat("w=%.0f", fabsf(w)), (int)r.x, (int)r.y - 14, 10, { 80,120,255,220 });
        }
        else if (_tool == EditorTool::WIN_ZONE) {
            Rectangle r = { (w >= 0) ? _platStart.x : swm.x, (h >= 0) ? _platStart.y : swm.y, fabsf(w) > 0 ? fabsf(w) : 8.f, fabsf(h) > 0 ? fabsf(h) : 8.f };
            DrawRectangleLinesEx(r, 1.5f, { 80,255,140,180 });
            DrawText(TextFormat("WIN  %.0fx%.0f", r.width, r.height), (int)r.x, (int)r.y - 14, 10, { 80,255,140,220 });
        }
        else if (_tool == EditorTool::KILL_ZONE) {
            Rectangle r = { (w >= 0) ? _platStart.x : swm.x, (h >= 0) ? _platStart.y : swm.y, fabsf(w) > 0 ? fabsf(w) : 8.f, fabsf(h) > 0 ? fabsf(h) : 8.f };
            DrawRectangleLinesEx(r, 1.5f, { 255,60,60,180 });
            DrawText(TextFormat("KILL  %.0fx%.0f", r.width, r.height), (int)r.x, (int)r.y - 14, 10, { 255,60,60,220 });
        }
    }
    if (_placingLadder) {
        float h = swm.y - _ladStart.y;
        Rectangle r = { _ladStart.x,(h >= 0) ? _ladStart.y : swm.y,40.f,fabsf(h) > 0 ? fabsf(h) : 8.f };
        DrawRectangleLinesEx(r, 1.5f, { 255,220,0,180 });
        DrawText(TextFormat("h=%.0f", fabsf(h)), (int)r.x, (int)r.y - 14, 10, { 255,220,0,220 });
    }
    if (_directOp != DirectOp::NONE) {
        const char* opLabel[] = { "","G MOVE","R ROTATE","S SCALE" };
        const char* axLbl = _grabAxisX ? " [X]" : _grabAxisY ? " [Y]" : "";
        DrawText(TextFormat("%s%s", opLabel[(int)_directOp], axLbl), 10, TOOLBAR_H + 6, 14, { 255,220,0,255 });
        DrawCircleLines((int)wm.x, (int)wm.y, 12, YELLOW);
        DrawLineEx(_grabMouseStart, wm, 1.5f, { 255,220,0,150 });
    }
    DrawLine((int)swm.x - 8, (int)swm.y, (int)swm.x + 8, (int)swm.y, { 255,255,255,80 });
    DrawLine((int)swm.x, (int)swm.y - 8, (int)swm.x, (int)swm.y + 8, { 255,255,255,80 });
}

void LevelEditor::DrawToolbarUI() const {
    DrawRectangle(0, 0, _sw, TOOLBAR_H, { 30,32,42,255 });
    DrawLine(0, TOOLBAR_H - 1, _sw, TOOLBAR_H - 1, { 70,80,110,255 });
    float bw = (float)_sw / 8.f;
    auto TBtn = [&](int c)->Rectangle {return{ c * bw + 1,1,bw - 2,(float)TOOLBAR_H - 2 }; };
    auto TB = [&](int c, const char* l, Color bg, Color fg) {
        Rectangle r = TBtn(c); DrawRectangleRec(r, bg); DrawRectangleLinesEx(r, 1, { 80,90,120,255 });
        int tw = MeasureText(l, 12); DrawText(l, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - 6), 12, fg);
        };
    TB(0, "<", { 40,42,55,255 }, WHITE); TB(1, TextFormat("Lv%d", _levelId), { 50,55,75,255 }, YELLOW);
    TB(2, ">", { 40,42,55,255 }, WHITE); TB(3, _gridOn ? "H:Grid ON" : "H:Grid OFF", { 40,42,55,255 }, _gridOn ? GREEN : GRAY);
    TB(4, TextFormat("÷%d", _gridDiv), { 40,42,55,255 }, { 100,200,255,255 });
    TB(5, "^S Save", { 30,80,50,255 }, WHITE); TB(6, "PLAY", { 30,60,100,255 }, WHITE); TB(7, "B: EMOTE", { 60,30,80,255 }, WHITE);
    const char* gn[] = { "1/Q:SEL","2/W:MOV","3/E:ROT","4:SCL" };
    Color gc[] = { LIGHTGRAY,{230,60,60,255},{220,150,0,255},{60,210,60,255} };
    float gx = 4.f;
    for (int g = 0; g < 4; g++) {
        bool act = (_gizmo == (GizmoMode)g);
        DrawRectangle((int)gx, TOOLBAR_H + 1, 54, 11, act ? Color{ (unsigned char)(gc[g].r / 2),(unsigned char)(gc[g].g / 2),(unsigned char)(gc[g].b / 2),255 } : Color{ 20,22,32,255 });
        int tw = MeasureText(gn[g], 9); DrawText(gn[g], (int)(gx + 27 - tw / 2), TOOLBAR_H + 2, 9, act ? gc[g] : Color{ 90,95,110,255 });
        gx += 57.f;
    }
}
void LevelEditor::DrawBrowserUI() {
    int by0 = _sh - BROWSER_H;
    DrawRectangle(0, by0, _canvasW, BROWSER_H, { 24,26,36,255 });
    DrawLine(0, by0, _canvasW, by0, { 70,80,110,255 });
    const EditorTool r0[] = { EditorTool::SELECT,EditorTool::PLAYER_SPAWN,EditorTool::REGULUS,EditorTool::CAVE,EditorTool::PLATFORM,EditorTool::LADDER };
    const EditorTool r1[] = { EditorTool::BEAM,EditorTool::PATH_NODE,EditorTool::NUKE_SPAWN,EditorTool::BEATRICE_SPAWN,EditorTool::ENEMY_SPAWN,EditorTool::ELEVATOR };
    const EditorTool r2[] = { EditorTool::WIN_ZONE, EditorTool::KILL_ZONE, EditorTool::CONVEYOR,
                              EditorTool::POINT_LIGHT, EditorTool::SPOT_LIGHT, EditorTool::SKY_LIGHT,
                              EditorTool::PROP };
    auto DT = [&](int row, int col, int cols, EditorTool t) {
        Rectangle r = BrowserBtn(row, col, cols); bool act = (_tool == t); Color tc = ToolColor(t);
        DrawRectangleRec(r, act ? Color{ (unsigned char)(tc.r / 3),(unsigned char)(tc.g / 3),(unsigned char)(tc.b / 3),255 } : Color{ 30,32,44,255 });
        DrawRectangleLinesEx(r, act ? 2.f : 1.f, act ? tc : Color{ 60,65,85,255 });
        const char* nm = ToolName(t); int tw = MeasureText(nm, 10);
        DrawText(nm, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - 5), 10, act ? tc : Color{ 170,175,190,255 });
        };
    for (int c = 0; c < 6; c++) DT(0, c, 6, r0[c]);
    for (int c = 0; c < 6; c++) DT(1, c, 6, r1[c]);
    for (int c = 0; c < 7; c++) DT(2, c, 7, r2[c]);
    float sy = (float)(_sh - 18);
    DrawRectangle(0, (int)sy - 2, _canvasW, 20, { 18,20,28,255 });
    const char* smsg = _statusTimer > 0.f ? _status : "1=Sel 2=Mov 3=Rot 4=Scl | G=Grab R=Rot S=Scale | ^C=Copy ^V=Paste ^D=Dup | H=Grid [/]=Grid÷ ^Z/Y DEL ^S B=Emote N=Menu Tab=Seq";
    DrawText(smsg, 6, (int)sy, 10, _statusTimer > 0.f ? YELLOW : Color{ 90,95,110,255 });
}

void LevelEditor::DrawOutliner() {
    Rectangle or_ = OutlinerRect();
    DrawRectangleRec(or_, { 22,24,34,255 });
    DrawRectangleLinesEx(or_, 1, { 55,60,85,255 });
    DrawRectangle((int)or_.x, (int)or_.y, (int)or_.width, 18, { 30,34,50,255 });
    DrawText("OUTLINER", (int)or_.x + 6, (int)or_.y + 3, 11, { 180,185,210,255 });
    DrawText(TextFormat("%d", (int)_outline.size()), (int)or_.x + 80, (int)or_.y + 4, 9, { 100,105,130,255 });

    if (_outlParentPick) {
        DrawRectangle((int)or_.x, (int)or_.y, (int)or_.width, 18, { 60,20,10,255 });
        DrawText("Click row = set as parent  (ESC=cancel)", (int)or_.x + 4, (int)or_.y + 3, 9, ORANGE);
    }

    int vis = (int)(or_.height - 20) / OUTLINE_ROW;
    Vector2 mp = GetMousePosition();

    if (CheckCollisionPointRec(mp, or_)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            _outlineScroll = std::max(0, _outlineScroll - (int)wheel);
            int maxS = std::max(0, (int)_outline.size() - vis);
            if (_outlineScroll > maxS) _outlineScroll = maxS;
        }
    }

    for (int r = 0; r < vis; r++) {
        int idx = r + _outlineScroll;
        if (idx >= (int)_outline.size()) break;
        const OutlineRow& row = _outline[idx];
        bool isSel = (row.ent == _sel || IsInMultiSel(row.ent));
        bool isCtx = (_outlCtxMenu && _outlCtxRow == idx);
        bool isDrop = (_outlDragging && _outlDropRow == idx);
        float indent = row.depth * 12.f;
        Rectangle rowR = { or_.x + 2, or_.y + 18 + (float)r * OUTLINE_ROW, or_.width - 4, (float)OUTLINE_ROW };
        bool hov = CheckCollisionPointRec(mp, rowR);

        Color rowBg = isSel ? Color{ 45,55,90,255 }
            : isCtx ? Color{ 60,40,20,255 }
            : isDrop ? Color{ 20,60,20,255 }
            : hov ? Color{ 35,38,55,255 }
        : Color{ 22,24,34,255 };
        float ry = or_.y + 18 + r * OUTLINE_ROW;
        DrawRectangle((int)or_.x + 2, (int)ry, (int)or_.width - 4, OUTLINE_ROW - 1, rowBg);

        if (row.depth > 0) {
            float lx = or_.x + 4 + indent - 8;
            DrawLine((int)lx, (int)(ry + 1), (int)lx, (int)(ry + OUTLINE_ROW * 0.6f), { 60,65,90,255 });
            DrawLine((int)lx, (int)(ry + OUTLINE_ROW * 0.6f), (int)(lx + 7), (int)(ry + OUTLINE_ROW * 0.6f), { 60,65,90,255 });
        }

        DrawRectangle((int)(or_.x + 2 + indent), (int)ry, 3, OUTLINE_ROW - 1, row.color);
        DrawText(row.icon, (int)(or_.x + 6 + indent), (int)ry + 3, 9, row.color);
        DrawText(row.name, (int)(or_.x + 22 + indent), (int)ry + 3, 9, isSel ? WHITE : Color{ 170,175,200,255 });

        if (row.hasChildren)
            DrawText(">", (int)(or_.x + or_.width - 22), (int)ry + 3, 9, { 120,130,160,255 });
        if (GetParent(row.ent).valid())
            DrawText("c", (int)(or_.x + or_.width - 14), (int)ry + 3, 9, { 255,180,80,200 });
        if (isSel)
            DrawText("●", (int)(or_.x + or_.width - 8), (int)ry + 3, 9, YELLOW);

        if (hov && IsMouseButtonPressed(MOUSE_RIGHT_BUTTON)) {
            _outlCtxMenu = true; _outlCtxRow = idx;
        }

        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (_outlParentPick && !(row.ent == _outlPickTarget)) {
                SetRelationParent(_outlPickTarget, row.ent);
                _outlParentPick = false;
            }
            else {
                SelectEnt(row.ent);
            }
        }

        if (hov && IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !_outlParentPick) {
            static int dragStartRow = -1;
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) dragStartRow = idx;
            if (dragStartRow >= 0 && dragStartRow != idx &&
                fabsf(mp.x - or_.x) < or_.width) {
                _outlDragging = true; _outlDragRow = dragStartRow; _outlDropRow = idx;
            }
        }
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && _outlDragging) {
            if (_outlDragRow >= 0 && _outlDragRow < (int)_outline.size() &&
                _outlDropRow >= 0 && _outlDropRow < (int)_outline.size() &&
                _outlDragRow != _outlDropRow) {
                SetRelationParent(_outline[_outlDragRow].ent, _outline[_outlDropRow].ent);
            }
            _outlDragging = false; _outlDragRow = -1; _outlDropRow = -1;
        }
    }

    if (_outlCtxMenu && _outlCtxRow >= 0 && _outlCtxRow < (int)_outline.size()) {
        SelectedEnt ctxEnt = _outline[_outlCtxRow].ent;
        float mx = or_.x + 2, my = or_.y + 18 + (_outlCtxRow - _outlineScroll) * OUTLINE_ROW;
        Rectangle cm = { mx + (float)or_.width * 0.4f, my, 130, 46 };
        DrawRectangleRec(cm, { 25,28,42,255 });
        DrawRectangleLinesEx(cm, 1, { 80,85,120,255 });

        Rectangle b1 = { cm.x + 2, cm.y + 2, cm.width - 4, 20 };
        bool h1 = CheckCollisionPointRec(mp, b1);
        DrawRectangleRec(b1, h1 ? Color{ 45,50,80,255 } : Color{ 32,35,55,255 });
        DrawText("Attach as child of...", (int)b1.x + 4, (int)b1.y + 4, 9, WHITE);

        Rectangle b2 = { cm.x + 2, cm.y + 24, cm.width - 4, 20 };
        bool h2 = CheckCollisionPointRec(mp, b2);
        DrawRectangleRec(b2, h2 ? Color{ 45,50,80,255 } : Color{ 32,35,55,255 });
        DrawText("Detach from parent", (int)b2.x + 4, (int)b2.y + 4, 9,
            GetParent(ctxEnt).valid() ? WHITE : Color{ 80,85,110,255 });

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (h1) { _outlParentPick = true; _outlPickTarget = ctxEnt; _outlCtxMenu = false; SetStatus("Click the parent entity in the outliner."); }
            else if (h2 && GetParent(ctxEnt).valid()) { RemoveRelation(ctxEnt); _outlCtxMenu = false; }
            else _outlCtxMenu = false;
        }
        if (IsKeyPressed(KEY_ESCAPE)) _outlCtxMenu = false;
    }

    if ((int)_outline.size() > vis) {
        float frac = (float)vis / _outline.size();
        float pos2 = (float)_outlineScroll / _outline.size();
        float barH = or_.height * frac;
        float barY = or_.y + 18 + (or_.height - 18) * pos2;
        DrawRectangle((int)(or_.x + or_.width - 4), (int)barY, 3, (int)barH, { 80,90,130,255 });
    }
}

void LevelEditor::DrawDataPanel() {
    Rectangle dr = DataPanelRect();
    DrawRectangleRec(dr, { 20,22,32,255 });
    DrawRectangleLinesEx(dr, 1, { 55,60,85,255 });
    DrawRectangle((int)dr.x, (int)dr.y, (int)dr.width, 18, { 28,32,48,255 });
    DrawText("PROPERTIES", (int)dr.x + 6, (int)dr.y + 3, 11, { 180,185,210,255 });
    BeginScissorMode((int)dr.x, (int)(dr.y + 18), (int)dr.width, (int)(dr.height - 18));
    if (!_sel.valid() && !_selEdge.valid()) {
        float px = dr.x + 6, fw = dr.width - 12;
        float cy = dr.y + 22 - (float)_dataPanelScroll;
        float rowH = 20.f;
        auto SH = [&](const char* t) {
            DrawLine((int)px, (int)cy, (int)(px + fw), (int)cy, { 60,65,90,255 });
            DrawText(t, (int)px, (int)cy + 2, 10, { 130,140,170,255 }); cy += 14;
        };
        auto NumFld = [&](const char* label, float& val, float step, float mn, float mx) -> bool {
            char buf[32]; snprintf(buf, sizeof(buf), "%.0f", val);
            int lw = MeasureText(label, 9), bw2 = (int)(fw - lw - 4);
            Rectangle r = { px + lw + 4, cy, (float)bw2, 15 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            DrawText(label, (int)px, (int)cy + 3, 9, { 160,165,190,255 });
            DrawRectangleRec(r, hov ? Color{45,48,68,255} : Color{28,32,48,255});
            DrawRectangleLinesEx(r, 1, {70,75,100,255});
            int tw = MeasureText(buf, 9);
            DrawText(buf, (int)(r.x + r.width/2 - tw/2), (int)r.y + 3, 9, WHITE);
            bool changed = false;
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                if (GetMouseX() < r.x + r.width/2) { val = Clamp(val - step, mn, mx); changed = true; }
                else { val = Clamp(val + step, mn, mx); changed = true; }
            }
            return changed;
        };
        SH("── Level Range ─────────────");
        DrawText("Random level pool:", (int)px, (int)cy, 9, { 150,155,180,255 }); cy += 12;
        float fMin = (float)_levelRangeMin, fMax = (float)_levelRangeMax;
        if (NumFld("Min", fMin, 1.f, 1.f, 20.f)) { _levelRangeMin = (int)fMin; SaveGameSettings(_levelRangeMin, _levelRangeMax, _volMusic, _volSFX, _volAbility, _volUI, _volAmbient, _highScore, _highLevels); }
        cy += rowH;
        if (NumFld("Max", fMax, 1.f, 1.f, 20.f)) { _levelRangeMax = (int)fMax; SaveGameSettings(_levelRangeMin, _levelRangeMax, _volMusic, _volSFX, _volAbility, _volUI, _volAmbient, _highScore, _highLevels); }
        cy += rowH;
        DrawText(TextFormat("Levels %d - %d in pool", _levelRangeMin, _levelRangeMax), (int)px, (int)cy, 9, { 100,200,120,255 });
        _dataPanelContentH = cy - (dr.y + 22 - (float)_dataPanelScroll) + (float)_dataPanelScroll;
        EndScissorMode();
        return;
    }
    float px = dr.x + 6, fw = dr.width - 12;
    float cy = dr.y + 22 - (float)_dataPanelScroll;
    float rowH = 20.f;
    auto SectionHeader = [&](const char* title) {
        DrawLine((int)px, (int)cy, (int)(px + fw), (int)cy, { 60,65,90,255 });
        DrawText(title, (int)px, (int)cy + 2, 10, { 130,140,170,255 });
        cy += 14;
        };
    // Propagate a single changed field to all other multi-selected entities of the same type
    auto syncField = [&](EditorTool tool, auto setter) {
        if (_multiSel.size() <= 1) return;
        for (const auto& e : _multiSel) {
            if (e.type != (int)tool || e.index == _sel.index) continue;
            setter(e.index);
        }
    };
    if (_sel.valid()) {
    SectionHeader("── Transform ─────────────");
    float& refX = (_sel.type == (int)EditorTool::PLATFORM) ? _level.platforms[_sel.index].x
        : (_sel.type == (int)EditorTool::LADDER) ? _level.ladders[_sel.index].x
        : (_sel.type == (int)EditorTool::BEAM) ? _level.beams[_sel.index].x
        : (_sel.type == (int)EditorTool::PATH_NODE) ? _level.pathNodes[_sel.index].x
        : (_sel.type == (int)EditorTool::NUKE_SPAWN) ? _level.nukeSpawns[_sel.index].x
        : (_sel.type == (int)EditorTool::BEATRICE_SPAWN) ? _level.beatriceSpawns[_sel.index].x
        : (_sel.type == (int)EditorTool::ENEMY_SPAWN) ? _level.enemySpawns[_sel.index].x
        : (_sel.type == (int)EditorTool::ELEVATOR) ? _level.elevators[_sel.index].x
        : (_sel.type == (int)EditorTool::WIN_ZONE) ? _level.winZone.x
        : (_sel.type == (int)EditorTool::KILL_ZONE) ? _level.killZones[_sel.index].x
        : (_sel.type == (int)EditorTool::PLAYER_SPAWN) ? _level.playerSpawn.x
        : (_sel.type == (int)EditorTool::REGULUS) ? _level.regulusPos.x
        : (_sel.type == (int)EditorTool::CAVE) ? _level.cavePos.x
        : (_sel.type == (int)EditorTool::POINT_LIGHT) ? _level.lights[_sel.index].x
        : (_sel.type == (int)EditorTool::SPOT_LIGHT) ? _level.lights[_sel.index].x
        : (_sel.type == (int)EditorTool::SKY_LIGHT) ? _level.lights[_sel.index].x
        : (_sel.type == (int)EditorTool::PROP) ? _level.props[_sel.index].x
        : _level.cavePos.x;
    float& refY = (_sel.type == (int)EditorTool::PLATFORM) ? _level.platforms[_sel.index].y
        : (_sel.type == (int)EditorTool::LADDER) ? _level.ladders[_sel.index].y
        : (_sel.type == (int)EditorTool::BEAM) ? _level.beams[_sel.index].y
        : (_sel.type == (int)EditorTool::PATH_NODE) ? _level.pathNodes[_sel.index].y
        : (_sel.type == (int)EditorTool::NUKE_SPAWN) ? _level.nukeSpawns[_sel.index].y
        : (_sel.type == (int)EditorTool::BEATRICE_SPAWN) ? _level.beatriceSpawns[_sel.index].y
        : (_sel.type == (int)EditorTool::ENEMY_SPAWN) ? _level.enemySpawns[_sel.index].y
        : (_sel.type == (int)EditorTool::ELEVATOR) ? _level.elevators[_sel.index].y
        : (_sel.type == (int)EditorTool::WIN_ZONE) ? _level.winZone.y
        : (_sel.type == (int)EditorTool::KILL_ZONE) ? _level.killZones[_sel.index].y
        : (_sel.type == (int)EditorTool::PLAYER_SPAWN) ? _level.playerSpawn.y
        : (_sel.type == (int)EditorTool::REGULUS) ? _level.regulusPos.y
        : (_sel.type == (int)EditorTool::CAVE) ? _level.cavePos.y
        : (_sel.type == (int)EditorTool::POINT_LIGHT) ? _level.lights[_sel.index].y
        : (_sel.type == (int)EditorTool::SPOT_LIGHT) ? _level.lights[_sel.index].y
        : (_sel.type == (int)EditorTool::SKY_LIGHT) ? _level.lights[_sel.index].y
        : (_sel.type == (int)EditorTool::PROP) ? _level.props[_sel.index].y
        : _level.cavePos.y;
    if (NumField("X ", refX, 1.f, -2000, 2000, px, cy, fw)) {} cy += rowH;
    if (NumField("Y ", refY, 1.f, -2000, 2000, px, cy, fw)) {} cy += rowH;

    if (_sel.type == (int)EditorTool::ELEVATOR && _sel.index < (int)_level.elevators.size()) {
        auto& el = _level.elevators[_sel.index];
        SectionHeader("── Elevator ────────────────");
        if (NumField("H  ", el.h, 2.f, 32, 2000, px, cy, fw)) { syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].h = el.h; }); } cy += rowH;
        if (NumField("W  ", el.w, 1.f, 16, 200, px, cy, fw)) { syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].w = el.w; }); } cy += rowH;
        if (NumField("Spd", el.speed, 2.f, 10, 600, px, cy, fw)) { syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].speed = el.speed; }); } cy += rowH;
        float bw2 = (fw - 4) / 2.f;
        Rectangle rUp = { px, cy, bw2, 18 };
        Rectangle rDwn = { px + bw2 + 4, cy, bw2, 18 };
        bool hUp = CheckCollisionPointRec(GetMousePosition(), rUp);
        bool hDwn = CheckCollisionPointRec(GetMousePosition(), rDwn);
        DrawRectangleRec(rUp, el.direction == 1 ? Color{ 20,100,40,255 } : (hUp ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
        DrawRectangleRec(rDwn, el.direction == -1 ? Color{ 100,40,20,255 } : (hDwn ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
        DrawText("▲ UP", (int)(rUp.x + 4), (int)cy + 4, 9, el.direction == 1 ? WHITE : Color{ 140,145,170,255 });
        DrawText("▼ DOWN", (int)(rDwn.x + 4), (int)cy + 4, 9, el.direction == -1 ? WHITE : Color{ 140,145,170,255 });
        if (hUp  && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); el.direction =  1; syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].direction =  1; }); }
        if (hDwn && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); el.direction = -1; syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].direction = -1; }); }
        cy += 22;
        SectionHeader("── Elevator Options ────────");
        // Invisible toggle
        {
            Rectangle r = { px, cy, fw, 16 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            DrawRectangleRec(r, el.invisible ? Color{20,80,60,255} : (hov ? Color{40,45,65,255} : Color{28,32,48,255}));
            DrawRectangleLinesEx(r, el.invisible ? 2.f : 1.f, el.invisible ? Color{80,220,160,255} : Color{70,75,100,255});
            const char* lbl = el.invisible ? "Invisible: ON" : "Invisible: OFF";
            DrawText(lbl, (int)px + 4, (int)cy + 3, 9, el.invisible ? Color{180,255,220,255} : Color{170,175,200,255});
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); el.invisible = !el.invisible; syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].invisible = el.invisible; }); }
            cy += 20;
        }
        // Horizontal toggle
        {
            Rectangle r = { px, cy, fw, 16 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            DrawRectangleRec(r, el.horizontal ? Color{20,60,100,255} : (hov ? Color{40,45,65,255} : Color{28,32,48,255}));
            DrawRectangleLinesEx(r, el.horizontal ? 2.f : 1.f, el.horizontal ? Color{80,160,255,255} : Color{70,75,100,255});
            const char* lbl = el.horizontal ? "Horizontal: ON" : "Horizontal: OFF (vertical)";
            DrawText(lbl, (int)px + 4, (int)cy + 3, 9, el.horizontal ? Color{160,210,255,255} : Color{170,175,200,255});
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); el.horizontal = !el.horizontal; syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].horizontal = el.horizontal; }); }
            cy += 20;
        }
        // BackAndForth toggle
        {
            Rectangle r = { px, cy, fw, 16 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), r);
            DrawRectangleRec(r, el.backAndForth ? Color{80,40,100,255} : (hov ? Color{40,45,65,255} : Color{28,32,48,255}));
            DrawRectangleLinesEx(r, el.backAndForth ? 2.f : 1.f, el.backAndForth ? Color{200,120,255,255} : Color{70,75,100,255});
            const char* lbl = el.backAndForth ? "BackForth: ON" : "BackForth: OFF (loop)";
            DrawText(lbl, (int)px + 4, (int)cy + 3, 9, el.backAndForth ? Color{230,180,255,255} : Color{170,175,200,255});
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); el.backAndForth = !el.backAndForth; syncField(EditorTool::ELEVATOR, [&](int i) { _level.elevators[i].backAndForth = el.backAndForth; }); }
            cy += 20;
        }
        SectionHeader("── Children ────────────────");
        SelectedEnt elEnt = { (int)EditorTool::ELEVATOR, _sel.index };
        auto ch = GetChildren(elEnt);
        if (ch.empty()) {
            DrawText("No children. Right-click items in", (int)px, (int)cy, 9, { 80,85,110,255 }); cy += 11;
            DrawText("outliner to attach.", (int)px, (int)cy, 9, { 80,85,110,255 }); cy += 14;
        }
        for (const auto& ce : ch) {
            DrawText(TextFormat("  • %s %d", ToolName((EditorTool)ce.type), ce.index),
                (int)px, (int)cy, 9, ToolColor((EditorTool)ce.type)); cy += 12;
        }
    }
    if (_sel.type == (int)EditorTool::CAVE) {
        SectionHeader("── Cave Options ───────────");
        Rectangle visR = { px, cy, fw, 18 };
        bool hVis = CheckCollisionPointRec(GetMousePosition(), visR);
        bool isVis = _level.caveVisible;
        DrawRectangleRec(visR, isVis ? Color{20,80,20,255} : (hVis ? Color{60,30,30,255} : Color{40,20,20,255}));
        DrawRectangleLinesEx(visR, isVis ? 2.f : 1.f, isVis ? Color{80,220,80,255} : Color{200,80,80,255});
        const char* visLbl = isVis ? "Visible: ON" : "Visible: OFF (circle only)";
        int vlw = MeasureText(visLbl, 9);
        DrawText(visLbl, (int)(visR.x + visR.width * 0.5f - vlw * 0.5f), (int)cy + 5, 9,
            isVis ? Color{180,255,180,255} : Color{255,160,160,255});
        if (hVis && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); _level.caveVisible = !_level.caveVisible; }
        cy += 22;

        Rectangle spawnTogR = { px, cy, fw, 18 };
        bool hSpawnTog = CheckCollisionPointRec(GetMousePosition(), spawnTogR);
        bool isSpawn = _level.caveSpawnEnabled;
        DrawRectangleRec(spawnTogR, isSpawn ? Color{20,60,20,255} : (hSpawnTog ? Color{60,40,20,255} : Color{40,30,20,255}));
        DrawRectangleLinesEx(spawnTogR, isSpawn ? 2.f : 1.f, isSpawn ? Color{80,220,80,255} : Color{255,160,80,255});
        const char* spawnLbl = isSpawn ? "EnemySpawn: ON" : "EnemySpawn: OFF";
        int spawnLblW = MeasureText(spawnLbl, 9);
        DrawText(spawnLbl, (int)(spawnTogR.x + spawnTogR.width*0.5f - spawnLblW*0.5f), (int)cy+5, 9,
            isSpawn ? Color{180,255,180,255} : Color{255,200,140,255});
        if (hSpawnTog && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); _level.caveSpawnEnabled = !_level.caveSpawnEnabled; }
        cy += 22;

        if (_level.caveSpawnEnabled) {
            if (NumField("Rate(s)", _level.caveSpawnRate, 0.1f, 2.f, 120.f, px, cy, fw)) {}
            cy += rowH;
        }

        // Barrel-end bunny spawn toggle
        Rectangle bbR = { px, cy, fw, 18 };
        bool hBB = CheckCollisionPointRec(GetMousePosition(), bbR);
        bool isBB = _level.barrelEndSpawnBunnies;
        DrawRectangleRec(bbR, isBB ? Color{20,60,20,255} : (hBB ? Color{60,30,30,255} : Color{40,20,20,255}));
        DrawRectangleLinesEx(bbR, isBB ? 2.f : 1.f, isBB ? Color{80,220,80,255} : Color{220,80,80,255});
        const char* bbLbl = isBB ? "BarrelBunny: ON" : "BarrelBunny: OFF";
        int bbLblW = MeasureText(bbLbl, 9);
        DrawText(bbLbl, (int)(bbR.x + bbR.width*0.5f - bbLblW*0.5f), (int)cy+5, 9,
            isBB ? Color{180,255,180,255} : Color{255,140,140,255});
        if (hBB && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); _level.barrelEndSpawnBunnies = !_level.barrelEndSpawnBunnies; }
        cy += 22;
    }
    if (_sel.type == (int)EditorTool::PLATFORM) {
        SectionHeader("── Rotation ───────────────");
        auto& p = _level.platforms[_sel.index];
        if (NumField("Tilt", p.tilt, .3f, -89, 89, px, cy, fw)) { syncField(EditorTool::PLATFORM, [&](int i) { _level.platforms[i].tilt = p.tilt; }); } cy += rowH;
        float bw2 = (fw - 4) / 4.f;
        const float presets[] = { -45,-15,15,45 }; const char* plabels[] = { "-45","-15","15","45" };
        for (int pi = 0; pi < 4; pi++) {
            Rectangle br = { px + pi * (bw2 + 1),cy,bw2,14 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), br);
            DrawRectangleRec(br, hov ? Color{ 60,65,90,255 } : Color{ 35,38,55,255 });
            int tw = MeasureText(plabels[pi], 9); DrawText(plabels[pi], (int)(br.x + br.width / 2 - tw / 2), (int)br.y + 2, 9, { 180,185,210,255 });
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); p.tilt = presets[pi]; syncField(EditorTool::PLATFORM, [&](int i) { _level.platforms[i].tilt = presets[pi]; }); }
        }
        cy += 17;
    }
    if (_sel.type == (int)EditorTool::PLATFORM) {
        SectionHeader("── Scale ──────────────────");
        auto& p = _level.platforms[_sel.index];
        if (NumField("W  ", p.w, 1.f, GRID_SZ, 2000, px, cy, fw)) { syncField(EditorTool::PLATFORM, [&](int i) { _level.platforms[i].w = p.w; }); } cy += rowH;
        if (NumField("H  ", p.h, .5f, 0, 200, px, cy, fw)) { syncField(EditorTool::PLATFORM, [&](int i) { _level.platforms[i].h = p.h; }); } cy += rowH;
        {
            float hw = (fw - 3) * 0.5f;
            Rectangle cpR = { px, cy, hw, 15 }, ppR = { px + hw + 3, cy, hw, 15 };
            bool cpH = CheckCollisionPointRec(GetMousePosition(), cpR);
            bool ppH = CheckCollisionPointRec(GetMousePosition(), ppR);
            bool canPaste = (_propClip.type == (int)EditorTool::PLATFORM);
            DrawRectangleRec(cpR, cpH ? Color{ 50,80,50,255 } : Color{ 30,50,30,255 }); DrawRectangleLinesEx(cpR, 1, { 60,160,60,255 });
            DrawText("Cpy Props", (int)(cpR.x + 2), (int)(cpR.y + 2), 9, { 120,220,120,255 });
            DrawRectangleRec(ppR, canPaste ? (ppH ? Color{ 60,50,20,255 } : Color{ 40,35,15,255 }) : Color{ 25,28,38,255 });
            DrawRectangleLinesEx(ppR, 1, canPaste ? Color{ 220,180,60,255 } : Color{ 50,55,70,255 });
            DrawText("Pst Props", (int)(ppR.x + 2), (int)(ppR.y + 2), 9, canPaste ? Color{ 220,180,60,255 } : Color{ 80,85,100,255 });
            if (cpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) CopyProps();
            if (ppH && canPaste && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); PasteProps(); }
            cy += 18;
        }
    }
    else if (_sel.type == (int)EditorTool::LADDER && _sel.index < (int)_level.ladders.size()) {
        SectionHeader("── Scale ──────────────────");
        auto& l = _level.ladders[_sel.index];
        if (NumField("W  ", l.w, 1.f, 8, 200, px, cy, fw)) { syncField(EditorTool::LADDER, [&](int i) { _level.ladders[i].w = l.w; }); } cy += rowH;
        if (NumField("H  ", l.h, 1.f, GRID_SZ, 2000, px, cy, fw)) { syncField(EditorTool::LADDER, [&](int i) { _level.ladders[i].h = l.h; }); } cy += rowH;
    }
    if (_sel.type == (int)EditorTool::BEAM && _sel.index < (int)_level.beams.size()) {
        auto& bm = _level.beams[_sel.index];
        SectionHeader("── Beam Texture ───────────");
        // Row 1: default + variants 1-5
        {
            const int COLS = 6;
            float tbw = (fw - (COLS - 1) * 2.f) / COLS;
            for (int ti = 0; ti < COLS; ti++) {
                Rectangle tb = { px + ti * (tbw + 2.f), cy, tbw, 16 };
                bool isActive = (bm.texVariant == ti);
                bool hov = CheckCollisionPointRec(GetMousePosition(), tb);
                DrawRectangleRec(tb, isActive ? Color{ 60,40,10,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
                DrawRectangleLinesEx(tb, isActive ? 2.f : 1.f, isActive ? Color{ 255,200,80,255 } : Color{ 70,75,100,255 });
                const char* lbl = (ti == 0) ? "Def" : TextFormat("%d", ti);
                int nlw = MeasureText(lbl, 9);
                DrawText(lbl, (int)(tb.x + tb.width / 2 - nlw / 2), (int)tb.y + 4, 9,
                    isActive ? Color{ 255,200,80,255 } : Color{ 170,175,200,255 });
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.texVariant = ti; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].texVariant = ti; }); }
            }
            cy += 20;
        }
        // Row 2: variants 6-10
        {
            const int COLS = 5;
            float tbw = (fw - (COLS - 1) * 2.f) / COLS;
            for (int ti = 0; ti < COLS; ti++) {
                int varIdx = ti + 6;
                Rectangle tb = { px + ti * (tbw + 2.f), cy, tbw, 16 };
                bool isActive = (bm.texVariant == varIdx);
                bool hov = CheckCollisionPointRec(GetMousePosition(), tb);
                DrawRectangleRec(tb, isActive ? Color{ 60,40,10,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
                DrawRectangleLinesEx(tb, isActive ? 2.f : 1.f, isActive ? Color{ 255,200,80,255 } : Color{ 70,75,100,255 });
                const char* lbl = TextFormat("%d", varIdx);
                int nlw = MeasureText(lbl, 9);
                DrawText(lbl, (int)(tb.x + tb.width / 2 - nlw / 2), (int)tb.y + 4, 9,
                    isActive ? Color{ 255,200,80,255 } : Color{ 170,175,200,255 });
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.texVariant = varIdx; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].texVariant = varIdx; }); }
            }
            cy += 20;
        }
        // Row 3: variants 11-12 (TransFloor, TransFloor2)
        {
            const int COLS = 2;
            const char* rowLabels[] = { "TFloor", "TFloor2" };
            float tbw = (fw - (COLS - 1) * 2.f) / COLS;
            for (int ti = 0; ti < COLS; ti++) {
                int varIdx = ti + 11;
                Rectangle tb = { px + ti * (tbw + 2.f), cy, tbw, 16 };
                bool isActive = (bm.texVariant == varIdx);
                bool hov = CheckCollisionPointRec(GetMousePosition(), tb);
                DrawRectangleRec(tb, isActive ? Color{ 20,50,80,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
                DrawRectangleLinesEx(tb, isActive ? 2.f : 1.f, isActive ? Color{ 100,200,255,255 } : Color{ 70,75,100,255 });
                int nlw = MeasureText(rowLabels[ti], 9);
                DrawText(rowLabels[ti], (int)(tb.x + tb.width / 2 - nlw / 2), (int)tb.y + 4, 9,
                    isActive ? Color{ 100,200,255,255 } : Color{ 170,175,200,255 });
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.texVariant = varIdx; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].texVariant = varIdx; }); }
            }
            cy += 20;
        }
        // Show the texture name
        const char* varName = (bm.texVariant == 0) ? "Dk_FloorPart (default)"
            : (bm.texVariant == 11) ? "TransFloor"
            : (bm.texVariant == 12) ? "TransFloor2"
            : TextFormat("Dk_FloorPart%d", bm.texVariant);
        DrawText(varName, (int)px, (int)cy, 9, { 130,200,130,255 }); cy += 13;
        SectionHeader("── Render Layer ───────────");
        {
            float rlF = (float)bm.renderLayer;
            if (NumField("Layer", rlF, 1.f, -10, 100, px, cy, fw)) {
                PushUndo(); bm.renderLayer = (int)rlF;
                syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].renderLayer = bm.renderLayer; });
            }
            cy += rowH;
        }
        DrawText("0=below killzones, 1+=above", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 13;
        // ── Flip X ────────────────────────────────────────────────────────────
        SectionHeader("── Flip ───────────────────");
        {
            Rectangle flipR = { px, cy, fw, 18 };
            bool hFlip = CheckCollisionPointRec(GetMousePosition(), flipR);
            DrawRectangleRec(flipR, bm.flipX ? Color{ 40,20,80,255 } : (hFlip ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
            DrawRectangleLinesEx(flipR, bm.flipX ? 2.f : 1.f, bm.flipX ? Color{ 180,100,255,255 } : Color{ 70,75,100,255 });
            const char* flipLbl = bm.flipX ? "Flip X: ON  ◀▶" : "Flip X: OFF  ▶◀";
            int flw = MeasureText(flipLbl, 9);
            DrawText(flipLbl, (int)(flipR.x + flipR.width / 2 - flw / 2), (int)flipR.y + 5, 9,
                bm.flipX ? Color{ 200,150,255,255 } : Color{ 170,175,200,255 });
            if (hFlip && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.flipX = !bm.flipX; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].flipX = bm.flipX; }); }
            cy += 22;
        }
        // ── Transparent (light pass-through) ─────────────────────────────────
        SectionHeader("── Light ──────────────────");
        {
            Rectangle trR = { px, cy, fw, 18 };
            bool hTr = CheckCollisionPointRec(GetMousePosition(), trR);
            DrawRectangleRec(trR, bm.transparent ? Color{ 20,50,80,255 } : (hTr ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
            DrawRectangleLinesEx(trR, bm.transparent ? 2.f : 1.f, bm.transparent ? Color{ 100,200,255,255 } : Color{ 70,75,100,255 });
            const char* trLbl = bm.transparent ? "Transparent: ON  (light passes)" : "Transparent: OFF (blocks light)";
            int trw = MeasureText(trLbl, 9);
            DrawText(trLbl, (int)(trR.x + trR.width / 2 - trw / 2), (int)trR.y + 5, 9,
                bm.transparent ? Color{ 100,200,255,255 } : Color{ 170,175,200,255 });
            if (hTr && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.transparent = !bm.transparent; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].transparent = bm.transparent; }); }
            cy += 22;
        }
        // ── Sound material selector ───────────────────────────────────────────
        SectionHeader("── Sound Material ─────────");
        {
            static const char* matNames[] = {
                "NONE","BLUNTWOOD","CONCRETE","DECKWOOD","DIRT","GRASS",
                "GRAVEL","LINO","MARBLE","METALBAR","METALBOX","MUD",
                "SAND","SNOW","STONE","WOOD","SQUEAKY"
            };
            static const int MAT_COUNT = 17;
            static const int MCOLS = 3;
            float tbw = (fw - (MCOLS - 1) * 2.f) / MCOLS;
            for (int mi = 0; mi < MAT_COUNT; mi++) {
                int col = mi % MCOLS;
                if (col == 0 && mi > 0) cy += 18;
                Rectangle mb = { px + col * (tbw + 2.f), cy, tbw, 16 };
                bool isActive = (bm.soundMaterial == mi);
                bool hov = CheckCollisionPointRec(GetMousePosition(), mb);
                DrawRectangleRec(mb, isActive ? Color{ 30,60,20,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
                DrawRectangleLinesEx(mb, isActive ? 2.f : 1.f, isActive ? Color{ 100,220,60,255 } : Color{ 70,75,100,255 });
                int nlw = MeasureText(matNames[mi], 8);
                DrawText(matNames[mi], (int)(mb.x + mb.width / 2 - nlw / 2), (int)mb.y + 4, 8,
                    isActive ? Color{ 140,255,80,255 } : Color{ 150,155,180,255 });
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); bm.soundMaterial = mi; syncField(EditorTool::BEAM, [&](int i) { _level.beams[i].soundMaterial = mi; }); }
            }
            cy += 20;
        }
    }   // end BEAM section
    if (_sel.type == (int)EditorTool::WIN_ZONE) {
        auto& wz = _level.winZone;
        SectionHeader("── Win Zone ───────────────");
        if (NumField("W  ", wz.w, 1.f, GRID_SZ, 2000, px, cy, fw)) {} cy += rowH;
        if (NumField("H  ", wz.h, 1.f, GRID_SZ, 2000, px, cy, fw)) {} cy += rowH;
        DrawText("Touching this area triggers", (int)px, (int)cy, 9, { 80,200,120,255 }); cy += 11;
        DrawText("the next level / win screen.", (int)px, (int)cy, 9, { 80,200,120,255 }); cy += 14;
    }
    if (_sel.type == (int)EditorTool::KILL_ZONE && _sel.index < (int)_level.killZones.size()) {
        auto& kz = _level.killZones[_sel.index];
        SectionHeader("── Kill Zone ──────────────");
        if (NumField("W  ", kz.w, 1.f, GRID_SZ, 2000, px, cy, fw)) { syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].w = kz.w; }); } cy += rowH;
        if (NumField("H  ", kz.h, 1.f, GRID_SZ, 2000, px, cy, fw)) { syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].h = kz.h; }); } cy += rowH;
        SectionHeader("── Rotation ───────────────");
        if (NumField("Rot", kz.rotation, 1.f, -360, 360, px, cy, fw)) { syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].rotation = kz.rotation; }); } cy += rowH;
        float bw2kz = (fw - 4) / 4.f;
        const float kzPresets[] = { 0.f, 90.f, 180.f, 270.f }; const char* kzLabels[] = { "0Â°", "90Â°", "180Â°", "270Â°" };
        for (int pi = 0; pi < 4; pi++) {
            Rectangle br = { px + pi * (bw2kz + 1),cy,bw2kz,14 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), br);
            bool isAct = (fabsf(kz.rotation - kzPresets[pi]) < 0.5f);
            DrawRectangleRec(br, isAct ? Color{ 60,40,10,255 } : (hov ? Color{ 60,65,90,255 } : Color{ 35,38,55,255 }));
            DrawRectangleLinesEx(br, 1.f, isAct ? Color{ 255,200,80,255 } : Color{ 55,60,85,255 });
            int tw = MeasureText(kzLabels[pi], 9); DrawText(kzLabels[pi], (int)(br.x + br.width / 2 - tw / 2), (int)br.y + 2, 9, isAct ? Color{ 255,200,80,255 } : Color{ 180,185,210,255 });
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); kz.rotation = kzPresets[pi]; syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].rotation = kzPresets[pi]; }); }
        }
        cy += 17;
        SectionHeader("── Texture ────────────────");
        const char* texNames[] = { "None", "GoldenPiston" };
        int texCount = 2;
        float tbw = (fw - (texCount - 1) * 2.f) / texCount;
        for (int ti = 0; ti < texCount; ti++) {
            Rectangle tb = { px + ti * (tbw + 2.f), cy, tbw, 16 };
            bool isActive = ((int)kz.texId == ti);
            bool hov = CheckCollisionPointRec(GetMousePosition(), tb);
            DrawRectangleRec(tb, isActive ? Color{ 60, 30, 10, 255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
            DrawRectangleLinesEx(tb, isActive ? 2.f : 1.f, isActive ? Color{ 255,200,80,255 } : Color{ 70,75,100,255 });
            int ntw = MeasureText(texNames[ti], 9);
            DrawText(texNames[ti], (int)(tb.x + tb.width / 2 - ntw / 2), (int)tb.y + 3, 9,
                isActive ? Color{ 255,200,80,255 } : Color{ 170,175,200,255 });
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); kz.texId = (KillZoneTexture)ti; syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].texId = (KillZoneTexture)ti; }); }
        }
        cy += 20;
        if (kz.texId == KillZoneTexture::DK_GOLDEN_PISTON && _goldenPistonTex && _goldenPistonTex->id > 0) {
            float sw2 = fminf(fw, 40.f), sh2 = sw2;
            DrawTexturePro(*_goldenPistonTex,
                { 0, 0, (float)_goldenPistonTex->width, (float)_goldenPistonTex->height },
                { px, cy, sw2, sh2 }, {}, kz.rotation, WHITE);
            cy += sh2 + 4;
        }
        SectionHeader("── Render Layer ───────────");
        {
            float rlF = (float)kz.renderLayer;
            if (NumField("Layer", rlF, 1.f, -10, 100, px, cy, fw)) {
                PushUndo(); kz.renderLayer = (int)rlF;
                syncField(EditorTool::KILL_ZONE, [&](int i) { _level.killZones[i].renderLayer = kz.renderLayer; });
            }
            cy += rowH;
        }
        DrawText("1=above beams, 0=same level", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 13;
        // ── Copy / Paste props ────────────────────────────────────────────────
        {
            float hw = (fw - 3) * 0.5f;
            Rectangle cpR = { px, cy, hw, 15 }, ppR = { px + hw + 3, cy, hw, 15 };
            bool cpH = CheckCollisionPointRec(GetMousePosition(), cpR);
            bool ppH = CheckCollisionPointRec(GetMousePosition(), ppR);
            bool canPaste = (_propClip.type == (int)EditorTool::KILL_ZONE);
            DrawRectangleRec(cpR, cpH ? Color{ 50,80,50,255 } : Color{ 30,50,30,255 }); DrawRectangleLinesEx(cpR, 1, { 60,160,60,255 });
            DrawText("Cpy Props", (int)(cpR.x + 2), (int)(cpR.y + 2), 9, { 120,220,120,255 });
            DrawRectangleRec(ppR, canPaste ? (ppH ? Color{ 60,50,20,255 } : Color{ 40,35,15,255 }) : Color{ 25,28,38,255 });
            DrawRectangleLinesEx(ppR, 1, canPaste ? Color{ 220,180,60,255 } : Color{ 50,55,70,255 });
            DrawText("Pst Props", (int)(ppR.x + 2), (int)(ppR.y + 2), 9, canPaste ? Color{ 220,180,60,255 } : Color{ 80,85,100,255 });
            if (cpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) CopyProps();
            if (ppH && canPaste && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); PasteProps(); }
            cy += 18;
        }
    }
    if (_sel.type == (int)EditorTool::CONVEYOR && _sel.index < (int)_level.conveyors.size()) {
        auto& cv = _level.conveyors[_sel.index];
        SectionHeader("── Conveyor ────────────────");
        if (NumField("Len", cv.length, 2.f, 32, 4000, px, cy, fw)) { syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].length = cv.length; }); } cy += rowH;
        if (NumField("Spd", cv.speed, 1.f, 10, 600, px, cy, fw)) { syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].speed = cv.speed; }); } cy += rowH;
        if (NumField("Rot", cv.rotation, 0.3f, -89, 89, px, cy, fw)) { syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].rotation = cv.rotation; }); } cy += rowH;
        // Direction buttons
        float bw2cv = (fw - 4) / 2.f;
        Rectangle rL = { px, cy, bw2cv, 18 }, rR = { px + bw2cv + 4, cy, bw2cv, 18 };
        bool hL = CheckCollisionPointRec(GetMousePosition(), rL);
        bool hR = CheckCollisionPointRec(GetMousePosition(), rR);
        DrawRectangleRec(rL, cv.direction == -1 ? Color{ 20,100,40,255 } : (hL ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
        DrawRectangleRec(rR, cv.direction == 1 ? Color{ 20,100,40,255 } : (hR ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
        DrawText("< LEFT", (int)(rL.x + 4), (int)cy + 4, 9, cv.direction == -1 ? WHITE : Color{ 140,145,170,255 });
        DrawText("RIGHT >", (int)(rR.x + 4), (int)cy + 4, 9, cv.direction == 1 ? WHITE : Color{ 140,145,170,255 });
        if (hL && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); cv.direction = -1; syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].direction = -1; }); }
        if (hR && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); cv.direction =  1; syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].direction =  1; }); }
        cy += 22;
        SectionHeader("── Sound Material ─────────");
        {
            static const char* cvMatNames[] = {
                "NONE","BLUNTWOOD","CONCRETE","DECKWOOD","DIRT","GRASS",
                "GRAVEL","LINO","MARBLE","METALBAR","METALBOX","MUD",
                "SAND","SNOW","STONE","WOOD","SQUEAKY"
            };
            static const int CV_MAT_COUNT = 17;
            static const int CV_MCOLS = 3;
            float tbw2 = (fw - (CV_MCOLS - 1) * 2.f) / CV_MCOLS;
            for (int mi = 0; mi < CV_MAT_COUNT; mi++) {
                int col = mi % CV_MCOLS;
                if (col == 0 && mi > 0) cy += 18;
                Rectangle mb = { px + col * (tbw2 + 2.f), cy, tbw2, 16 };
                bool isActive = (cv.soundMaterial == mi);
                bool hov = CheckCollisionPointRec(GetMousePosition(), mb);
                DrawRectangleRec(mb, isActive ? Color{30,60,20,255} : (hov ? Color{45,48,68,255} : Color{28,32,48,255}));
                DrawRectangleLinesEx(mb, isActive ? 2.f : 1.f, isActive ? Color{100,220,60,255} : Color{70,75,100,255});
                int nlw = MeasureText(cvMatNames[mi], 8);
                DrawText(cvMatNames[mi], (int)(mb.x + mb.width / 2 - nlw / 2), (int)mb.y + 4, 8,
                    isActive ? Color{140,255,80,255} : Color{150,155,180,255});
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); cv.soundMaterial = mi; syncField(EditorTool::CONVEYOR, [&](int i) { _level.conveyors[i].soundMaterial = mi; }); }
            }
            cy += 20;
        }
    }
    if (_sel.type == (int)EditorTool::PROP && _sel.index < (int)_level.props.size()) {
        auto& pr = _level.props[_sel.index];
        SectionHeader("── Prop ────────────────────");
        if (NumField("W  ", pr.width, 1.f, 4, 2000, px, cy, fw)) { syncField(EditorTool::PROP, [&](int i) { _level.props[i].width = pr.width; }); } cy += rowH;
        if (NumField("H  ", pr.height, 1.f, 4, 2000, px, cy, fw)) { syncField(EditorTool::PROP, [&](int i) { _level.props[i].height = pr.height; }); } cy += rowH;
        if (NumField("Rot", pr.rotation, 0.3f, -360, 360, px, cy, fw)) { syncField(EditorTool::PROP, [&](int i) { _level.props[i].rotation = pr.rotation; }); } cy += rowH;
        SectionHeader("── Lighting ────────────────");
        if (NumField("Aff", pr.lightAffect, 0.05f, 0.f, 3.f, px, cy, fw)) { syncField(EditorTool::PROP, [&](int i) { _level.props[i].lightAffect = pr.lightAffect; }); } cy += rowH;
        DrawText("0=unlit 1=lit 2+=glow", (int)px, (int)cy, 9, { 110,150,200,255 }); cy += 12;
        SectionHeader("── Render Layer ────────────");
        {
            float rlF = (float)pr.renderLayer;
            if (NumField("Layer", rlF, 1.f, 0, 2, px, cy, fw)) { PushUndo(); pr.renderLayer = (int)rlF; syncField(EditorTool::PROP, [&](int i) { _level.props[i].renderLayer = pr.renderLayer; }); }
            cy += rowH;
        }
        DrawText("0=behind  1=front  2=overlay", (int)px, (int)cy, 9, { 110,150,200,255 }); cy += 12;
        SectionHeader("── Collision ───────────────");
        {
            Rectangle colR = { px, cy, fw, 18 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), colR);
            DrawRectangleRec(colR, pr.hasCollision ? Color{ 30,80,30,255 } : (hov ? Color{ 40,45,65,255 } : Color{ 28,32,48,255 }));
            DrawRectangleLinesEx(colR, pr.hasCollision ? 2.f : 1.f, pr.hasCollision ? Color{ 100,220,100,255 } : Color{ 70,75,100,255 });
            const char* ts = pr.hasCollision ? "Collision: ON  (click off)" : "Collision: OFF (click on)";
            int tww = MeasureText(ts, 9);
            DrawText(ts, (int)(colR.x + colR.width / 2 - tww / 2), (int)colR.y + 5, 9,
                pr.hasCollision ? Color{ 180,255,180,255 } : Color{ 170,175,200,255 });
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); pr.hasCollision = !pr.hasCollision; syncField(EditorTool::PROP, [&](int i) { _level.props[i].hasCollision = pr.hasCollision; }); }
            cy += 22;
        }
        SectionHeader("── Tex Variant ─────────────");
        {
            static const char* propVarNames[] = { "Light", "WoodBox", "Barrel", "Support", "OilCan", "Fire", "Coin" };
            static constexpr int PROP_VAR_COUNT = 7;
            const int COLS = 3;
            float tbw = (fw - (COLS - 1) * 2.f) / COLS;
            for (int row = 0; row < 3; row++) {
                for (int col = 0; col < COLS; col++) {
                    int vi = row * COLS + col;
                    if (vi >= PROP_VAR_COUNT) break;
                    Rectangle tb = { px + col * (tbw + 2.f), cy, tbw, 16 };
                    bool isActive = (pr.texVariant == vi);
                    bool hov = CheckCollisionPointRec(GetMousePosition(), tb);
                    DrawRectangleRec(tb, isActive ? Color{ 60,40,10,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
                    DrawRectangleLinesEx(tb, isActive ? 2.f : 1.f, isActive ? Color{ 255,200,80,255 } : Color{ 70,75,100,255 });
                    int nlw = MeasureText(propVarNames[vi], 9);
                    DrawText(propVarNames[vi], (int)(tb.x + tb.width / 2 - nlw / 2), (int)tb.y + 4, 9,
                        isActive ? Color{ 255,200,80,255 } : Color{ 170,175,200,255 });
                    if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); pr.texVariant = vi; syncField(EditorTool::PROP, [&](int i) { _level.props[i].texVariant = vi; }); }
                }
                cy += 20;
            }
        }
        // ── Bob ───────────────────────────────────────────────────────────────────
        {
            SectionHeader("── Bob ─────────────────────");
            if (NumField("Amp", pr.bobAmp, 0.5f, 0.f, 40.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].bobAmp = pr.bobAmp; }); } cy += rowH;
            DrawText("0 = off  pixels", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
            if (NumField("Spd", pr.bobSpeed, 0.05f, 0.1f, 20.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].bobSpeed = pr.bobSpeed; }); } cy += rowH;
            DrawText("rad/s  3=normal", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
        }
        // ── Flicker ───────────────────────────────────────────────────────────────
        {
            SectionHeader("── Flicker ─────────────────");
            if (NumField("Int", pr.flickerIntens, 0.02f, 0.f, 1.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].flickerIntens = pr.flickerIntens; }); } cy += rowH;
            DrawText("0=off  1=max shine", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
            if (NumField("Spd", pr.flickerSpeed, 0.1f, 0.1f, 30.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].flickerSpeed = pr.flickerSpeed; }); } cy += rowH;
            DrawText("rad/s  5=normal", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
        }
        // ── Sway ──────────────────────────────────────────────────────────────────
        {
            SectionHeader("── Sway ────────────────────");
            if (NumField("Ang", pr.swayAngle, 0.5f, 0.f, 45.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].swayAngle = pr.swayAngle; }); } cy += rowH;
            DrawText("0=off  deg 5-10 typical", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
            if (NumField("Spd", pr.swaySpeed, 0.05f, 0.1f, 10.f, px, cy, fw)) { PushUndo(); syncField(EditorTool::PROP, [&](int i) { _level.props[i].swaySpeed = pr.swaySpeed; }); } cy += rowH;
            DrawText("rad/s  1.5=~4s period", (int)px, (int)cy, 9, { 110,130,180,255 }); cy += 12;
        }
        // Copy / Paste props
        {
            float hw = (fw - 3) * 0.5f;
            Rectangle cpR = { px, cy, hw, 15 }, ppR = { px + hw + 3, cy, hw, 15 };
            bool cpH = CheckCollisionPointRec(GetMousePosition(), cpR);
            bool ppH = CheckCollisionPointRec(GetMousePosition(), ppR);
            bool canPaste = (_propClip.type == (int)EditorTool::PROP);
            DrawRectangleRec(cpR, cpH ? Color{ 50,80,50,255 } : Color{ 30,50,30,255 }); DrawRectangleLinesEx(cpR, 1, { 60,160,60,255 });
            DrawText("Cpy Props", (int)(cpR.x + 2), (int)(cpR.y + 2), 9, { 120,220,120,255 });
            DrawRectangleRec(ppR, canPaste ? (ppH ? Color{ 60,50,20,255 } : Color{ 40,35,15,255 }) : Color{ 25,28,38,255 });
            DrawRectangleLinesEx(ppR, 1, canPaste ? Color{ 220,180,60,255 } : Color{ 50,55,70,255 });
            DrawText("Pst Props", (int)(ppR.x + 2), (int)(ppR.y + 2), 9, canPaste ? Color{ 220,180,60,255 } : Color{ 80,85,100,255 });
            if (cpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) CopyProps();
            if (ppH && canPaste && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); PasteProps(); }
            cy += 18;
        }
    }
    if ((_sel.type == (int)EditorTool::POINT_LIGHT ||
        _sel.type == (int)EditorTool::SPOT_LIGHT ||
        _sel.type == (int)EditorTool::SKY_LIGHT) && _sel.index < (int)_level.lights.size())
    {
        auto& L = _level.lights[_sel.index];
        SectionHeader("── Light ──────────────────");

        // Type selector — three buttons
        const char* tyNames[] = { "Point", "Spot", "Sky" };
        float tw = (fw - 4) / 3.f;
        for (int ti = 0; ti < 3; ti++) {
            Rectangle br = { px + ti * (tw + 2), cy, tw, 16 };
            bool act = ((int)L.type == ti);
            bool hov = CheckCollisionPointRec(GetMousePosition(), br);
            DrawRectangleRec(br, act ? Color{ 60,40,10,255 } : (hov ? Color{ 45,48,68,255 } : Color{ 28,32,48,255 }));
            DrawRectangleLinesEx(br, act ? 2.f : 1.f, act ? Color{ 255,200,80,255 } : Color{ 60,65,90,255 });
            int twn = MeasureText(tyNames[ti], 9);
            DrawText(tyNames[ti], (int)(br.x + br.width / 2 - twn / 2), (int)br.y + 3, 9,
                act ? Color{ 255,200,80,255 } : Color{ 170,175,200,255 });
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && (int)L.type != ti) {
                PushUndo(); L.type = (LightType)ti;
                _sel.type = (ti == 0) ? (int)EditorTool::POINT_LIGHT
                    : (ti == 1) ? (int)EditorTool::SPOT_LIGHT
                    : (int)EditorTool::SKY_LIGHT;
            }
        }
        cy += 20;

        SectionHeader("── Color ──────────────────");
        auto lt = (EditorTool)_sel.type;
        if (NumField("R  ", L.r, 0.005f, 0.f, 1.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].r = L.r; }); } cy += rowH;
        if (NumField("G  ", L.g, 0.005f, 0.f, 1.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].g = L.g; }); } cy += rowH;
        if (NumField("B  ", L.b, 0.005f, 0.f, 1.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].b = L.b; }); } cy += rowH;
        // Color preview swatch
        Color prv = { (unsigned char)(L.r * 255), (unsigned char)(L.g * 255), (unsigned char)(L.b * 255), 255 };
        Rectangle sw2 = { px, cy, fw, 14 };
        DrawRectangleRec(sw2, prv);
        DrawRectangleLinesEx(sw2, 1, { 60,65,90,255 });
        cy += 18;

        SectionHeader("── Emission ───────────────");
        if (NumField("Int", L.intensity, 0.02f, 0.f, 8.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].intensity = L.intensity; }); } cy += rowH;
        if (NumField("Rad", L.radius, 1.0f, 8.f, 4000.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].radius = L.radius; }); } cy += rowH;
        if (NumField("Inn", L.innerRadius, 1.0f, 0.f, L.radius * 0.99f, px, cy, fw)) {
            L.innerRadius = fminf(L.innerRadius, L.radius * 0.99f);
            syncField(lt, [&](int i) { _level.lights[i].innerRadius = L.innerRadius; });
        } cy += rowH;
        DrawText("Inn = flat center; 0 = pure falloff", (int)px, (int)cy, 9, { 110, 150, 200, 255 }); cy += 12;

        if (L.type == LightType::SPOT) {
            SectionHeader("── Spot ───────────────────");
            if (NumField("Ang", L.angle, 0.5f, 1.f, 175.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].angle = L.angle; }); } cy += rowH;
            if (NumField("Dir", L.direction, 0.5f, 0.f, 360.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].direction = L.direction; }); } cy += rowH;
        }
        if (L.type == LightType::SKY) {
            SectionHeader("── Sky ────────────────────");
            if (NumField("Dir", L.direction, 0.5f, 0.f, 360.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].direction = L.direction; }); } cy += rowH;
            DrawText("270 = light from above", (int)px, (int)cy, 9, { 110,150,200,255 }); cy += 12;
        }

        SectionHeader("── Advanced ───────────────");
        {
            float bF = (float)L.bounces;
            if (NumField("Bnc", bF, 1.f, 0.f, 4.f, px, cy, fw)) { L.bounces = (int)bF; syncField(lt, [&](int i) { _level.lights[i].bounces = L.bounces; }); }
            cy += rowH;
        }
        if (NumField("Fog", L.fogStrength, 0.01f, 0.f, 2.f, px, cy, fw)) { syncField(lt, [&](int i) { _level.lights[i].fogStrength = L.fogStrength; }); } cy += rowH;

        // Enabled toggle
        {
            Rectangle eb = { px, cy, fw, 16 };
            bool hov = CheckCollisionPointRec(GetMousePosition(), eb);
            Color c1 = L.enabled ? Color{ 30,80,30,255 } : Color{ 60,30,30,255 };
            Color c2 = L.enabled ? Color{ 100,220,100,255 } : Color{ 220,100,100,255 };
            DrawRectangleRec(eb, hov ? Color{ (unsigned char)(c1.r + 20), (unsigned char)(c1.g + 20), (unsigned char)(c1.b + 20), 255 } : c1);
            DrawRectangleLinesEx(eb, 1, c2);
            const char* ts = L.enabled ? "ENABLED (click to disable)" : "DISABLED (click to enable)";
            int tww = MeasureText(ts, 9);
            DrawText(ts, (int)(eb.x + eb.width / 2 - tww / 2), (int)eb.y + 4, 9, c2);
            if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); L.enabled = !L.enabled; syncField(lt, [&](int i) { _level.lights[i].enabled = L.enabled; }); }
            cy += 20;
        }

        // Copy / Paste props
        {
            float hw = (fw - 3) * 0.5f;
            Rectangle cpR = { px, cy, hw, 15 }, ppR = { px + hw + 3, cy, hw, 15 };
            bool cpH = CheckCollisionPointRec(GetMousePosition(), cpR);
            bool ppH = CheckCollisionPointRec(GetMousePosition(), ppR);
            bool canPaste = (_propClip.type == (int)EditorTool::POINT_LIGHT ||
                _propClip.type == (int)EditorTool::SPOT_LIGHT ||
                _propClip.type == (int)EditorTool::SKY_LIGHT);
            DrawRectangleRec(cpR, cpH ? Color{ 50,80,50,255 } : Color{ 30,50,30,255 });
            DrawRectangleLinesEx(cpR, 1, { 60,160,60,255 });
            DrawText("Cpy Props", (int)(cpR.x + 2), (int)(cpR.y + 2), 9, { 120,220,120,255 });
            DrawRectangleRec(ppR, canPaste ? (ppH ? Color{ 60,50,20,255 } : Color{ 40,35,15,255 }) : Color{ 25,28,38,255 });
            DrawRectangleLinesEx(ppR, 1, canPaste ? Color{ 220,180,60,255 } : Color{ 50,55,70,255 });
            DrawText("Pst Props", (int)(ppR.x + 2), (int)(ppR.y + 2), 9, canPaste ? Color{ 220,180,60,255 } : Color{ 80,85,100,255 });
            if (cpH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) CopyProps();
            if (ppH && canPaste && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); PasteProps(); }
            cy += 18;
        }
    }
    if (_sel.type == (int)EditorTool::PATH_NODE) {
        SectionHeader("── Node Settings ──────────");
        auto& n = _level.pathNodes[_sel.index];
        static const Color SLOT_COL[3] = { {255,140,0,255},{0,180,255,255},{200,60,200,255} };
        static const Color SLOT_DIM[3] = { {80,40,0,255},{0,50,100,255},{70,20,70,255} };
        DrawText(TextFormat("N0:%d  N1:%d  N2:%d", n.next[0], n.next[1], n.next[2]), (int)px, (int)cy + 2, 9, { 180,185,210,255 }); cy += rowH;
        float bw3 = (fw - 8) / 3.f;
        const char* btnLbl[3] = { "Set N0","Set N1","Set N2" };
        ConnectMode btnMode[3] = { ConnectMode::NEXT0,ConnectMode::NEXT1,ConnectMode::NEXT2 };
        for (int k = 0; k < 3; k++) {
            Rectangle rBtn = { px + k * (bw3 + 4), cy, bw3, 16 };
            bool hk = CheckCollisionPointRec(GetMousePosition(), rBtn);
            DrawRectangleRec(rBtn, hk ? SLOT_COL[k] : SLOT_DIM[k]);
            DrawText(btnLbl[k], (int)rBtn.x + 2, (int)rBtn.y + 2, 9, WHITE);
            if (hk && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { _connectMode = btnMode[k]; _connectFrom = _sel.index; SetStatus(TextFormat("Click node -> N%d", k)); }
        }
        cy += 20;
        float bwS = (fw - 4) / 2.f;
        // Split toggle (half width)
        Rectangle rSpl = { px, cy, bwS, 16 };
        bool hs = CheckCollisionPointRec(GetMousePosition(), rSpl);
        DrawRectangleRec(rSpl, n.isSplitNode ? (hs ? Color{20,120,20,255} : Color{10,80,10,255}) : (hs ? Color{60,60,80,255} : Color{35,38,55,255}));
        DrawText(n.isSplitNode ? "Split: ON" : "Split: OFF", (int)rSpl.x + 4, (int)rSpl.y + 2, 10, WHITE);
        if (hs && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); n.isSplitNode = !n.isSplitNode; }
        cy += 20;
        // Ender direction row — 5 equal buttons: Normal / Down / Right / Left / None
        {
            static const char* EDIR_LBL[5] = { "Nrm", "Down", "Rght", "Left", "None" };
            static const Color EDIR_ON[5]  = { {55,55,75,255},{0,160,180,255},{40,160,40,255},{190,120,20,255},{140,40,180,255} };
            static const Color EDIR_OFF[5] = { {30,32,45,255},{0,60,70,255},{15,55,15,255},{70,45,8,255},{55,15,70,255} };
            DrawText("Ender:", (int)px, (int)cy + 2, 9, {150,155,180,255});
            float bw5 = (fw - 4) / 5.f;
            for (int k = 0; k < 5; k++) {
                int dir = k - 1; // k==0 → -1 (normal), k==1 → 0 (down), ...
                Rectangle rb = { px + k*(bw5+1), cy, bw5, 16 };
                bool hov = CheckCollisionPointRec(GetMousePosition(), rb);
                bool sel = (n.enderDir == dir);
                Color bc = sel ? EDIR_ON[k] : (hov ? Color{EDIR_ON[k].r,(unsigned char)fminf(EDIR_ON[k].g+20,255),EDIR_ON[k].b,200} : EDIR_OFF[k]);
                DrawRectangleRec(rb, bc);
                if (sel) DrawRectangleLinesEx(rb, 1.f, WHITE);
                int tw = MeasureText(EDIR_LBL[k], 9); DrawText(EDIR_LBL[k], (int)(rb.x+(rb.width-tw)/2), (int)rb.y+3, 9, sel ? WHITE : Color{160,165,185,255});
                if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); n.enderDir = dir; }
            }
        }
        cy += 20;
        float rv = n.rollThreshold;
        if (NumField("Roll", rv, 0.05f, 0, 10, px, cy, fw)) { PushUndo(); n.rollThreshold = (int)roundf(rv); } cy += rowH;
        // Hint: E to extrude
        DrawText("[E] to extrude new node", (int)px, (int)cy + 1, 9, { 130,140,160,255 }); cy += rowH;
    }
    // DELETE button (only when entity is selected)
    {
        Rectangle delR = { px, cy + 4, fw, 18 };
        bool delH = CheckCollisionPointRec(GetMousePosition(), delR);
        DrawRectangleRec(delR, delH ? Color{ 180,30,30,255 } : Color{ 100,20,20,255 });
        int dtw = MeasureText("DELETE", 11); DrawText("DELETE", (int)(delR.x + delR.width / 2 - dtw / 2), (int)delR.y + 3, 11, WHITE);
        if (delH && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            _multiSel.empty() ? DeleteSelected() : DeleteMultiSelected();
    }
    } // end if (_sel.valid())

    // Edge selected panel
    if (_selEdge.valid() && _selEdge.from < (int)_level.pathNodes.size()) {
        SectionHeader("── Edge Settings ──────────");
        auto& srcNode = _level.pathNodes[_selEdge.from];
        int nb = srcNode.next[_selEdge.slot];
        DrawText(TextFormat("Edge %d -> %d (slot %d)", _selEdge.from, nb, _selEdge.slot), (int)px, (int)cy + 2, 9, { 180,185,210,255 }); cy += rowH;
        int& et = srcNode.edgeType[_selEdge.slot];
        float bwE = (fw - 4) / 2.f;
        Rectangle rNrm = { px, cy, bwE, 16 }, rLad = { px + bwE + 4, cy, bwE, 16 };
        bool hNrm = CheckCollisionPointRec(GetMousePosition(), rNrm);
        bool hLad = CheckCollisionPointRec(GetMousePosition(), rLad);
        DrawRectangleRec(rNrm, (et == 0) ? Color{ 60,160,60,255 } : (hNrm ? Color{ 40,80,40,255 } : Color{ 25,45,25,255 }));
        DrawText("NORMAL", (int)rNrm.x + 4, (int)rNrm.y + 2, 9, WHITE);
        DrawRectangleRec(rLad, (et == 1) ? Color{ 0,140,220,255 } : (hLad ? Color{ 0,60,100,255 } : Color{ 0,35,60,255 }));
        DrawText("LADDER", (int)rLad.x + 4, (int)rLad.y + 2, 9, WHITE);
        if (hNrm && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); et = 0; }
        if (hLad && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); et = 1; }
        cy += 20;
        Rectangle rDis = { px, cy, fw, 16 };
        bool hDis = CheckCollisionPointRec(GetMousePosition(), rDis);
        DrawRectangleRec(rDis, hDis ? Color{ 160,50,20,255 } : Color{ 90,30,10,255 });
        DrawText("Disconnect edge", (int)rDis.x + 4, (int)rDis.y + 2, 9, WHITE);
        if (hDis && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { PushUndo(); srcNode.next[_selEdge.slot] = -1; srcNode.edgeType[_selEdge.slot] = 0; _selEdge.clear(); }
    }

    // Track total content height so the scroll bar can be sized correctly
    _dataPanelContentH = (cy + (float)_dataPanelScroll) - (dr.y + 22.f);
    EndScissorMode();

    // Scroll bar (only when content overflows)
    if (_dataPanelContentH > dr.height - 18.f) {
        float barTrackH = dr.height - 18.f;
        float barFrac   = barTrackH / (_dataPanelContentH + 18.f);
        float barH      = fmaxf(20.f, barTrackH * barFrac);
        float maxScroll = fmaxf(0.f, _dataPanelContentH - barTrackH + 18.f);
        float barY      = dr.y + 18.f + (barTrackH - barH) * ((float)_dataPanelScroll / maxScroll);
        DrawRectangle((int)(dr.x + dr.width - 4), (int)(dr.y + 18), 4, (int)barTrackH, { 30,33,48,255 });
        DrawRectangle((int)(dr.x + dr.width - 4), (int)barY, 4, (int)barH, { 90,95,130,255 });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Master Draw
// ─────────────────────────────────────────────────────────────────────────────
void LevelEditor::Draw() {
    int canvasBottom = _seqOpen ? (_sh - SEQ_H) : (_sh - BROWSER_H);
    Rectangle canvasRect = { 0, (float)TOOLBAR_H,
                             (float)_canvasW, (float)(canvasBottom - TOOLBAR_H) };

    if (_lightingPreview) {
        // The editor camera has offset.y = TOOLBAR_H so world content draws
        // beneath the toolbar on screen. Inside the lighting scene RT we want
        // the canvas region only, so build a canvas-local camera with no
        // toolbar offset, then composite into canvasRect on screen.
        Camera2D lcam = _cam;
        lcam.offset.y = 0.f;

        _lighting.BeginScene(lcam);
        DrawBackground();
        DrawGrid();
        DrawLevelLitContent();
        _lighting.EndScene();

        _lighting.BakeOccludersFromLevel(_level, lcam);
        _lighting.Composite(_level, lcam, canvasRect);

        // Editor overlays drawn unlit, in the editor's normal camera space.
        BeginMode2D(_cam);
        DrawLevelOverlays();
        DrawPlacementPreview();
        EndMode2D();
    }
    else {
        BeginMode2D(_cam);
        DrawBackground(); DrawGrid(); DrawLevelEntities(); DrawPlacementPreview();
        EndMode2D();
    }

    DrawRectangleLinesEx(canvasRect, 1, { 60,70,100,200 });

    DrawToolbarUI();

    if (_seqOpen) DrawSequencer();
    else         DrawBrowserUI();

    DrawLine(_canvasW, TOOLBAR_H, _canvasW, _sh - BROWSER_H, { 70,80,110,255 });
    DrawOutliner();
    DrawDataPanel();

    {
        Rectangle r = { (float)(_sw - RIGHT_W - 52),(float)(TOOLBAR_H + 1),50,12 };
        bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, _seqOpen ? Color{ 30,80,120,255 } : Color{ 20,22,32,255 });
        DrawRectangleLinesEx(r, 1, _seqOpen ? Color{ 60,160,220,255 } : Color{ 55,60,85,255 });
        int tw = MeasureText("TAB:SEQ", 9); DrawText("TAB:SEQ", (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + 2), 9, _seqOpen ? Color{ 60,200,255,255 } : Color{ 100,105,130,255 });
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            _seqOpen = !_seqOpen;
            if (!_seqOpen) SeqPreviewStop();
            SeqLoad();
        }
    }

    // Lighting preview toggle button (next to TAB:SEQ)
    {
        Rectangle r = { (float)(_sw - RIGHT_W - 108),(float)(TOOLBAR_H + 1),52,12 };
        bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        DrawRectangleRec(r, _lightingPreview ? Color{ 100,80,20,255 } : Color{ 20,22,32,255 });
        DrawRectangleLinesEx(r, 1, _lightingPreview ? Color{ 255,200,80,255 } : Color{ 55,60,85,255 });
        int tw = MeasureText("F8:LIGHT", 9);
        DrawText("F8:LIGHT", (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + 2), 9,
            _lightingPreview ? Color{ 255,220,120,255 } : Color{ 100,105,130,255 });
        if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) _lightingPreview = !_lightingPreview;
    }

    // Diagnostics overlay: counts and on/off state — top-left of canvas
    if (_lightingPreview) {
        int total = (int)_level.lights.size();
        int enabled = 0;
        for (const auto& L : _level.lights) if (L.enabled) enabled++;
        const char* msg = TextFormat("LIGHTING ON  |  %d lights (%d enabled)",
            total, enabled);
        int tw = MeasureText(msg, 10);
        DrawRectangle(2, TOOLBAR_H + 2, tw + 10, 16, { 0, 0, 0, 180 });
        DrawText(msg, 6, TOOLBAR_H + 5, 10, { 255, 220, 120, 255 });
        if (total == 0) {
            const char* hint = "Place a POINT/SPOT/SKY light from the bottom toolbar.";
            int tw2 = MeasureText(hint, 11);
            DrawRectangle(_canvasW / 2 - tw2 / 2 - 8, TOOLBAR_H + 26, tw2 + 16, 20, { 0, 0, 0, 180 });
            DrawText(hint, _canvasW / 2 - tw2 / 2, TOOLBAR_H + 31, 11, { 200, 200, 220, 255 });
        }
    }

    if (_connectMode != ConnectMode::NONE) {
        const char* h = (_connectMode == ConnectMode::NEXT0) ? ">> Click node -> NEXT[0]  (ESC=cancel)"
                      : (_connectMode == ConnectMode::NEXT1) ? ">> Click node -> NEXT[1]  (ESC=cancel)"
                                                             : ">> Click node -> NEXT[2]  (ESC=cancel)";
        Color hc = (_connectMode == ConnectMode::NEXT0) ? Color{ 255,140,0,255 }
                 : (_connectMode == ConnectMode::NEXT1) ? Color{ 0,200,255,255 }
                                                        : Color{ 220,80,220,255 };
        int tw = MeasureText(h, 13), tx = _canvasW / 2 - tw / 2, ty = _sh / 2 - 10;
        DrawRectangle(tx - 8, ty - 5, tw + 16, 24, { 0,0,0,200 });
        DrawText(h, tx, ty, 13, hc);
    }
    if (_multiSel.size() > 1) {
        const char* ms = TextFormat("%d selected", (int)_multiSel.size());
        int tw = MeasureText(ms, 12), tx = _canvasW / 2 - tw / 2;
        DrawRectangle(tx - 6, TOOLBAR_H + 18, tw + 12, 20, { 0,0,0,180 }); DrawText(ms, tx, TOOLBAR_H + 21, 12, { 255,200,0,255 });
    }
    if (_directOp != DirectOp::NONE) {
        const char* opn[] = { "","GRAB (G)","ROTATE (R)","SCALE (S)" };
        const char* axs = _grabAxisX ? " [X]" : _grabAxisY ? " [Y]" : "";
        const char* gh = TextFormat("%s%s — Enter/LMB:confirm  ESC/RMB:cancel", opn[(int)_directOp], axs);
        int tw = MeasureText(gh, 13), tx = _canvasW / 2 - tw / 2;
        DrawRectangle(tx - 8, TOOLBAR_H + 6, tw + 16, 20, { 0,0,0,200 }); DrawText(gh, tx, TOOLBAR_H + 9, 13, YELLOW);
    }
}

// =============================================================================
//  CINEMATIC SEQUENCER
// =============================================================================

Rectangle LevelEditor::SeqBrowserRect() const { return { 0,(float)(_sh - SEQ_H),(float)SEQ_BW,(float)SEQ_H }; }
Rectangle LevelEditor::SeqControlsRect() const { return { (float)SEQ_BW,(float)(_sh - SEQ_H),(float)(_canvasW - SEQ_BW),(float)SEQ_CTRL_H }; }
Rectangle LevelEditor::SeqTimelineRect() const {
    int top = _sh - SEQ_H + SEQ_CTRL_H;
    return { (float)(SEQ_BW + SEQ_HDR),(float)top,(float)(_canvasW - SEQ_BW - SEQ_HDR),(float)(SEQ_H - SEQ_CTRL_H) };
}
Rectangle LevelEditor::SeqTrackRowRect(int row) const {
    Rectangle tl = SeqTimelineRect();
    return { (float)SEQ_BW, tl.y + SEQ_RH + row * SEQ_TH,(float)(_canvasW - SEQ_BW), SEQ_TH };
}
float LevelEditor::SeqTimeToX(float t) const { Rectangle tl = SeqTimelineRect(); return tl.x + (t - _seqScrollX) * _seqZoom; }
float LevelEditor::SeqXToTime(float px) const { Rectangle tl = SeqTimelineRect(); return _seqScrollX + (px - tl.x) / _seqZoom; }

void LevelEditor::SeqLoad() {
    if (!_seqOpen) return;
    auto names = ListCinematics();
    _seqList.clear();
    for (const auto& n : names) { CinematicSequence s; if (LoadCinematic(s, n.c_str())) _seqList.push_back(s); }
    if (_activeSeq >= (int)_seqList.size()) _activeSeq = (int)_seqList.size() - 1;
}
void LevelEditor::SeqSave() {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    SaveCinematic(_seqList[_activeSeq]);
    SetStatus(TextFormat("Cinematic '%s' saved.", _seqList[_activeSeq].name));
}
void LevelEditor::SeqNew() {
    CinematicSequence s;
    snprintf(s.name, sizeof(s.name), "Sequence%d", _seqNameCounter++);
    s.duration = 5.f; s.endMode = CinematicEndMode::LOOP; s.valid = true;
    _seqList.push_back(s); _activeSeq = (int)_seqList.size() - 1;
    _seqInPoint = 0.f; _seqOutPoint = s.duration;
    SaveCinematic(_seqList[_activeSeq]);
    SetStatus(TextFormat("New cinematic: %s", s.name));
}
void LevelEditor::SeqDelete() {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    DeleteCinematic(_seqList[_activeSeq].name);
    _seqList.erase(_seqList.begin() + _activeSeq);
    _activeSeq = _seqList.empty() ? -1 : std::min(_activeSeq, (int)_seqList.size() - 1);
    SetStatus("Cinematic deleted.");
}

void LevelEditor::SeqAddKeyframe() {
    if (_activeSeq < 0 || !_sel.valid()) return;
    auto& seq = _seqList[_activeSeq];
    CinematicKeyframe kf; kf.time = _seqTime;
    Vector2 pos = GetEntPos(_sel); kf.x = pos.x; kf.y = pos.y;
    int i = _sel.index;
    if (_sel.type == (int)EditorTool::PLATFORM && i < (int)_level.platforms.size()) {
        kf.tilt = _level.platforms[i].tilt; kf.width = _level.platforms[i].w; kf.height = _level.platforms[i].h;
    }
    else if (_sel.type == (int)EditorTool::LADDER && i < (int)_level.ladders.size()) {
        kf.height = _level.ladders[i].h; kf.width = _level.ladders[i].w;
    }
    int trackIdx = -1;
    for (int t = 0; t < (int)seq.tracks.size(); t++)
        if (seq.tracks[t].entityType == _sel.type && seq.tracks[t].entityIndex == _sel.index) { trackIdx = t; break; }
    if (trackIdx < 0) {
        CinematicTrack tr; tr.entityType = _sel.type; tr.entityIndex = _sel.index;
        snprintf(tr.name, sizeof(tr.name), "%s %d", ToolName((EditorTool)_sel.type), _sel.index);
        seq.tracks.push_back(tr); trackIdx = (int)seq.tracks.size() - 1;
    }
    auto& track = seq.tracks[trackIdx];
    bool replaced = false;
    for (auto& k : track.keys) { if (fabsf(k.time - _seqTime) < 0.01f) { k = kf; replaced = true; break; } }
    if (!replaced) track.keys.push_back(kf);
    std::sort(track.keys.begin(), track.keys.end(), [](const CinematicKeyframe& a, const CinematicKeyframe& b) { return a.time < b.time; });
    _selTrack = trackIdx;
    SetStatus(TextFormat("Keyframe @ %.2fs  [%s]", _seqTime, track.name));
}

void LevelEditor::SeqDeleteKeyframe() {
    if (_activeSeq < 0 || _selTrack < 0 || _selKey < 0) return;
    auto& seq = _seqList[_activeSeq];
    if (_selTrack >= (int)seq.tracks.size()) return;
    auto& tr = seq.tracks[_selTrack];
    if (_selKey >= (int)tr.keys.size()) return;
    tr.keys.erase(tr.keys.begin() + _selKey); _selKey = -1;
    SetStatus("Keyframe deleted.");
}

void LevelEditor::SeqPreviewUpdate(float dt) {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    const auto& seq = _seqList[_activeSeq];
    _seqTime += dt;
    float outPt = (_seqOutPoint >= 0.f) ? _seqOutPoint : seq.duration;
    outPt = std::min(outPt, seq.duration);
    switch (seq.endMode) {
    case CinematicEndMode::LOOP:  if (_seqTime > outPt) _seqTime = _seqInPoint; break;
    case CinematicEndMode::STAY:  if (_seqTime > outPt) _seqTime = outPt; break;
    case CinematicEndMode::RESET: if (_seqTime > outPt) { SeqPreviewStop(); return; } break;
    }
    auto states = EvaluateCinematic(seq, _seqTime);
    for (const auto& st : states) {
        int idx = st.entityIndex;
        switch (st.entityType) {
        case 4: if (idx >= 0 && idx < (int)_level.platforms.size()) { _level.platforms[idx].x = st.x; _level.platforms[idx].y = st.y; if (st.tilt != 0.f) _level.platforms[idx].tilt = st.tilt; if (st.width > 0.f) _level.platforms[idx].w = st.width; } break;
        case 5: if (idx >= 0 && idx < (int)_level.ladders.size()) { _level.ladders[idx].x = st.x; _level.ladders[idx].y = st.y; } break;
        case 6: if (idx >= 0 && idx < (int)_level.beams.size()) { _level.beams[idx].x = st.x; _level.beams[idx].y = st.y; } break;
        case 1: _level.playerSpawn = { st.x, st.y }; break;
        case 2: _level.regulusPos = { st.x, st.y }; break;
        case 3: _level.cavePos = { st.x, st.y }; break;
        }
    }
}

void LevelEditor::SeqPreviewStop() {
    if (_seqPlaying) _level = _seqSnapshot;
    _seqPlaying = false; _seqTime = _seqInPoint;
}

void LevelEditor::UpdateSequencer() {
    if (!_seqOpen) return;
    Vector2 mp = GetMousePosition();
    bool lP = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool lD = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool lR = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    bool rP = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);
    bool rD = IsMouseButtonDown(MOUSE_RIGHT_BUTTON);
    bool rR = IsMouseButtonReleased(MOUSE_RIGHT_BUTTON);
    float wheel = GetMouseWheelMove();
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    if (IsKeyPressed(KEY_SPACE) && _activeSeq >= 0) {
        if (!_seqPlaying) { _seqSnapshot = _level; _seqPlaying = true; }
        else { _seqPlaying = false; } return;
    }
    if (ctrl && IsKeyPressed(KEY_S)) { SeqSave(); return; }
    if (!ctrl && IsKeyPressed(KEY_S) && _sel.valid() && _activeSeq >= 0) { SeqAddKeyframe(); return; }
    if ((IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) && _selKey >= 0) { SeqDeleteKeyframe(); return; }

    Rectangle br = SeqBrowserRect();
    if (CheckCollisionPointRec(mp, br)) {
        if (wheel != 0) {
            int vis = (int)((br.height - 50) / 18);
            int maxS = std::max(0, (int)_seqList.size() - vis);
            _seqTrackScroll = std::max(0, std::min((int)(_seqTrackScroll - wheel), maxS));
        }
        if (lP) {
            Rectangle btnNew = { br.x + 4, br.y + 4, 36, 16 };
            Rectangle btnDel = { br.x + 44, br.y + 4, 36, 16 };
            if (CheckCollisionPointRec(mp, btnNew)) { SeqNew(); return; }
            if (CheckCollisionPointRec(mp, btnDel)) { SeqDelete(); return; }
            for (int r = 0; r < 30; r++) {
                int idx = r + _seqTrackScroll;
                if (idx >= (int)_seqList.size()) break;
                Rectangle rr = { br.x + 2, br.y + 24 + (float)r * 18, br.width - 4, 17 };
                if (CheckCollisionPointRec(mp, rr)) { _activeSeq = idx; _selTrack = -1; _selKey = -1; _seqInPoint = 0.f; _seqOutPoint = _seqList[idx].duration; SeqPreviewStop(); return; }
            }
        }
        return;
    }

    if (_activeSeq < 0) return;
    auto& seq = _seqList[_activeSeq];
    float outPt = (_seqOutPoint >= 0.f) ? _seqOutPoint : seq.duration;

    Rectangle cr = SeqControlsRect();
    if (CheckCollisionPointRec(mp, cr) && lP) {
        float bw = cr.width / 9.f;
        for (int b = 0; b < 9; b++) {
            Rectangle rb = { cr.x + b * bw + 1, cr.y + 2, bw - 2, cr.height - 4 };
            if (!CheckCollisionPointRec(mp, rb)) continue;
            switch (b) {
            case 0: _seqTime = _seqInPoint; break;
            case 1: if (!_seqPlaying) { _seqSnapshot = _level; _seqPlaying = true; } break;
            case 2: _seqPlaying = !_seqPlaying; break;
            case 3: SeqPreviewStop(); break;
            case 4: if (seq.duration > 0.5f) { seq.duration -= 0.5f; _seqOutPoint = seq.duration; } break;
            case 5: seq.duration += 0.5f; if (_seqOutPoint < 0.f) _seqOutPoint = seq.duration; break;
            case 6: seq.endMode = CinematicEndMode::LOOP; break;
            case 7: seq.endMode = CinematicEndMode::STAY; break;
            case 8: seq.endMode = CinematicEndMode::RESET; break;
            }
        }
        return;
    }

    Rectangle tl = SeqTimelineRect();
    bool inSeqArea = (mp.y >= _sh - SEQ_H && mp.x < _canvasW);
    if (inSeqArea) {
        if (rP) { _seqPanning = true; _seqPanStartX = mp.x; _seqPanStartSX = _seqScrollX; }
        if (_seqPanning && rD) _seqScrollX = std::max(0.f, _seqPanStartSX - (mp.x - _seqPanStartX) / _seqZoom);
        if (rR) _seqPanning = false;
        if (IsMouseButtonDown(MOUSE_MIDDLE_BUTTON) || ctrl) { if (wheel != 0) _seqZoom = std::max(15.f, std::min(400.f, _seqZoom + wheel * 12.f)); }
        else if (wheel != 0) _seqScrollX = std::max(0.f, _seqScrollX - wheel * 0.5f);
    }

    Rectangle ruler = { tl.x, tl.y, tl.width, SEQ_RH };
    auto NearX = [&](float t) { return fabsf(SeqTimeToX(t) - mp.x) < 9.f && mp.y >= ruler.y - 4.f && mp.y <= ruler.y + ruler.height + 8.f; };
    if (lP && mp.y >= tl.y - 4 && mp.y <= tl.y + SEQ_RH + 8 && mp.x >= tl.x) {
        if (NearX(_seqInPoint)) _seqDragIn = true;
        else if (NearX(outPt))  _seqDragOut = true;
        else                    _seqDragPlayhead = true;
    }
    if (_seqDragIn && lD) _seqInPoint = std::max(0.f, std::min(outPt - 0.05f, SeqXToTime(mp.x)));
    if (_seqDragOut && lD) _seqOutPoint = std::max(_seqInPoint + 0.05f, std::min(seq.duration, SeqXToTime(mp.x)));
    if (_seqDragPlayhead && lD) _seqTime = std::max(0.f, std::min(seq.duration, SeqXToTime(mp.x)));
    if (lR) { _seqDragPlayhead = false; _seqDragIn = false; _seqDragOut = false; }

    if (lP && !_seqDragPlayhead && !_seqDragIn && !_seqDragOut) {
        _selKey = -1;
        for (int ti = 0; ti < (int)seq.tracks.size(); ti++) {
            Rectangle row = SeqTrackRowRect(ti);
            if (!CheckCollisionPointRec(mp, { tl.x, row.y, tl.width, row.height })) continue;
            for (int ki = 0; ki < (int)seq.tracks[ti].keys.size(); ki++) {
                float kx = SeqTimeToX(seq.tracks[ti].keys[ki].time);
                if (fabsf(kx - mp.x) < 7.f) { _selTrack = ti; _selKey = ki; _seqKeyDragOrig = seq.tracks[ti].keys[ki].time; _seqDragKey = true; break; }
            }
            if (mp.x < tl.x) _selTrack = ti;
        }
    }
    if (_seqDragKey && lD && _selTrack >= 0 && _selKey >= 0 && _selTrack < (int)seq.tracks.size()) {
        float newT = std::max(0.f, std::min(seq.duration, SeqXToTime(mp.x)));
        seq.tracks[_selTrack].keys[_selKey].time = newT;
        std::sort(seq.tracks[_selTrack].keys.begin(), seq.tracks[_selTrack].keys.end(), [](const CinematicKeyframe& a, const CinematicKeyframe& b) { return a.time < b.time; });
    }
    if (lR) _seqDragKey = false;
    if (_seqPlaying) SeqPreviewUpdate(GetFrameTime());
}

void LevelEditor::DrawSeqBrowser() {
    Rectangle br = SeqBrowserRect();
    DrawRectangleRec(br, { 18,20,30,255 }); DrawRectangleLinesEx(br, 1, { 55,60,85,255 });
    DrawText("CINEMATICS", (int)br.x + 4, (int)br.y - 1, 10, { 150,155,180,255 });
    auto Btn = [&](const char* lbl, float x, float w, Color c) {
        Rectangle r = { x, br.y + 4, w, 16 }; bool hov = CheckCollisionPointRec(GetMousePosition(), r);
        Color bg = hov ? Color{ (unsigned char)std::min(255,(int)c.r + 30),(unsigned char)std::min(255,(int)c.g + 30),(unsigned char)std::min(255,(int)c.b + 30),255 } : c;
        DrawRectangleRec(r, bg); int tw = MeasureText(lbl, 9); DrawText(lbl, (int)(x + w / 2 - tw / 2), (int)(br.y + 7), 9, WHITE);
        };
    Btn("[+]", br.x + 4, 34, { 30,80,50,255 }); Btn("[x]", br.x + 42, 34, { 80,30,30,255 });
    Vector2 mp = GetMousePosition();
    for (int r = 0; r < 30; r++) {
        int idx = r + _seqTrackScroll;
        if (idx >= (int)_seqList.size()) break;
        bool act = (_activeSeq == idx), hov = CheckCollisionPointRec(mp, { br.x + 2, br.y + 24 + (float)r * 18, br.width - 4, 17 });
        DrawRectangle((int)br.x + 2, (int)(br.y + 24 + r * 18), (int)br.width - 4, 17, act ? Color{ 35,60,100,255 } : hov ? Color{ 30,35,50,255 } : Color{ 20,22,32,255 });
        DrawText(_seqList[idx].name, (int)br.x + 6, (int)(br.y + 27 + r * 18), 10, act ? WHITE : Color{ 140,145,170,255 });
        Color emc = (_seqList[idx].endMode == CinematicEndMode::LOOP) ? GREEN : (_seqList[idx].endMode == CinematicEndMode::STAY) ? YELLOW : ORANGE;
        DrawCircleV({ br.x + br.width - 8, br.y + 33 + (float)r * 18 }, 4, emc);
    }
}

void LevelEditor::DrawSeqControls() {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    const auto& seq = _seqList[_activeSeq];
    Rectangle cr = SeqControlsRect();
    DrawRectangle((int)cr.x, (int)cr.y, (int)cr.width, (int)cr.height, { 25,28,40,255 });
    DrawLine((int)cr.x, (int)cr.y, (int)(cr.x + cr.width), (int)cr.y, { 55,60,85,255 });
    float bw = cr.width / 9.f;
    const char* labels[] = { "|<", _seqPlaying ? "||" : ">", "[]", "D-", "D+", "LOOP", "STAY", "RST", _seqPlaying ? "LIVE" : "PREV" };
    bool isLoop = (seq.endMode == CinematicEndMode::LOOP), isStay = (seq.endMode == CinematicEndMode::STAY), isReset = (seq.endMode == CinematicEndMode::RESET);
    Color bcs[9] = { {50,50,70,255}, _seqPlaying ? Color{30,100,60,255} : Color{50,80,50,255}, {80,30,30,255}, {45,45,65,255},{45,45,65,255},
        isLoop ? Color{20,100,40,255} : Color{35,38,55,255}, isStay ? Color{100,90,20,255} : Color{35,38,55,255}, isReset ? Color{100,40,20,255} : Color{35,38,55,255}, {30,50,80,255} };
    Vector2 mp = GetMousePosition();
    for (int b = 0; b < 9; b++) {
        Rectangle rb = { cr.x + b * bw + 1, cr.y + 2, bw - 2, cr.height - 4 }; bool hov = CheckCollisionPointRec(mp, rb);
        Color bc = bcs[b]; if (hov) { bc.r = (unsigned char)std::min(255, bc.r + 25); bc.g = (unsigned char)std::min(255, bc.g + 25); bc.b = (unsigned char)std::min(255, bc.b + 25); }
        DrawRectangleRec(rb, bc); int tw = MeasureText(labels[b], 9); DrawText(labels[b], (int)(rb.x + rb.width / 2 - tw / 2), (int)(rb.y + rb.height / 2 - 5), 9, WHITE);
    }
    float outPt = (_seqOutPoint >= 0.f) ? _seqOutPoint : seq.duration;
    DrawText(TextFormat("t:%.2f  I:%.2f  O:%.2f  dur:%.2f", _seqTime, _seqInPoint, outPt, seq.duration), (int)(cr.x + cr.width - 230), (int)(cr.y + cr.height / 2 - 5), 9, { 180,185,210,255 });
}

void LevelEditor::DrawSeqTimeline() {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    const auto& seq = _seqList[_activeSeq];
    float outPt = (_seqOutPoint >= 0.f) ? _seqOutPoint : seq.duration;
    Rectangle tl = SeqTimelineRect();
    DrawRectangleRec(tl, { 16,18,26,255 }); DrawRectangleLinesEx(tl, 1, { 55,60,85,255 });
    for (int ti = 0; ti < (int)seq.tracks.size(); ti++) {
        Rectangle row = SeqTrackRowRect(ti);
        Rectangle hdr = { (float)SEQ_BW, row.y, (float)SEQ_HDR, row.height };
        bool sel = (_selTrack == ti);
        DrawRectangleRec(hdr, sel ? Color{ 35,55,85,255 } : Color{ 22,24,34,255 });
        DrawLine((int)hdr.x, (int)(hdr.y + hdr.height), (int)(hdr.x + SEQ_HDR + tl.width), (int)(hdr.y + hdr.height), { 38,42,58,255 });
        Color tc = ToolColor((EditorTool)seq.tracks[ti].entityType);
        DrawRectangle((int)hdr.x, (int)hdr.y, 3, (int)hdr.height, tc);
        DrawText(seq.tracks[ti].name, (int)(hdr.x + 6), (int)(hdr.y + 4), 9, sel ? WHITE : Color{ 160,165,190,255 });
        DrawRectangleRec({ tl.x,row.y,tl.width,row.height }, ti % 2 ? Color{ 18,20,30,255 } : Color{ 20,22,32,255 });
    }
    BeginScissorMode((int)tl.x, (int)tl.y, (int)tl.width, (int)tl.height);
    { float inX = SeqTimeToX(_seqInPoint); if (inX > tl.x) DrawRectangle((int)tl.x, (int)tl.y, (int)(inX - tl.x), (int)tl.height, { 0,0,0,80 }); }
    { float outX = SeqTimeToX(outPt), endX = tl.x + tl.width; if (outX < endX) DrawRectangle((int)outX, (int)tl.y, (int)(endX - outX), (int)tl.height, { 0,0,0,80 }); }
    DrawRectangle((int)tl.x, (int)tl.y, (int)tl.width, (int)SEQ_RH, { 28,32,46,255 });
    float tickStep = (_seqZoom > 80) ? 0.25f : (_seqZoom > 30) ? 0.5f : 1.f;
    for (float t2 = 0.f; t2 <= seq.duration + 0.01f; t2 += tickStep) {
        float x = SeqTimeToX(t2); if (x < tl.x || x > tl.x + tl.width) continue;
        bool major = (fmodf(t2, 1.f) < 0.005f);
        DrawLine((int)x, (int)tl.y, (int)x, (int)(tl.y + SEQ_RH * (major ? 1.f : 0.5f)), major ? Color{ 100,110,140,255 } : Color{ 55,60,80,255 });
        if (major) DrawText(TextFormat("%.0f", t2), (int)x + 2, (int)tl.y + 2, 8, { 120,125,155,255 });
    }
    { float dx = SeqTimeToX(seq.duration); DrawLine((int)dx, (int)tl.y, (int)dx, (int)(tl.y + tl.height), { 100,40,40,180 }); }
    for (int ti = 0; ti < (int)seq.tracks.size(); ti++) {
        Rectangle row = SeqTrackRowRect(ti); float cy2 = row.y + row.height * 0.5f;
        Color tc = ToolColor((EditorTool)seq.tracks[ti].entityType);
        if (seq.tracks[ti].keys.size() >= 2) { float x0 = SeqTimeToX(seq.tracks[ti].keys.front().time), x1 = SeqTimeToX(seq.tracks[ti].keys.back().time); DrawLineEx({ x0,cy2 }, { x1,cy2 }, 1.5f, { (unsigned char)(tc.r / 2),(unsigned char)(tc.g / 2),(unsigned char)(tc.b / 2),180 }); }
        for (int ki = 0; ki < (int)seq.tracks[ti].keys.size(); ki++) {
            float kx = SeqTimeToX(seq.tracks[ti].keys[ki].time); bool ksel = (_selTrack == ti && _selKey == ki);
            DrawPoly({ kx,cy2 }, 4, ksel ? 8.f : 6.f, 45.f, ksel ? YELLOW : tc);
            DrawPolyLines({ kx,cy2 }, 4, ksel ? 8.f : 6.f, 45.f, ksel ? WHITE : Color{ (unsigned char)std::min(255,tc.r + 80),(unsigned char)std::min(255,tc.g + 80),(unsigned char)std::min(255,tc.b + 80),255 });
        }
    }
    { float inX = SeqTimeToX(_seqInPoint); DrawLine((int)inX, (int)tl.y, (int)inX, (int)(tl.y + tl.height), { 50,220,80,255 }); DrawTriangle({ inX - 6,tl.y + SEQ_RH }, { inX + 6,tl.y + SEQ_RH }, { inX,tl.y + SEQ_RH + 10 }, { 50,220,80,255 }); DrawText("I", (int)inX + 3, (int)tl.y + 3, 8, { 50,220,80,255 }); }
    { float outX = SeqTimeToX(outPt); DrawLine((int)outX, (int)tl.y, (int)outX, (int)(tl.y + tl.height), { 220,50,50,255 }); DrawTriangle({ outX - 6,tl.y + SEQ_RH }, { outX + 6,tl.y + SEQ_RH }, { outX,tl.y + SEQ_RH + 10 }, { 220,50,50,255 }); DrawText("O", (int)outX + 3, (int)tl.y + 3, 8, { 220,50,50,255 }); }
    { float phx = SeqTimeToX(_seqTime); DrawLine((int)phx, (int)tl.y, (int)phx, (int)(tl.y + tl.height), { 255,200,50,255 }); DrawTriangle({ phx - 7,tl.y }, { phx + 7,tl.y }, { phx,tl.y + SEQ_RH }, { 255,200,50,255 }); }
    EndScissorMode();
}

void LevelEditor::DrawSequencer() {
    DrawRectangle(0, _sh - SEQ_H, _canvasW, SEQ_H, { 16,18,26,255 });
    DrawLine(0, _sh - SEQ_H, _canvasW, _sh - SEQ_H, { 70,80,110,255 });
    DrawSeqBrowser();
    if (_activeSeq >= 0 && _activeSeq < (int)_seqList.size()) { DrawSeqControls(); DrawSeqTimeline(); }
    else DrawText("Select or create a sequence →", SEQ_BW + 10, _sh - SEQ_H + SEQ_H / 2 - 8, 11, { 80,85,110,255 });
    if (_selTrack >= 0 && _selKey >= 0 && _activeSeq >= 0 && _activeSeq < (int)_seqList.size()) {
        const auto& seq = _seqList[_activeSeq];
        if (_selTrack < (int)seq.tracks.size() && _selKey < (int)seq.tracks[_selTrack].keys.size()) {
            const auto& k = seq.tracks[_selTrack].keys[_selKey];
            DrawText(TextFormat("Key t=%.3f  x=%.1f  y=%.1f  tilt=%.1f  w=%.1f  h=%.1f   [Del]=remove  drag=retime", k.time, k.x, k.y, k.tilt, k.width, k.height), SEQ_BW + 4, _sh - 13, 10, { 200,205,230,255 });
        }
    }
    else DrawText("SPACE=play/pause  S=keyframe  Del=remove  RMB+drag=pan  MWheel=scroll  Ctrl+Wheel or MMB=zoom  I/O=in-out points", SEQ_BW + 4, _sh - 13, 9, { 80,85,110,255 });
}

// =============================================================================
//  ELEVATOR ENTITY
// =============================================================================

Rectangle LevelEditor::ElevRect(const ElevatorData& el) const { return { el.x, el.y, el.w, el.h }; }

void LevelEditor::DrawElevatorEnt(const ElevatorData& el, bool sel, bool msel) const {
    const float sc = 4.f;
    Color bc = sel ? YELLOW : (msel ? Color{ 255,200,0,240 } : Color{ 255,140,50,220 });
    if (!el.invisible) {
        if (_ropeTex && _ropeTex->id > 0) {
            float tw = _ropeTex->width * sc, th = _ropeTex->height * sc;
            float drawX = el.x + el.w * 0.5f - tw * 0.5f;
            float rawPan = (float)GetTime() * el.speed * (el.direction == 1 ? 1.f : -1.f);
            float panOff = fmodf(rawPan, th); if (panOff < 0.f) panOff += th;
            float startY = el.y - th + panOff;
            for (float y = startY; y < el.y + el.h; y += th) {
                float dy = fmaxf(y, el.y), dyEnd = fminf(y + th, el.y + el.h);
                if (dy >= dyEnd) continue;
                float srcYOff = (dy - y) / sc, srcH = (dyEnd - dy) / sc;
                DrawTexturePro(*_ropeTex, { 0, srcYOff, (float)_ropeTex->width, srcH }, { drawX, dy, tw, dyEnd - dy }, {}, 0.f, WHITE);
            }
        }
        DrawRectangle((int)el.x, (int)el.y, (int)el.w, (int)el.h, Color{ 80, 55, 30, 255 });
        DrawRectangle((int)el.x, (int)el.y, (int)el.w, (int)el.h, sel ? Color{ 255,160,50,35 } : Color{ 255,120,30,20 });
    } else {
        // invisible: draw a dashed outline so it's visible in the editor only
        DrawRectangleLinesEx({ el.x, el.y, el.w, el.h }, 1.f, Color{ 180,100,30,80 });
    }
    DrawRectangleLinesEx({ el.x, el.y, el.w, el.h }, sel ? 2.f : 1.5f, bc);
    float cx = el.x + el.w * 0.5f, cy2 = el.y + el.h * 0.5f;
    if (el.horizontal) {
        float ax = (el.direction == 1) ? 14.f : -14.f;
        DrawTriangle({ cx - ax * 0.4f, cy2 - 7 }, { cx - ax * 0.4f, cy2 + 7 }, { cx + ax, cy2 }, bc);
    } else {
        float ay = (el.direction == 1) ? -14.f : 14.f;
        DrawTriangle({ cx - 7, cy2 - ay * 0.4f }, { cx + 7, cy2 - ay * 0.4f }, { cx, cy2 + ay }, bc);
    }
    const char* modeStr = el.backAndForth ? (el.horizontal ? "H<>" : "V<>") : (el.horizontal ? "H->" : "V");
    char buf[64]; snprintf(buf, sizeof(buf), "%.0fpx/s %s%s", el.speed,
        el.direction == 1 ? (el.horizontal ? "R" : "UP") : (el.horizontal ? "L" : "DN"), el.backAndForth ? " B&F" : "");
    DrawText(buf, (int)el.x + 2, (int)(el.y + el.h + 2), 9, bc);
    (void)modeStr;
    SelectedEnt e = { (int)EditorTool::ELEVATOR, (int)(&el - _level.elevators.data()) };
    for (const auto& rel : _level.relations) {
        if (!(rel.parent == e)) continue;
        Vector2 childPos = GetEntPos(rel.child);
        DrawLineEx({ cx, el.y + el.h * 0.5f }, childPos, 1.f, { 255,180,80,120 });
    }
}

// =============================================================================
//  WIN ZONE  (fixed: Rectangle uses .width/.height, not .w/.h)
// =============================================================================

Rectangle LevelEditor::WinZoneRect(const WinZoneData& wz) const { return { wz.x, wz.y, wz.w, wz.h }; }

void LevelEditor::DrawWinZoneEnt(const WinZoneData& wz, bool sel, bool msel) const {
    Rectangle r = WinZoneRect(wz);
    Color fill = sel ? Color{ 80, 255, 140, 50 } : msel ? Color{ 80, 255, 140, 40 } : Color{ 80, 255, 140, 22 };
    Color bc = sel ? Color{ 120, 255, 160, 255 } : msel ? Color{ 255, 220, 0, 240 } : Color{ 80, 220, 120, 200 };
    DrawRectangleRec(r, fill);
    DrawRectangleLinesEx(r, sel ? 2.5f : 1.5f, bc);
    for (float ox = 0; ox < r.width + r.height; ox += 14.f) {
        float x1 = r.x + ox, y1 = r.y;
        float x2 = r.x + ox - r.height, y2 = r.y + r.height;
        x1 = fmaxf(x1, r.x); x2 = fmaxf(x2, r.x);
        x1 = fminf(x1, r.x + r.width); x2 = fminf(x2, r.x + r.width);
        DrawLineEx({ x1, y1 }, { x2, y2 }, 1.f, { 80, 255, 140, 30 });
    }
    float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
    DrawCircleV({ cx, cy }, 8.f, { 80, 255, 140, 180 });
    DrawCircleLines((int)cx, (int)cy, 8, bc);
    DrawText("WIN", (int)(r.x + 3), (int)(r.y + 2), 9, bc);
    char buf[32]; snprintf(buf, sizeof(buf), "%.0fx%.0f", wz.w, wz.h);
    DrawText(buf, (int)r.x, (int)(r.y + r.height + 2), 8, bc);
}

// =============================================================================
//  KILL ZONE  (fixed: Rectangle uses .width/.height, not .w/.h)
// =============================================================================

Rectangle LevelEditor::KillZoneRect(const KillZoneData& kz) const {
    // AABB of the rotated rectangle (used for selection picking)
    float cx = kz.x + kz.w * 0.5f, cy2 = kz.y + kz.h * 0.5f;
    float hw = kz.w * 0.5f, hh = kz.h * 0.5f;
    float rad = kz.rotation * DEG2RAD;
    float ca = fabsf(cosf(rad)), sa = fabsf(sinf(rad));
    float ew = hw * ca + hh * sa, eh = hw * sa + hh * ca;
    return { cx - ew, cy2 - eh, ew * 2.f, eh * 2.f };
}

void LevelEditor::DrawKillZoneEnt(const KillZoneData& kz, bool sel, bool msel) const {
    float cx = kz.x + kz.w * 0.5f, cy2 = kz.y + kz.h * 0.5f;
    Vector2 origin = { kz.w * 0.5f, kz.h * 0.5f };  // pivot = centre

    Color fill = sel ? Color{ 255,60,60,55 } : msel ? Color{ 255,200,0,40 } : Color{ 255,30,30,28 };
    Color bc = sel ? YELLOW : msel ? Color{ 255,200,0,240 } : Color{ 255,60,60,200 };
    float lw = sel ? 2.5f : 1.5f;

    // Filled rotated rectangle
    DrawRectanglePro({ cx, cy2, kz.w, kz.h }, origin, kz.rotation, fill);

    // Texture drawn rotated around the same centre
    Texture2D* tex = nullptr;
    if (kz.texId == KillZoneTexture::DK_GOLDEN_PISTON) tex = _goldenPistonTex;
    if (tex && tex->id > 0) {
        Rectangle dst = { cx, cy2, kz.w, kz.h };
        DrawTexturePro(*tex,
            { 0.f, 0.f, (float)tex->width, (float)tex->height },
            dst, origin, kz.rotation,
            (msel && !sel) ? Color{ 255,180,80,200 } : WHITE);
    }

    // Compute rotated corners for outline
    float rad = kz.rotation * DEG2RAD;
    float ca = cosf(rad), sa = sinf(rad);
    float hw = kz.w * 0.5f, hh = kz.h * 0.5f;
    auto rot = [&](float lx, float ly) -> Vector2 {
        return { cx + lx * ca - ly * sa, cy2 + lx * sa + ly * ca };
        };
    Vector2 TL = rot(-hw, -hh), TR = rot(hw, -hh);
    Vector2 BR = rot(hw, hh), BL = rot(-hw, hh);
    DrawLineEx(TL, TR, lw, bc); DrawLineEx(TR, BR, lw, bc);
    DrawLineEx(BR, BL, lw, bc); DrawLineEx(BL, TL, lw, bc);
    DrawLineEx(TL, BR, 1.5f, { 255,60,60,80 });
    DrawLineEx(TR, BL, 1.5f, { 255,60,60,80 });

    DrawText("KILL", (int)(TL.x + 2), (int)(TL.y + 2), 9, bc);
    if (fabsf(kz.rotation) > 0.5f) {
        char b[32]; snprintf(b, sizeof(b), "%.0f\xc2\xb0", kz.rotation);
        DrawText(b, (int)(cx - MeasureText(b, 9) / 2), (int)(cy2 - 5), 9, bc);
    }
    if (kz.texId == KillZoneTexture::DK_GOLDEN_PISTON)
        DrawText("GldPiston", (int)(BL.x), (int)(BL.y + 2), 8, { 255,200,80,200 });
    else
        DrawText("no tex", (int)(BL.x), (int)(BL.y + 2), 8, { 150,80,80,200 });
    char buf[32]; snprintf(buf, sizeof(buf), "%.0fx%.0f", kz.w, kz.h);
    DrawText(buf, (int)(BR.x - MeasureText(buf, 8)), (int)(BR.y + 2), 8, bc);
}


// =============================================================================
//  PROPERTIES CLIPBOARD  (UE-style copy/paste per entity type)
// =============================================================================
void LevelEditor::CopyProps() {
    if (!_sel.valid()) return;
    _propClip.type = _sel.type;
    if (_sel.type == (int)EditorTool::PLATFORM && _sel.index < (int)_level.platforms.size())
        _propClip.plat = _level.platforms[_sel.index];
    else if (_sel.type == (int)EditorTool::KILL_ZONE && _sel.index < (int)_level.killZones.size())
        _propClip.kz = _level.killZones[_sel.index];
    else if (_sel.type == (int)EditorTool::LADDER && _sel.index < (int)_level.ladders.size())
        _propClip.lad = _level.ladders[_sel.index];
    else if (_sel.type == (int)EditorTool::WIN_ZONE && _level.hasWinZone)
        _propClip.wz = _level.winZone;
    else if (_sel.type == (int)EditorTool::ELEVATOR && _sel.index < (int)_level.elevators.size())
        _propClip.elev = _level.elevators[_sel.index];
    else if ((_sel.type == (int)EditorTool::POINT_LIGHT ||
        _sel.type == (int)EditorTool::SPOT_LIGHT ||
        _sel.type == (int)EditorTool::SKY_LIGHT) &&
        _sel.index < (int)_level.lights.size())
        _propClip.light = _level.lights[_sel.index];
    else if (_sel.type == (int)EditorTool::PROP && _sel.index < (int)_level.props.size())
        _propClip.prop = _level.props[_sel.index];
    SetStatus("Properties copied.");
}
void LevelEditor::PasteProps() {
    if (!_sel.valid()) return;
    // Lights: allow paste between any of the three light tools (they all share _level.lights)
    bool selIsLight = (_sel.type == (int)EditorTool::POINT_LIGHT ||
        _sel.type == (int)EditorTool::SPOT_LIGHT ||
        _sel.type == (int)EditorTool::SKY_LIGHT);
    bool clipIsLight = (_propClip.type == (int)EditorTool::POINT_LIGHT ||
        _propClip.type == (int)EditorTool::SPOT_LIGHT ||
        _propClip.type == (int)EditorTool::SKY_LIGHT);
    if (selIsLight && clipIsLight && _sel.index < (int)_level.lights.size()) {
        float ox = _level.lights[_sel.index].x, oy = _level.lights[_sel.index].y;
        _level.lights[_sel.index] = _propClip.light;
        _level.lights[_sel.index].x = ox; _level.lights[_sel.index].y = oy;
        // FIX: keep _sel.type in sync with the new LightType so the properties
        // panel type-selector buttons and the outliner show the correct icon.
        _sel.type = (_level.lights[_sel.index].type == LightType::POINT) ? (int)EditorTool::POINT_LIGHT
            : (_level.lights[_sel.index].type == LightType::SPOT) ? (int)EditorTool::SPOT_LIGHT
            : (int)EditorTool::SKY_LIGHT;
        SetStatus("Light properties pasted.");
        return;
    }

    if (_propClip.type != _sel.type) return;
    if (_sel.type == (int)EditorTool::PLATFORM && _sel.index < (int)_level.platforms.size()) {
        // Preserve position, copy everything else
        float ox = _level.platforms[_sel.index].x, oy = _level.platforms[_sel.index].y;
        _level.platforms[_sel.index] = _propClip.plat;
        _level.platforms[_sel.index].x = ox; _level.platforms[_sel.index].y = oy;
    }
    else if (_sel.type == (int)EditorTool::KILL_ZONE && _sel.index < (int)_level.killZones.size()) {
        float ox = _level.killZones[_sel.index].x, oy = _level.killZones[_sel.index].y;
        _level.killZones[_sel.index] = _propClip.kz;
        _level.killZones[_sel.index].x = ox; _level.killZones[_sel.index].y = oy;
    }
    else if (_sel.type == (int)EditorTool::LADDER && _sel.index < (int)_level.ladders.size()) {
        float ox = _level.ladders[_sel.index].x, oy = _level.ladders[_sel.index].y;
        _level.ladders[_sel.index] = _propClip.lad;
        _level.ladders[_sel.index].x = ox; _level.ladders[_sel.index].y = oy;
    }
    else if (_sel.type == (int)EditorTool::WIN_ZONE && _level.hasWinZone) {
        float ox = _level.winZone.x, oy = _level.winZone.y;
        _level.winZone = _propClip.wz;
        _level.winZone.x = ox; _level.winZone.y = oy;
    }
    else if (_sel.type == (int)EditorTool::ELEVATOR && _sel.index < (int)_level.elevators.size()) {
        float ox = _level.elevators[_sel.index].x, oy = _level.elevators[_sel.index].y;
        _level.elevators[_sel.index] = _propClip.elev;
        _level.elevators[_sel.index].x = ox; _level.elevators[_sel.index].y = oy;
    }
    else if (_sel.type == (int)EditorTool::PROP && _sel.index < (int)_level.props.size()) {
        float ox = _level.props[_sel.index].x, oy = _level.props[_sel.index].y;
        _level.props[_sel.index] = _propClip.prop;
        _level.props[_sel.index].x = ox; _level.props[_sel.index].y = oy;
    }
    SetStatus("Properties pasted.");
}

// =============================================================================
//  PARENT-CHILD SYSTEM
// =============================================================================

SelectedEnt LevelEditor::GetParent(SelectedEnt e) const {
    for (const auto& rel : _level.relations) if (rel.child == e) return rel.parent; return {};
}
std::vector<SelectedEnt> LevelEditor::GetChildren(SelectedEnt e) const {
    std::vector<SelectedEnt> out;
    for (const auto& rel : _level.relations) if (rel.parent == e) out.push_back(rel.child);
    return out;
}
bool LevelEditor::IsAncestor(SelectedEnt anc, SelectedEnt e) const {
    SelectedEnt cur = GetParent(e); int limit = 32;
    while (cur.valid() && limit-- > 0) { if (cur == anc) return true; cur = GetParent(cur); }
    return false;
}
void LevelEditor::SetRelationParent(SelectedEnt child, SelectedEnt parent) {
    // Allow singleton entities (Regulus, player spawn, cave, win zone) which use index=-1
    auto entUsable = [](const SelectedEnt& e) { return e.type >= 0; };
    if (!entUsable(child) || !entUsable(parent) || child == parent) return;
    if (IsAncestor(child, parent)) { SetStatus("Cannot parent: would create a cycle."); return; }
    PushUndo();
    _level.relations.erase(std::remove_if(_level.relations.begin(), _level.relations.end(), [&child](const ParentChildRelation& r) { return r.child == child; }), _level.relations.end());
    ParentChildRelation rel; rel.parent = parent; rel.child = child;
    Vector2 cp = GetEntPos(child), pp = GetEntPos(parent);
    rel.offsetX = cp.x - pp.x;
    if (parent.type == (int)EditorTool::ELEVATOR && parent.index < (int)_level.elevators.size()) rel.offsetY = cp.y - _level.elevators[parent.index].y;
    else rel.offsetY = cp.y - pp.y;
    _level.relations.push_back(rel);
    SetStatus(TextFormat("Parented %s %d → %s %d", ToolName((EditorTool)child.type), child.index, ToolName((EditorTool)parent.type), parent.index));
}
void LevelEditor::RemoveRelation(SelectedEnt child) {
    PushUndo();
    _level.relations.erase(std::remove_if(_level.relations.begin(), _level.relations.end(), [&child](const ParentChildRelation& r) { return r.child == child; }), _level.relations.end());
    SetStatus("Detached from parent.");
}
void LevelEditor::DeleteRelationsFor(SelectedEnt e) {
    _level.relations.erase(std::remove_if(_level.relations.begin(), _level.relations.end(), [&e](const ParentChildRelation& r) { return r.parent == e || r.child == e; }), _level.relations.end());
}

void LevelEditor::BuildOutlineTree(SelectedEnt e, int depth) {
    EditorTool t = (EditorTool)e.type;
    const char* icons[] = { "[S]","[P]","[R]","[C]","[=]","[|]","[-]","[o]","[N]","[B]","[E]","[^]","[W]","[X]","[~]","[*]","[+]","[#]","[Pr]" };
    const char* icon = (e.type >= 0 && e.type < 19) ? icons[e.type] : "[?]";
    OutlineRow row;
    row.ent = e; row.icon = icon; row.color = ToolColor(t); row.depth = depth;
    if (e.index >= 0) snprintf(row.name, sizeof(row.name), "%s %d", ToolName(t), e.index);
    else              snprintf(row.name, sizeof(row.name), "%s", ToolName(t));
    auto children = GetChildren(e); row.hasChildren = !children.empty();
    _outline.push_back(row);
    for (const auto& ch : children) BuildOutlineTree(ch, depth + 1);
}

// =============================================================================
//  AUDIO TRACK HELPERS
// =============================================================================

Rectangle LevelEditor::SeqAudioRowRect() const {
    Rectangle tl = SeqTimelineRect();
    int trackCount = (_activeSeq >= 0 && _activeSeq < (int)_seqList.size()) ? (int)_seqList[_activeSeq].tracks.size() : 0;
    float y = tl.y + SEQ_RH + trackCount * SEQ_TH;
    return { (float)SEQ_BW, y, (float)(_canvasW - SEQ_BW), SEQ_AUDIO_H };
}
void LevelEditor::SeqEnsureAudio(int idx) { while ((int)_seqAudio.size() <= idx) _seqAudio.push_back(SeqAudio{}); }
void LevelEditor::SeqUnloadAudio(int idx) {
    if (idx < 0 || idx >= (int)_seqAudio.size()) return;
    SeqAudio& sa = _seqAudio[idx];
    if (sa.loaded) { StopMusicStream(sa.music); UnloadMusicStream(sa.music); sa.loaded = false; sa.path[0] = '\0'; }
}
void LevelEditor::SeqLoadAudioFile(int idx, const char* path) {
    SeqEnsureAudio(idx); SeqUnloadAudio(idx);
    SeqAudio& sa = _seqAudio[idx];
    snprintf(sa.path, sizeof(sa.path), "%s", path);
    sa.music = LoadMusicStream(path);
    sa.loaded = (sa.music.stream.buffer != nullptr);
    SetStatus(sa.loaded ? TextFormat("Audio loaded: %s", path) : TextFormat("Failed to load audio: %s", path));
}
void LevelEditor::DrawSeqAudioBar() {
    if (_activeSeq < 0 || _activeSeq >= (int)_seqList.size()) return;
    Rectangle ar = SeqAudioRowRect();
    DrawRectangleRec(ar, { 18,20,30,255 }); DrawRectangleLinesEx(ar, 1, { 55,60,85,255 });
    SeqEnsureAudio(_activeSeq);
    const SeqAudio& sa = _seqAudio[_activeSeq];
    Rectangle hdr = { ar.x, ar.y, (float)SEQ_HDR, ar.height };
    DrawRectangleRec(hdr, { 22,24,36,255 });
    DrawRectangle((int)hdr.x, (int)hdr.y, 3, (int)hdr.height, { 100,150,255,255 });
    DrawText("AUDIO", (int)(hdr.x + 6), (int)(hdr.y + 6), 9, { 140,160,220,255 });
    Rectangle dropZone = { ar.x + SEQ_HDR, ar.y, ar.width - SEQ_HDR, ar.height };
    bool hov = CheckCollisionPointRec(GetMousePosition(), dropZone);
    DrawRectangleRec(dropZone, hov ? Color{ 28,34,52,255 } : Color{ 20,22,34,255 });
    if (sa.loaded && sa.path[0] != '\0') {
        DrawText(GetFileName(sa.path), (int)(dropZone.x + 6), (int)(dropZone.y + 6), 9, { 160,200,255,255 });
        if (_seqPlaying && sa.loaded) {
            float dur = GetMusicTimeLength(sa.music), pos = GetMusicTimePlayed(sa.music);
            if (dur > 0.f) DrawRectangle((int)dropZone.x, (int)(dropZone.y + dropZone.height - 3), (int)(dropZone.width * (pos / dur)), 3, { 80,160,255,200 });
        }
    }
    else DrawText("Drag audio file here or click to browse", (int)(dropZone.x + 6), (int)(dropZone.y + 6), 9, { 80,90,120,255 });
}
// =============================================================================
//  CONVEYOR ENTITY
// =============================================================================

Rectangle LevelEditor::ConveyorRect(const ConveyorData& cv) const {
    return { cv.x, cv.y, cv.length, cv.beltH };
}

void LevelEditor::DrawConveyorEnt(const ConveyorData& cv, bool sel, bool msel) const {
    Color bc = sel ? YELLOW : (msel ? Color{ 255,220,60,240 } : Color{ 255,200,60,220 });

    // 3-frame animation: right-moving plays frames 0→1→2, left-moving plays 2→1→0
    int rawFrame = (int)(GetTime() * 9.f) % 3;   // 0,1,2 cycling at 9fps (3fps per frame)
    int frameL = (cv.direction == 1) ? rawFrame : (2 - rawFrame);  // left cap
    int frameR = 2 - frameL;                                       // right cap (inverted)
    int frame = frameL;  // used for middle section

    float rad = cv.rotation * DEG2RAD;
    float ca = cosf(rad), sa = sinf(rad);
    float ecW = cv.endCapW, bH = cv.beltH;
    float midW = fmaxf(0.f, cv.length - 2.f * ecW);

    auto DrawSec = [&](Texture2D* tex, float lx, float w, bool flipH) {
        float wx = cv.x + lx * ca, wy = cv.y + lx * sa;
        if (tex && tex->id > 0) {
            float srcX = flipH ? (float)tex->width : 0.f;
            float srcW = flipH ? -(float)tex->width : (float)tex->width;
            DrawTexturePro(*tex, { srcX, 0, srcW, (float)tex->height },
                { wx, wy, w, bH }, {}, cv.rotation, WHITE);
        }
        else {
            DrawRectanglePro({ wx, wy, w, bH }, {}, cv.rotation, Color{ 80,70,40,180 });
        }
        };

    DrawSec(_convSide[frameL], 0.f, ecW, false);  // left cap
    if (midW > 0.f) {
        if (_convM[frame] && _convM[frame]->id > 0) {
            // Tile the middle at ecW world-pixels per tile.
            // Partial last tiles are source-cropped, not stretched.
            float tileDisp = ecW;
            for (float lx = ecW; lx < ecW + midW; lx += tileDisp) {
                float drawW = fminf(tileDisp, ecW + midW - lx);
                float srcCropW = (float)_convM[frame]->width * (drawW / tileDisp);
                DrawTexturePro(*_convM[frame],
                    { 0, 0, srcCropW, (float)_convM[frame]->height },
                    { cv.x + lx * ca, cv.y + lx * sa, drawW, bH },
                    {}, cv.rotation, WHITE);
            }
        }
        else {
            float wx = cv.x + ecW * ca, wy = cv.y + ecW * sa;
            DrawRectanglePro({ wx, wy, midW, bH }, {}, cv.rotation, Color{ 80,70,40,180 });
        }
    }
    DrawSec(_convSide[frameR], cv.length - ecW, ecW, true); // right cap — inverted frame + flipped

    // Overlay: selection tint + direction arrow
    DrawRectanglePro({ cv.x, cv.y, cv.length, bH }, {}, cv.rotation,
        sel ? Color{ 255,220,60,30 } : Color{ 255,200,60,10 });
    float arrowLx = cv.length * 0.5f;
    float ax = cv.x + arrowLx * ca, ay = cv.y + arrowLx * sa;
    float dx = cv.direction * 10.f * ca, dy = cv.direction * 10.f * sa;
    DrawLineEx({ ax - dx, ay - dy }, { ax + dx, ay + dy }, 2.f, bc);
    DrawText(TextFormat("%.0fpx/s %s", cv.speed, cv.direction == 1 ? "▶" : "◀"),
        (int)cv.x + 2, (int)(cv.y + bH + 2), 9, bc);
}

// =============================================================================
//  PROP ENTITY
// =============================================================================

Rectangle LevelEditor::PropRect(const PropData& pr) const {
    return { pr.x - pr.width * 0.5f, pr.y - pr.height * 0.5f, pr.width, pr.height };
}

bool LevelEditor::PointInProp(Vector2 pt, const PropData& pr) const {
    float rad = -pr.rotation * DEG2RAD;
    float ca = cosf(rad), sa = sinf(rad);
    float dx = pt.x - pr.x, dy = pt.y - pr.y;
    float lx = dx * ca - dy * sa;
    float ly = dx * sa + dy * ca;
    return fabsf(lx) <= pr.width * 0.5f && fabsf(ly) <= pr.height * 0.5f;
}

void LevelEditor::DrawPropEnt(const PropData& pr, bool sel, bool msel) const {
    Color outline = sel ? YELLOW : (msel ? SKYBLUE : Color{ 180,100,220,200 });

    // Draw texture or placeholder
    Texture2D* tex = (pr.texVariant < _propTexCount && _propTex[pr.texVariant])
        ? _propTex[pr.texVariant] : nullptr;
    if (tex && tex->id > 0) {
        DrawTexturePro(*tex,
            { 0, 0, (float)tex->width, (float)tex->height },
            { pr.x, pr.y, pr.width, pr.height },
            { pr.width * 0.5f, pr.height * 0.5f },
            pr.rotation, WHITE);
    }
    else {
        DrawRectanglePro({ pr.x - pr.width * 0.5f, pr.y - pr.height * 0.5f, pr.width, pr.height },
            {}, pr.rotation, Color{ 180,100,220,100 });
    }

    // Rotated outline
    float rad = pr.rotation * DEG2RAD;
    float ca = cosf(rad), sa = sinf(rad);
    float hw = pr.width * 0.5f, hh = pr.height * 0.5f;
    Vector2 corners[4] = {
        { pr.x + (-hw) * ca - (-hh) * sa, pr.y + (-hw) * sa + (-hh) * ca },
        { pr.x + (hw)*ca - (-hh) * sa, pr.y + (hw)*sa + (-hh) * ca },
        { pr.x + (hw)*ca - (hh)*sa, pr.y + (hw)*sa + (hh)*ca },
        { pr.x + (-hw) * ca - (hh)*sa, pr.y + (-hw) * sa + (hh)*ca },
    };
    for (int k = 0; k < 4; k++)
        DrawLineEx(corners[k], corners[(k + 1) % 4], sel ? 2.f : 1.f, outline);

    // Label
    DrawText(TextFormat("Pr aff=%.1f col=%s", pr.lightAffect, pr.hasCollision ? "Y" : "N"),
        (int)(corners[0].x), (int)(corners[0].y - 14), 9, outline);
}

void LevelEditor::SetPropTextures(Texture2D** ptrs, int count)
{
    _propTexCount = count;

    for (int i = 0; i < count && i < 16; i++)
        _propTex[i] = ptrs[i];
}