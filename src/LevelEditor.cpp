// ============================================================
//  LevelEditor.cpp
// ============================================================
#include "LevelEditor.h"
#include "raymath.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
//  Static metadata
// ─────────────────────────────────────────────────────────────────────────────

const char* LevelEditor::ToolName(EditorTool t)
{
    switch (t) {
    case EditorTool::SELECT:         return "SELECT";
    case EditorTool::PLAYER_SPAWN:   return "PLAYER";
    case EditorTool::REGULUS:        return "REGULUS";
    case EditorTool::CAVE:           return "CAVE";
    case EditorTool::PLATFORM:       return "PLATFORM";
    case EditorTool::LADDER:         return "LADDER";
    case EditorTool::BEAM:           return "BEAM";
    case EditorTool::PATH_NODE:      return "PATH NODE";
    case EditorTool::NUKE_SPAWN:     return "NUKE";
    case EditorTool::BEATRICE_SPAWN: return "BEATRICE";
    case EditorTool::ENEMY_SPAWN:    return "ENEMY";
    default:                         return "???";
    }
}
Color LevelEditor::ToolColor(EditorTool t)
{
    switch (t) {
    case EditorTool::SELECT:         return LIGHTGRAY;
    case EditorTool::PLAYER_SPAWN:   return GREEN;
    case EditorTool::REGULUS:        return { 160,32,240,255 };
    case EditorTool::CAVE:           return ORANGE;
    case EditorTool::PLATFORM:       return { 80,120,255,255 };
    case EditorTool::LADDER:         return YELLOW;
    case EditorTool::BEAM:           return DARKGRAY;
    case EditorTool::PATH_NODE:      return WHITE;
    case EditorTool::NUKE_SPAWN:     return SKYBLUE;
    case EditorTool::BEATRICE_SPAWN: return MAGENTA;
    case EditorTool::ENEMY_SPAWN:    return RED;
    default:                         return GRAY;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::Init(int screenW, int screenH)
{
    _sw = screenW; _sh = screenH;
    _canvasH = (float)(_sh - TOOLBAR_H - BROWSER_H);
    _zoom = _canvasH / (float)_sh;

    _cam.offset = { 0.f, (float)TOOLBAR_H };
    _cam.target = { 0.f, 0.f };
    _cam.rotation = 0.f;
    _cam.zoom = _zoom;

    LoadLevel(1);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Coordinate helpers
// ─────────────────────────────────────────────────────────────────────────────

Vector2 LevelEditor::WorldMouse() const
{
    return GetScreenToWorld2D(GetMousePosition(), _cam);
}

Vector2 LevelEditor::Snap(Vector2 v) const
{
    if (!_gridOn) return v;
    float gs = (float)GRID_SZ / _gridDiv;
    return { roundf(v.x / gs) * gs, roundf(v.y / gs) * gs };
}

bool LevelEditor::InCanvas()  const { Vector2 m = GetMousePosition(); return m.y >= TOOLBAR_H && m.y < (_sh - BROWSER_H); }
bool LevelEditor::InBrowser() const { return GetMousePosition().y >= (_sh - BROWSER_H); }
bool LevelEditor::InToolbar() const { return GetMousePosition().y < TOOLBAR_H; }

// ─────────────────────────────────────────────────────────────────────────────
//  Entity geometry
// ─────────────────────────────────────────────────────────────────────────────

Rectangle LevelEditor::PlatRect(const PlatformData& p) const
{
    float h = (p.h > 0.f) ? p.h : 12.f;
    // Axis-aligned bounding box (accounts for tilt height change)
    float yDrop = p.w * fabsf(tanf(p.tilt * DEG2RAD));
    return { p.x, p.y, p.w, h + yDrop };
}

Rectangle LevelEditor::LadRect(const LadderData& l) const
{
    return { l.x, l.y, l.w, l.h };
}

Rectangle LevelEditor::BeamRect(Vector2 pos) const
{
    if (_beamTex && _beamTex->id > 0) {
        const float sc = 4.f;
        return { pos.x, pos.y, _beamTex->width * sc, _beamTex->height * sc };
    }
    return { pos.x - 8.f, pos.y - 4.f, 16.f, 8.f };
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entity position accessors (generic, by SelectedEnt)
// ─────────────────────────────────────────────────────────────────────────────

Vector2 LevelEditor::GetEntPos(const SelectedEnt& e) const
{
    if (!e.valid()) return {};
    int i = e.index;
    switch ((EditorTool)e.type) {
    case EditorTool::PLAYER_SPAWN:   return _level.playerSpawn;
    case EditorTool::REGULUS:        return _level.regulusPos;
    case EditorTool::CAVE:           return _level.cavePos;
    case EditorTool::PLATFORM:       return { _level.platforms[i].x, _level.platforms[i].y };
    case EditorTool::LADDER:         return { _level.ladders[i].x,   _level.ladders[i].y };
    case EditorTool::BEAM:           return _level.beams[i];
    case EditorTool::PATH_NODE:      return { _level.pathNodes[i].x, _level.pathNodes[i].y };
    case EditorTool::NUKE_SPAWN:     return _level.nukeSpawns[i];
    case EditorTool::BEATRICE_SPAWN: return _level.beatriceSpawns[i];
    case EditorTool::ENEMY_SPAWN:    return _level.enemySpawns[i];
    default: return {};
    }
}

void LevelEditor::SetEntPos(const SelectedEnt& e, Vector2 p)
{
    if (!e.valid()) return;
    int i = e.index;
    switch ((EditorTool)e.type) {
    case EditorTool::PLAYER_SPAWN:   _level.playerSpawn = p; break;
    case EditorTool::REGULUS:        _level.regulusPos = p; break;
    case EditorTool::CAVE:           _level.cavePos = p; break;
    case EditorTool::PLATFORM:       _level.platforms[i].x = p.x; _level.platforms[i].y = p.y; break;
    case EditorTool::LADDER:         _level.ladders[i].x = p.x;   _level.ladders[i].y = p.y;   break;
    case EditorTool::BEAM:           _level.beams[i] = p; break;
    case EditorTool::PATH_NODE:      _level.pathNodes[i].x = p.x; _level.pathNodes[i].y = p.y; break;
    case EditorTool::NUKE_SPAWN:     _level.nukeSpawns[i] = p; break;
    case EditorTool::BEATRICE_SPAWN: _level.beatriceSpawns[i] = p; break;
    case EditorTool::ENEMY_SPAWN:    _level.enemySpawns[i] = p; break;
    default: break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Entity picking
// ─────────────────────────────────────────────────────────────────────────────

bool LevelEditor::PickEntity(Vector2 p)
{
    _sel.clear();
    const float R = 14.f;

    if (_level.hasPlayerSpawn && CheckCollisionPointCircle(p, _level.playerSpawn, R))
    {
        _sel = { (int)EditorTool::PLAYER_SPAWN,0 }; return true;
    }
    if (_level.hasRegulus && CheckCollisionPointCircle(p, _level.regulusPos, R))
    {
        _sel = { (int)EditorTool::REGULUS,0 }; return true;
    }
    if (_level.hasCave && CheckCollisionPointRec(p, { _level.cavePos.x,_level.cavePos.y,50,50 }))
    {
        _sel = { (int)EditorTool::CAVE,0 }; return true;
    }

    for (int i = 0; i < (int)_level.pathNodes.size(); i++) {
        Vector2 np = { _level.pathNodes[i].x,_level.pathNodes[i].y };
        if (CheckCollisionPointCircle(p, np, 10.f)) { _sel = { (int)EditorTool::PATH_NODE,i }; return true; }
    }
    for (int i = 0; i < (int)_level.platforms.size(); i++)
        if (CheckCollisionPointRec(p, PlatRect(_level.platforms[i])))
        {
            _sel = { (int)EditorTool::PLATFORM,i }; return true;
        }
    for (int i = 0; i < (int)_level.ladders.size(); i++)
        if (CheckCollisionPointRec(p, LadRect(_level.ladders[i])))
        {
            _sel = { (int)EditorTool::LADDER,i }; return true;
        }
    for (int i = 0; i < (int)_level.beams.size(); i++)
        if (CheckCollisionPointRec(p, BeamRect(_level.beams[i])))
        {
            _sel = { (int)EditorTool::BEAM,i }; return true;
        }
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.nukeSpawns[i], R))
        {
            _sel = { (int)EditorTool::NUKE_SPAWN,i }; return true;
        }
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.beatriceSpawns[i], R))
        {
            _sel = { (int)EditorTool::BEATRICE_SPAWN,i }; return true;
        }
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)
        if (CheckCollisionPointCircle(p, _level.enemySpawns[i], R))
        {
            _sel = { (int)EditorTool::ENEMY_SPAWN,i }; return true;
        }
    return false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Multi-selection helpers
// ─────────────────────────────────────────────────────────────────────────────

bool LevelEditor::IsInMultiSel(const SelectedEnt& e) const
{
    for (const auto& s : _multiSel) if (s == e) return true;
    return false;
}

void LevelEditor::BoxSelectEntities(Rectangle box)
{
    _multiSel.clear();
    // Normalise (user may have dragged left/up)
    if (box.width < 0) { box.x += box.width;  box.width = -box.width; }
    if (box.height < 0) { box.y += box.height; box.height = -box.height; }

    auto AddCirc = [&](EditorTool t, Vector2 pos, int idx) {
        if (CheckCollisionPointRec(pos, box)) _multiSel.push_back({ (int)t,idx });
        };
    for (int i = 0; i < (int)_level.platforms.size(); i++)
        if (CheckCollisionRecs(PlatRect(_level.platforms[i]), box))
            _multiSel.push_back({ (int)EditorTool::PLATFORM,i });
    for (int i = 0; i < (int)_level.ladders.size(); i++)
        if (CheckCollisionRecs(LadRect(_level.ladders[i]), box))
            _multiSel.push_back({ (int)EditorTool::LADDER,i });
    for (int i = 0; i < (int)_level.beams.size(); i++)
        if (CheckCollisionRecs(BeamRect(_level.beams[i]), box))
            _multiSel.push_back({ (int)EditorTool::BEAM,i });
    for (int i = 0; i < (int)_level.pathNodes.size(); i++)
        AddCirc(EditorTool::PATH_NODE, { _level.pathNodes[i].x,_level.pathNodes[i].y }, i);
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)
        AddCirc(EditorTool::NUKE_SPAWN, _level.nukeSpawns[i], i);
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++)
        AddCirc(EditorTool::BEATRICE_SPAWN, _level.beatriceSpawns[i], i);
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)
        AddCirc(EditorTool::ENEMY_SPAWN, _level.enemySpawns[i], i);
    if (_level.hasPlayerSpawn && CheckCollisionPointRec(_level.playerSpawn, box))
        _multiSel.push_back({ (int)EditorTool::PLAYER_SPAWN,0 });
    if (_level.hasRegulus && CheckCollisionPointRec(_level.regulusPos, box))
        _multiSel.push_back({ (int)EditorTool::REGULUS,0 });
    if (_level.hasCave && CheckCollisionPointRec(_level.cavePos, box))
        _multiSel.push_back({ (int)EditorTool::CAVE,0 });

    if (!_multiSel.empty()) _sel = _multiSel[0];
    SetStatus(TextFormat("%d entities selected.", (int)_multiSel.size()));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Delete
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::DeleteSelected()
{
    if (!_sel.valid()) return;
    PushUndo();
    auto erase = [](auto& vec, int i) { if (i >= 0 && i < (int)vec.size()) vec.erase(vec.begin() + i); };

    EditorTool t = (EditorTool)_sel.type;
    int i = _sel.index;
    switch (t) {
    case EditorTool::PLAYER_SPAWN:   _level.hasPlayerSpawn = false; break;
    case EditorTool::REGULUS:        _level.hasRegulus = false; break;
    case EditorTool::CAVE:           _level.hasCave = false; break;
    case EditorTool::PLATFORM:       erase(_level.platforms, i); break;
    case EditorTool::LADDER:         erase(_level.ladders, i); break;
    case EditorTool::BEAM:           erase(_level.beams, i); break;
    case EditorTool::PATH_NODE: {
        for (auto& n : _level.pathNodes) {
            if (n.next[0] == i) n.next[0] = -1; if (n.next[1] == i) n.next[1] = -1;
            if (n.next[0] > i) n.next[0]--;   if (n.next[1] > i) n.next[1]--;
        }
        erase(_level.pathNodes, i); break;
    }
    case EditorTool::NUKE_SPAWN:     erase(_level.nukeSpawns, i); break;
    case EditorTool::BEATRICE_SPAWN: erase(_level.beatriceSpawns, i); break;
    case EditorTool::ENEMY_SPAWN:    erase(_level.enemySpawns, i); break;
    default: break;
    }
    _sel.clear(); _multiSel.clear();
    _connectMode = ConnectMode::NONE;
    SetStatus("Entity deleted.");
}

void LevelEditor::DeleteMultiSelected()
{
    if (_multiSel.empty() && !_sel.valid()) return;
    PushUndo();
    auto erase = [](auto& vec, int i) { if (i >= 0 && i < (int)vec.size()) vec.erase(vec.begin() + i); };

    // Helper: collect indices for a given type, sort descending, erase
    auto DelType = [&](EditorTool t, auto& vec) {
        std::vector<int> idx;
        for (const auto& e : _multiSel) if (e.type == (int)t) idx.push_back(e.index);
        if (_sel.valid() && _sel.type == (int)t)
            if (std::find(idx.begin(), idx.end(), _sel.index) == idx.end()) idx.push_back(_sel.index);
        std::sort(idx.rbegin(), idx.rend());
        for (int i : idx) erase(vec, i);
        };
    DelType(EditorTool::PLATFORM, _level.platforms);
    DelType(EditorTool::LADDER, _level.ladders);
    DelType(EditorTool::BEAM, _level.beams);
    DelType(EditorTool::NUKE_SPAWN, _level.nukeSpawns);
    DelType(EditorTool::BEATRICE_SPAWN, _level.beatriceSpawns);
    DelType(EditorTool::ENEMY_SPAWN, _level.enemySpawns);

    // Path nodes require reference re-indexing — delete in reverse order
    std::vector<int> pathIdx;
    for (const auto& e : _multiSel) if (e.type == (int)EditorTool::PATH_NODE) pathIdx.push_back(e.index);
    if (_sel.valid() && _sel.type == (int)EditorTool::PATH_NODE)
        if (std::find(pathIdx.begin(), pathIdx.end(), _sel.index) == pathIdx.end()) pathIdx.push_back(_sel.index);
    std::sort(pathIdx.rbegin(), pathIdx.rend());
    for (int i : pathIdx) {
        for (auto& n : _level.pathNodes) {
            if (n.next[0] == i) n.next[0] = -1; if (n.next[1] == i) n.next[1] = -1;
            if (n.next[0] > i) n.next[0]--;   if (n.next[1] > i) n.next[1]--;
        }
        erase(_level.pathNodes, i);
    }

    // Singletons
    for (const auto& e : _multiSel) {
        if (e.type == (int)EditorTool::PLAYER_SPAWN) _level.hasPlayerSpawn = false;
        if (e.type == (int)EditorTool::REGULUS)      _level.hasRegulus = false;
        if (e.type == (int)EditorTool::CAVE)         _level.hasCave = false;
    }

    _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE;
    SetStatus("Deleted selected entities.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load / Save
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::LoadLevel(int id)
{
    _levelId = id; _level = LevelData{};
    if (!::LoadLevel(_level, id)) {
        if (id == 1) { _level = GetDefaultLevel1(); SetStatus("Loaded built-in level 1."); }
        else { _level.id = id; _level.valid = true; SetStatus("New empty level."); }
    }
    else SetStatus(TextFormat("Loaded level %d from disk.", id));
    _sel.clear(); _multiSel.clear();
    _placingPlatform = false; _placingLadder = false;
    _dragging = false; _multiDragging = false; _boxSelecting = false;
    _connectMode = ConnectMode::NONE;
    _undoStack.clear(); _redoStack.clear();
}

void LevelEditor::SaveCurrentLevel()
{
    _level.id = _levelId;
    if (SaveLevel(_level)) SetStatus(TextFormat("Level %d saved.", _levelId));
    else                   SetStatus("ERROR: Could not save level!");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Undo / Redo
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::PushUndo()
{
    _undoStack.push_back(_level);
    if ((int)_undoStack.size() > MAX_UNDO) _undoStack.erase(_undoStack.begin());
    _redoStack.clear();
}

void LevelEditor::Undo()
{
    if (_undoStack.empty()) { SetStatus("Nothing to undo."); return; }
    _redoStack.push_back(_level);
    _level = _undoStack.back(); _undoStack.pop_back();
    _sel.clear(); _multiSel.clear();
    SetStatus("Undo.");
}

void LevelEditor::Redo()
{
    if (_redoStack.empty()) { SetStatus("Nothing to redo."); return; }
    _undoStack.push_back(_level);
    _level = _redoStack.back(); _redoStack.pop_back();
    _sel.clear(); _multiSel.clear();
    SetStatus("Redo.");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Status bar
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::SetStatus(const char* msg, float dur)
{
    strncpy(_status, msg, sizeof(_status) - 1); _statusTimer = dur;
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateToolbar
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::UpdateToolbar()
{
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

    // Keyboard shortcuts
    if (ctrl && IsKeyPressed(KEY_S))  SaveCurrentLevel();
    if (ctrl && IsKeyPressed(KEY_Z))  Undo();
    if (ctrl && IsKeyPressed(KEY_Y))  Redo();
    if (IsKeyPressed(KEY_G)) { _gridOn = !_gridOn; SetStatus(_gridOn ? "Grid ON" : "Grid OFF"); }
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE))
        _multiSel.empty() ? DeleteSelected() : DeleteMultiSelected();
    if (IsKeyPressed(KEY_ESCAPE))
    {
        _connectMode = ConnectMode::NONE; _tool = EditorTool::SELECT;
        _sel.clear(); _multiSel.clear(); _boxSelecting = false; SetStatus("Cancelled.");
    }
    // B = back to menu (always)
    if (IsKeyPressed(KEY_B)) { _wantsMenu = true; return; }
    // [ / ] cycle grid division
    if (IsKeyPressed(KEY_LEFT_BRACKET))
    {
        _gridDiv = (_gridDiv == 1) ? 4 : (_gridDiv == 4 ? 2 : 1); SetStatus(TextFormat("Grid ÷%d", _gridDiv));
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET))
    {
        _gridDiv = (_gridDiv == 1) ? 2 : (_gridDiv == 2 ? 4 : 1); SetStatus(TextFormat("Grid ÷%d", _gridDiv));
    }

    // Level nav keyboard
    if (!ctrl) {
        if (IsKeyPressed(KEY_LEFT) && _levelId > 1) { SaveCurrentLevel(); LoadLevel(_levelId - 1); }
        if (IsKeyPressed(KEY_RIGHT) && _levelId < 10) { SaveCurrentLevel(); LoadLevel(_levelId + 1); }
    }

    // Toolbar button clicks  —  8 buttons
    Vector2 mouse = GetMousePosition();
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || !InToolbar()) return;
    auto TBtn = [&](int col)->Rectangle { float w = (float)_sw / 8.f; return { col * w,0,w - 2,(float)TOOLBAR_H - 2 }; };

    if (CheckCollisionPointRec(mouse, TBtn(0))) { if (_levelId > 1) { SaveCurrentLevel(); LoadLevel(--_levelId); } }
    if (CheckCollisionPointRec(mouse, TBtn(2))) { if (_levelId < 10) { SaveCurrentLevel(); LoadLevel(++_levelId); } }
    if (CheckCollisionPointRec(mouse, TBtn(3))) { _gridOn = !_gridOn; SetStatus(_gridOn ? "Grid ON" : "Grid OFF"); }
    if (CheckCollisionPointRec(mouse, TBtn(4))) { _gridDiv = (_gridDiv == 1) ? 2 : (_gridDiv == 2) ? 4 : 1; SetStatus(TextFormat("Grid ÷%d", _gridDiv)); }
    if (CheckCollisionPointRec(mouse, TBtn(5))) SaveCurrentLevel();
    if (CheckCollisionPointRec(mouse, TBtn(6))) { SaveCurrentLevel(); _wantsPlay = true; }
    if (CheckCollisionPointRec(mouse, TBtn(7))) { _wantsMenu = true; }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateBrowser
// ─────────────────────────────────────────────────────────────────────────────

Rectangle LevelEditor::BrowserBtn(int row, int col, int cols) const
{
    float bw = (float)_sw / cols, bh = 38.f;
    float by = (float)(_sh - BROWSER_H) + 6 + row * (bh + 4);
    return { col * bw + 2,by,bw - 4,bh };
}

void LevelEditor::UpdateBrowser()
{
    if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return;
    Vector2 mouse = GetMousePosition();
    if (!InBrowser()) return;

    const int row0tools[] = { 0,1,2,3,4,5 };
    const int row1tools[] = { 6,7,8,9,10 };
    for (int c = 0; c < 6; c++)
        if (CheckCollisionPointRec(mouse, BrowserBtn(0, c, 6)))
        {
            _tool = (EditorTool)row0tools[c]; _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE; SetStatus(TextFormat("Tool: %s", ToolName(_tool))); return;
        }
    for (int c = 0; c < 5; c++)
        if (CheckCollisionPointRec(mouse, BrowserBtn(1, c, 5)))
        {
            _tool = (EditorTool)row1tools[c]; _sel.clear(); _multiSel.clear(); _connectMode = ConnectMode::NONE; SetStatus(TextFormat("Tool: %s", ToolName(_tool))); return;
        }
}

// ─────────────────────────────────────────────────────────────────────────────
//  UpdateCanvas
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::UpdateCanvas()
{
    Vector2 wm = WorldMouse(), swm = Snap(wm);
    bool lmbP = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    bool lmbD = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    bool lmbR = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    bool rmbP = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);

    // Right-click = delete under cursor
    if (rmbP) { PickEntity(wm); if (_sel.valid()) { DeleteSelected(); return; } }

    // Path-node connection mode
    if (_connectMode != ConnectMode::NONE) {
        if (lmbP) {
            for (int i = 0; i < (int)_level.pathNodes.size(); i++) {
                Vector2 np = { _level.pathNodes[i].x,_level.pathNodes[i].y };
                if (CheckCollisionPointCircle(wm, np, 12.f)) {
                    PushUndo();
                    auto& src = _level.pathNodes[_connectFrom];
                    if (_connectMode == ConnectMode::NEXT0) src.next[0] = i;
                    else                                  src.next[1] = i;
                    _connectMode = ConnectMode::NONE;
                    SetStatus(TextFormat("Node %d connected.", i)); return;
                }
            }
        }
        return;
    }

    // ── SELECT tool ──────────────────────────────────────────────────────────
    if (_tool == EditorTool::SELECT)
    {
        if (lmbP) {
            bool hitEntity = PickEntity(wm);
            if (hitEntity) {
                // Check if clicked entity is already in multi-selection → multi-drag
                if (_multiSel.size() > 1 && IsInMultiSel(_sel)) {
                    PushUndo();
                    _multiDragging = true;
                    _multiDragAnchor = wm;
                    _multiDragOrigins.clear();
                    for (const auto& e : _multiSel) _multiDragOrigins.push_back(GetEntPos(e));
                }
                else {
                    // Single select + drag
                    _multiSel.clear(); _boxSelecting = false;
                    PushUndo();
                    _dragging = true;
                    _dragOffset = Vector2Subtract(wm, GetSelPos());
                    SetStatus(TextFormat("Selected %s #%d — drag to move, DEL to delete", ToolName((EditorTool)_sel.type), _sel.index));
                }
            }
            else {
                // Start box selection
                _sel.clear(); _multiSel.clear();
                _dragging = false; _multiDragging = false;
                _boxSelecting = true;
                _boxStart = _boxEnd = wm;
            }
        }
        if (lmbD) {
            if (_dragging) SetSelPos(Snap(Vector2Subtract(wm, _dragOffset)));
            if (_multiDragging) {
                Vector2 delta = Vector2Subtract(wm, _multiDragAnchor);
                for (int i = 0; i < (int)_multiSel.size(); i++)
                    SetEntPos(_multiSel[i], Snap(Vector2Add(_multiDragOrigins[i], delta)));
            }
            if (_boxSelecting) _boxEnd = wm;
        }
        if (lmbR) {
            if (_boxSelecting) {
                Rectangle box = {
                    fminf(_boxStart.x,_boxEnd.x), fminf(_boxStart.y,_boxEnd.y),
                    fabsf(_boxEnd.x - _boxStart.x),  fabsf(_boxEnd.y - _boxStart.y)
                };
                if (box.width > 4.f || box.height > 4.f) BoxSelectEntities(box);
                else { _sel.clear(); _multiSel.clear(); }
                _boxSelecting = false;
            }
            _dragging = false; _multiDragging = false;
        }
        return;
    }

    // ── PLATFORM (drag to define width) ─────────────────────────────────────
    if (_tool == EditorTool::PLATFORM) {
        if (lmbP) { _placingPlatform = true; _platStart = swm; }
        if (_placingPlatform && lmbR) {
            float w = swm.x - _platStart.x;
            if (fabsf(w) < GRID_SZ) w = (float)(GRID_SZ * 4);
            PushUndo();
            PlatformData p;
            if (w >= 0) { p.x = _platStart.x; p.w = w; }
            else { p.x = swm.x;        p.w = -w; }
            p.y = _platStart.y; p.h = 0.f; p.tilt = 0.f;
            _level.platforms.push_back(p);
            _placingPlatform = false;
            SetStatus("Platform placed.");
        }
        return;
    }

    // ── LADDER (drag to define height) ───────────────────────────────────────
    if (_tool == EditorTool::LADDER) {
        if (lmbP) { _placingLadder = true; _ladStart = swm; }
        if (_placingLadder && lmbR) {
            float h = swm.y - _ladStart.y;
            if (fabsf(h) < GRID_SZ) h = (float)(GRID_SZ * 4);
            PushUndo();
            LadderData l;
            l.x = _ladStart.x; l.y = (h >= 0) ? _ladStart.y : swm.y;
            l.w = 40.f;        l.h = fabsf(h);
            _level.ladders.push_back(l);
            _placingLadder = false;
            SetStatus("Ladder placed.");
        }
        return;
    }

    // ── PATH_NODE ────────────────────────────────────────────────────────────
    if (_tool == EditorTool::PATH_NODE) {
        if (lmbP) {
            PushUndo();
            PathNodeData n; n.x = swm.x; n.y = swm.y;
            _level.pathNodes.push_back(n);
            _sel = { (int)EditorTool::PATH_NODE,(int)_level.pathNodes.size() - 1 };
            SetStatus(TextFormat("Path node %d placed.", _sel.index));
        }
        return;
    }

    // ── Single-click placement ───────────────────────────────────────────────
    if (lmbP) {
        PushUndo();
        switch (_tool) {
        case EditorTool::PLAYER_SPAWN:
            _level.hasPlayerSpawn = true; _level.playerSpawn = swm; SetStatus("Player spawn set."); break;
        case EditorTool::REGULUS:
            _level.hasRegulus = true; _level.regulusPos = swm; SetStatus("Regulus set."); break;
        case EditorTool::CAVE:
            _level.hasCave = true; _level.cavePos = swm; SetStatus("Cave set."); break;
        case EditorTool::BEAM:
            _level.beams.push_back(swm); SetStatus("Beam placed."); break;
        case EditorTool::NUKE_SPAWN:
            _level.nukeSpawns.push_back(swm); SetStatus("Nuke spawn placed."); break;
        case EditorTool::BEATRICE_SPAWN:
            _level.beatriceSpawns.push_back(swm); SetStatus("Beatrice spawn placed."); break;
        case EditorTool::ENEMY_SPAWN:
            _level.enemySpawns.push_back(swm); SetStatus("Enemy spawn placed."); break;
        default: break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Master Update
// ─────────────────────────────────────────────────────────────────────────────

void LevelEditor::Update(float dt)
{
    if (_statusTimer > 0.f) _statusTimer -= dt;
    UpdateToolbar();
    if (_wantsMenu || _wantsPlay) return;
    UpdateBrowser();
    if (InCanvas()) UpdateCanvas();
}

// =============================================================================
//  DRAW
// =============================================================================

// ── Entity draw helpers ────────────────────────────────────────────────────────

void LevelEditor::DrawPlatEnt(const PlatformData& p, bool sel, bool msel) const
{
    const float platH = (p.h > 0.f) ? p.h : 12.f;
    const float tiltRad = p.tilt * DEG2RAD;
    const float yRight = p.w * tanf(tiltRad);       // y-drop at right edge

    // Filled parallelogram (no beam texture on platforms — beams are separate)
    Color fill = msel ? Color{ 255,200,0,40 } : (sel ? Color{ 255,220,0,30 } : Color{ 0,140,255,35 });
    // Draw as two triangles to form the tilted quad
    Vector2 TL = { p.x,       p.y };
    Vector2 TR = { p.x + p.w,   p.y + yRight };
    Vector2 BL = { p.x,       p.y + platH };
    Vector2 BR = { p.x + p.w,   p.y + yRight + platH };
    DrawTriangle(TL, BL, TR, fill);
    DrawTriangle(BL, BR, TR, fill);

    // Outline
    Color bc = sel ? YELLOW : (msel ? Color{ 255,200,0,240 } : Color{ 0,160,255,220 });
    float lw = sel ? 2.5f : 1.5f;
    DrawLineEx(TL, TR, lw, bc);
    DrawLineEx(BL, BR, lw, bc);
    DrawLineEx(TL, BL, lw, bc);
    DrawLineEx(TR, BR, lw, bc);

    // Tilt label
    if (fabsf(p.tilt) > 0.1f) {
        char buf[32]; snprintf(buf, sizeof(buf), "%.1f°", p.tilt);
        DrawText(buf, (int)p.x + 2, (int)p.y - 12, 9, bc);
    }

    // Corner handles when selected
    if (sel || msel) {
        const float hs = 5.f;
        auto H = [hs](Vector2 c) { DrawRectangle((int)(c.x - hs), (int)(c.y - hs), (int)(hs * 2), (int)(hs * 2), YELLOW); };
        H(TL); H(TR); H(BL); H(BR);
    }
}

void LevelEditor::DrawLadEnt(const LadderData& l, bool sel, bool msel) const
{
    Rectangle r = LadRect(l);
    Color bc = sel ? YELLOW : (msel ? Color{ 255,200,0,240 } : Color{ 255,220,0,200 });

    if (_ladderTex && _ladderTex->id > 0) {
        const float sc = 4.f, tileW = 16.f * sc, tileH = 16.f * sc;
        float drawX = r.x + r.width * 0.5f - tileW * 0.5f;
        for (float y = r.y; y < r.y + r.height; y += tileH) {
            float dh = fminf(tileH, r.y + r.height - y), srh = dh / sc;
            DrawTexturePro(*_ladderTex, { 0,0,16.f,srh }, { drawX,y,tileW,dh }, {}, 0.f, WHITE);
        }
    }
    else {
        DrawRectangleRec(r, { 255,220,0,50 });
    }
    DrawRectangleLinesEx(r, sel ? 2.f : 1.5f, bc);
    if (sel || msel) {
        const float hs = 5.f;
        DrawRectangle((int)(r.x - hs), (int)(r.y + r.height * 0.5f - hs), (int)(hs * 2), (int)(hs * 2), YELLOW);
        DrawRectangle((int)(r.x + r.width - hs), (int)(r.y + r.height * 0.5f - hs), (int)(hs * 2), (int)(hs * 2), YELLOW);
    }
}

void LevelEditor::DrawCircEnt(Vector2 pos, float radius, Color c, bool sel, bool msel, const char* lbl) const
{
    if (msel && !sel) DrawCircleV(pos, radius + 5.f, { 255,200,0,180 });
    DrawCircleV(pos, radius + (sel ? 3.f : 0.f), sel ? YELLOW : c);
    DrawCircleV(pos, radius, c);
    if (lbl) { int tw = MeasureText(lbl, 9); DrawText(lbl, (int)pos.x - tw / 2, (int)pos.y - 4, 9, BLACK); }
}

void LevelEditor::DrawBeamEnt(Vector2 pos, bool sel, bool msel) const
{
    if (_beamTex && _beamTex->id > 0) {
        const float sc = 4.f;
        float w = _beamTex->width * sc, h = _beamTex->height * sc;
        DrawTexturePro(*_beamTex, { 0,0,(float)_beamTex->width,(float)_beamTex->height }, { pos.x,pos.y,w,h }, {}, 0.f, WHITE);
        if (sel)  DrawRectangleLinesEx({ pos.x,pos.y,w,h }, 2.f, YELLOW);
        if (msel && !sel) DrawRectangleLinesEx({ pos.x,pos.y,w,h }, 2.f, { 255,200,0,200 });
    }
    else {
        Rectangle r = BeamRect(pos);
        DrawRectangleRec(r, { 120,120,120,80 });
        DrawRectangleLinesEx(r, 1.f, sel ? YELLOW : (msel ? Color{ 255,200,0,200 } : GRAY));
    }
}

// ── Sprite-based entity renderers ─────────────────────────────────────────────

void LevelEditor::DrawPlayerSpawn(Vector2 pos, bool sel, bool msel) const
{
    if (_playerTex && _playerTex->id > 0) {
        const float sc = 3.5f;
        float w = _playerTex->width * sc, h = _playerTex->height * sc;
        Color tint = (msel && !sel) ? Color{ 255,220,100,220 } : WHITE;
        DrawTexturePro(*_playerTex, { 0,0,(float)_playerTex->width,(float)_playerTex->height },
            { pos.x - w * 0.5f,pos.y - h,w,h }, {}, 0.f, tint);
        if (sel || msel) DrawRectangleLinesEx({ pos.x - w * 0.5f,pos.y - h,w,h }, 2.f, sel ? YELLOW : Color{ 255,200,0,220 });
    }
    else {
        DrawCircEnt(pos, 12.f, GREEN, sel, msel, "P");
    }
    DrawCircleV(pos, 4.f, sel ? YELLOW : GREEN);
    DrawText("SPAWN", (int)pos.x + 6, (int)pos.y - 5, 8, GREEN);
}

void LevelEditor::DrawRegulusEnt(Vector2 pos, bool sel, bool msel) const
{
    if (_regulusTex && _regulusTex->id > 0) {
        const float sc = 3.5f * 0.7f * 1.2f;   // REGULUS_SCALE
        float w = _regulusTex->width * sc, h = _regulusTex->height * sc;
        Color tint = (msel && !sel) ? Color{ 255,220,100,220 } : WHITE;
        DrawTexturePro(*_regulusTex, { 0,0,(float)_regulusTex->width,(float)_regulusTex->height },
            { pos.x,pos.y - h,w,h }, {}, 0.f, tint);
        if (sel || msel) DrawRectangleLinesEx({ pos.x,pos.y - h,w,h }, 2.f, sel ? YELLOW : Color{ 255,200,0,220 });
    }
    else {
        DrawCircEnt(pos, 16.f, { 160,32,240,255 }, sel, msel, "R");
    }
    DrawCircleV(pos, 3.f, sel ? YELLOW : Color{ 160,32,240,255 });
}

void LevelEditor::DrawCaveEnt(Vector2 pos, bool sel, bool msel) const
{
    if (_caveTex && _caveTex->id > 0) {
        const float sc = 3.5f;              // HOUSE_DRAW_SCALE
        float w = 64.f * sc, h = 32.f * sc;      // HOUSE_NATIVE_W/H * scale
        Color tint = (msel && !sel) ? Color{ 255,220,100,220 } : WHITE;
        DrawTexturePro(*_caveTex, { 0,0,64.f,32.f }, { pos.x,pos.y,w,h }, {}, 0.f, tint);
        if (sel || msel) DrawRectangleLinesEx({ pos.x,pos.y,w,h }, 2.f, sel ? YELLOW : Color{ 255,200,0,220 });
    }
    else {
        Rectangle r = { pos.x,pos.y,224.f,112.f };
        DrawRectangleRec(r, { 255,165,0,60 });
        DrawRectangleLinesEx(r, sel ? 2.f : 1.5f, sel ? YELLOW : ORANGE);
        DrawText("CAVE", (int)r.x + 2, (int)r.y + 17, 9, ORANGE);
    }
}

// ── Path nodes ────────────────────────────────────────────────────────────────

void LevelEditor::DrawPathNodes()
{
    const auto& nodes = _level.pathNodes;
    for (int i = 0; i < (int)nodes.size(); i++) {
        const auto& n = nodes[i];
        Vector2 from = { n.x,n.y };
        for (int b = 0; b < 2; b++) {
            if (n.next[b] < 0 || n.next[b] >= (int)nodes.size()) continue;
            Vector2 to = { nodes[n.next[b]].x,nodes[n.next[b]].y };
            Color lc = (b == 0) ? Color{ 255,140,0,200 } : Color{ 0,200,255,200 };
            DrawLineEx(from, to, 2.f, lc);
            DrawCircleV(Vector2Lerp(from, to, 0.65f), 3.f, lc);
        }
    }
    for (int i = 0; i < (int)nodes.size(); i++) {
        const auto& n = nodes[i];
        bool sel = (_sel.valid() && _sel.type == (int)EditorTool::PATH_NODE && _sel.index == i);
        bool ms = IsInMultiSel({ (int)EditorTool::PATH_NODE,i });
        Color fill;
        if (i == 0)                                      fill = WHITE;
        else if (n.next[0] == -1 && n.next[1] == -1)         fill = RED;
        else if (n.isSplitNode && n.rollThreshold == 10)   fill = ORANGE;
        else if (n.isSplitNode)                        fill = GREEN;
        else                                           fill = YELLOW;
        Vector2 pos = { n.x,n.y };
        if (ms && !sel) DrawCircleV(pos, 14.f, { 255,200,0,180 });
        if (sel) DrawCircleV(pos, 13.f, YELLOW);
        DrawCircleV(pos, 10.f, BLACK); DrawCircleV(pos, 8.f, fill);
        char lbl[8]; snprintf(lbl, sizeof(lbl), "%d", i);
        int tw = MeasureText(lbl, 8); DrawText(lbl, (int)pos.x - tw / 2, (int)pos.y - 4, 8, BLACK);
    }
    if (_connectMode != ConnectMode::NONE && _connectFrom >= 0 && _connectFrom < (int)nodes.size()) {
        Vector2 src = { nodes[_connectFrom].x,nodes[_connectFrom].y };
        DrawCircleLines((int)src.x, (int)src.y, 16, (_connectMode == ConnectMode::NEXT0) ? ORANGE : SKYBLUE);
        DrawLineEx(src, WorldMouse(), 1.5f, (_connectMode == ConnectMode::NEXT0) ? Color{ 255,140,0,150 } : Color{ 0,200,255,150 });
    }
}

// ── Background ────────────────────────────────────────────────────────────────

void LevelEditor::DrawBackground() const
{
    if (_bgTex && _bgTex->id > 0) {
        DrawTexturePro(*_bgTex, { 0,0,(float)_bgTex->width,(float)_bgTex->height }, { 0,0,(float)_sw,(float)_sh }, {}, 0.f, WHITE);
        DrawRectangle(0, 0, _sw, _sh, { 0,0,0,55 });
    }
    else {
        DrawRectangle(0, 0, _sw, _sh, { 20,22,30,255 });
    }
    DrawRectangleLinesEx({ 0,0,(float)_sw,(float)_sh }, 2.f, { 60,60,80,200 });
}

// ── Grid ──────────────────────────────────────────────────────────────────────

void LevelEditor::DrawGrid() const
{
    if (!_gridOn) return;
    float gs = (float)GRID_SZ / _gridDiv;
    // Major gridlines every GRID_SZ, minor every gs
    for (float x = 0; x <= (float)_sw; x += gs) {
        bool major = (fmodf(x, (float)GRID_SZ) < 0.5f);
        Color c = major ? Color{ 55,60,80,255 } : Color{ 35,38,52,255 };
        DrawLine((int)x, 0, (int)x, _sh, c);
    }
    for (float y = 0; y <= (float)_sh; y += gs) {
        bool major = (fmodf(y, (float)GRID_SZ) < 0.5f);
        Color c = major ? Color{ 55,60,80,255 } : Color{ 35,38,52,255 };
        DrawLine(0, (int)y, _sw, (int)y, c);
    }
}

// ── Draw all level entities ───────────────────────────────────────────────────

void LevelEditor::DrawLevelEntities()
{
    auto IsSel = [&](EditorTool t, int i) { return _sel.valid() && _sel.type == (int)t && _sel.index == i; };
    auto IsMSel = [&](EditorTool t, int i) { return IsInMultiSel({ (int)t,i }); };

    for (int i = 0; i < (int)_level.platforms.size(); i++)
        DrawPlatEnt(_level.platforms[i], IsSel(EditorTool::PLATFORM, i), IsMSel(EditorTool::PLATFORM, i));
    for (int i = 0; i < (int)_level.ladders.size(); i++)
        DrawLadEnt(_level.ladders[i], IsSel(EditorTool::LADDER, i), IsMSel(EditorTool::LADDER, i));
    for (int i = 0; i < (int)_level.beams.size(); i++)
        DrawBeamEnt(_level.beams[i], IsSel(EditorTool::BEAM, i), IsMSel(EditorTool::BEAM, i));
    DrawPathNodes();
    for (int i = 0; i < (int)_level.nukeSpawns.size(); i++)
        DrawCircEnt(_level.nukeSpawns[i], 10.f, SKYBLUE, IsSel(EditorTool::NUKE_SPAWN, i), IsMSel(EditorTool::NUKE_SPAWN, i), "N");
    for (int i = 0; i < (int)_level.beatriceSpawns.size(); i++)
        DrawCircEnt(_level.beatriceSpawns[i], 10.f, MAGENTA, IsSel(EditorTool::BEATRICE_SPAWN, i), IsMSel(EditorTool::BEATRICE_SPAWN, i), "B");
    for (int i = 0; i < (int)_level.enemySpawns.size(); i++)
        DrawCircEnt(_level.enemySpawns[i], 10.f, RED, IsSel(EditorTool::ENEMY_SPAWN, i), IsMSel(EditorTool::ENEMY_SPAWN, i), "E");
    if (_level.hasPlayerSpawn)
        DrawPlayerSpawn(_level.playerSpawn, IsSel(EditorTool::PLAYER_SPAWN, 0), IsMSel(EditorTool::PLAYER_SPAWN, 0));
    if (_level.hasRegulus)
        DrawRegulusEnt(_level.regulusPos, IsSel(EditorTool::REGULUS, 0), IsMSel(EditorTool::REGULUS, 0));
    if (_level.hasCave)
        DrawCaveEnt(_level.cavePos, IsSel(EditorTool::CAVE, 0), IsMSel(EditorTool::CAVE, 0));
}

// ── Placement preview ─────────────────────────────────────────────────────────

void LevelEditor::DrawPlacementPreview() const
{
    Vector2 wm = WorldMouse(), swm = Snap(wm);

    // Drag move indicator
    if (_tool == EditorTool::SELECT && (_dragging || _multiDragging)) {
        const float A = 10.f;
        DrawLineEx({ wm.x - A,wm.y }, { wm.x + A,wm.y }, 2.f, { 255,255,0,200 });
        DrawLineEx({ wm.x,wm.y - A }, { wm.x,wm.y + A }, 2.f, { 255,255,0,200 });
    }

    // Box selection rectangle
    if (_boxSelecting) {
        float x = fminf(_boxStart.x, _boxEnd.x), y = fminf(_boxStart.y, _boxEnd.y);
        float w = fabsf(_boxEnd.x - _boxStart.x), h = fabsf(_boxEnd.y - _boxStart.y);
        DrawRectangle((int)x, (int)y, (int)w, (int)h, { 0,200,255,18 });
        DrawRectangleLinesEx({ x,y,w,h }, 1.5f, { 0,200,255,220 });
    }

    if (_placingPlatform) {
        float w = swm.x - _platStart.x;
        Rectangle r;
        if (w >= 0) r = { _platStart.x,_platStart.y,w > 0 ? w : 8.f,12.f };
        else      r = { swm.x,_platStart.y,-w > 0 ? -w : 8.f,12.f };
        DrawRectangleLinesEx(r, 1.5f, { 80,120,255,180 });
        DrawText(TextFormat("w=%.0f", fabsf(w)), (int)r.x, (int)r.y - 14, 10, { 80,120,255,220 });
    }
    if (_placingLadder) {
        float h = swm.y - _ladStart.y;
        Rectangle r = { _ladStart.x,(h >= 0) ? _ladStart.y : swm.y,40.f,fabsf(h) > 0 ? fabsf(h) : 8.f };
        DrawRectangleLinesEx(r, 1.5f, { 255,220,0,180 });
        DrawText(TextFormat("h=%.0f", fabsf(h)), (int)r.x, (int)r.y - 14, 10, { 255,220,0,220 });
    }

    // Crosshair
    DrawLine((int)swm.x - 8, (int)swm.y, (int)swm.x + 8, (int)swm.y, { 255,255,255,100 });
    DrawLine((int)swm.x, (int)swm.y - 8, (int)swm.x, (int)swm.y + 8, { 255,255,255,100 });
}

// ── Toolbar UI ────────────────────────────────────────────────────────────────

void LevelEditor::DrawToolbarUI() const
{
    DrawRectangle(0, 0, _sw, TOOLBAR_H, { 30,32,42,255 });
    DrawLine(0, TOOLBAR_H - 1, _sw, TOOLBAR_H - 1, { 70,80,110,255 });

    float bw = (float)_sw / 8.f;
    auto TBtn = [&](int c)->Rectangle { return { c * bw + 1,1,bw - 2,(float)TOOLBAR_H - 2 }; };
    auto DrawTB = [&](int c, const char* label, Color bg, Color fg) {
        Rectangle r = TBtn(c);
        DrawRectangleRec(r, bg); DrawRectangleLinesEx(r, 1, { 80,90,120,255 });
        int tw = MeasureText(label, 13);
        DrawText(label, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - 7), 13, fg);
        };

    DrawTB(0, "<", { 40,42,55,255 }, WHITE);
    DrawTB(1, TextFormat("Lv%d", _levelId), { 50,55,75,255 }, YELLOW);
    DrawTB(2, ">", { 40,42,55,255 }, WHITE);
    DrawTB(3, _gridOn ? "Grid:ON" : "Grid:OFF", { 40,42,55,255 }, _gridOn ? GREEN : GRAY);
    DrawTB(4, TextFormat("÷%d", _gridDiv), { 40,42,55,255 }, { 100,200,255,255 });
    DrawTB(5, "SAVE [^S]", { 30,80,50,255 }, WHITE);
    DrawTB(6, "PLAY", { 30,60,100,255 }, WHITE);
    DrawTB(7, "MENU [B]", { 80,30,30,255 }, WHITE);
}

// ── Browser UI ────────────────────────────────────────────────────────────────

void LevelEditor::DrawBrowserUI()
{
    int by0 = _sh - BROWSER_H;
    DrawRectangle(0, by0, _sw, BROWSER_H, { 24,26,36,255 });
    DrawLine(0, by0, _sw, by0, { 70,80,110,255 });

    const EditorTool row0[] = { EditorTool::SELECT,EditorTool::PLAYER_SPAWN,EditorTool::REGULUS,EditorTool::CAVE,EditorTool::PLATFORM,EditorTool::LADDER };
    const EditorTool row1[] = { EditorTool::BEAM,EditorTool::PATH_NODE,EditorTool::NUKE_SPAWN,EditorTool::BEATRICE_SPAWN,EditorTool::ENEMY_SPAWN };

    auto DrawTool = [&](int row, int col, int cols, EditorTool t) {
        Rectangle r = BrowserBtn(row, col, cols);
        bool active = (_tool == t);
        Color tc = ToolColor(t);
        Color bg = active ? Color{ (unsigned char)(tc.r / 3),(unsigned char)(tc.g / 3),(unsigned char)(tc.b / 3),255 } : Color{ 30,32,44,255 };
        DrawRectangleRec(r, bg);
        DrawRectangleLinesEx(r, active ? 2.f : 1.f, active ? tc : Color{ 60,65,85,255 });
        const char* nm = ToolName(t); int tw = MeasureText(nm, 11);
        DrawText(nm, (int)(r.x + r.width / 2 - tw / 2), (int)(r.y + r.height / 2 - 5), 11, active ? tc : Color{ 180,185,200,255 });
        };
    for (int c = 0; c < 6; c++) DrawTool(0, c, 6, row0[c]);
    for (int c = 0; c < 5; c++) DrawTool(1, c, 5, row1[c]);

    float sy = (float)(_sh - 24);
    DrawRectangle(0, (int)sy - 2, _sw, 26, { 18,20,28,255 });
    Color sc = _statusTimer > 0.f ? YELLOW : Color{ 100,105,120,255 };
    const char* smsg = _statusTimer > 0.f ? _status
        : "LMB=Place  RMB=Delete  DEL=Del  B=Menu  G=Grid  [/]=GridDiv  ^Z=Undo  ^Y=Redo  ^S=Save";
    DrawText(smsg, 8, (int)sy, 12, sc);
}

// ── Properties panel ──────────────────────────────────────────────────────────

void LevelEditor::DrawPropertiesPanel()
{
    float py = (float)(_sh - BROWSER_H) + 6 + 2 * (38 + 4) + 4;
    float ph = (float)(_sh - 26) - py;
    if (ph < 10) return;

    if (!_sel.valid()) {
        DrawText(TextFormat("Level %d  |  Plat:%d  Lad:%d  Nodes:%d  Beams:%d  Nukes:%d  Bea:%d  Enemies:%d  |  Undo:%d",
            _levelId, (int)_level.platforms.size(), (int)_level.ladders.size(),
            (int)_level.pathNodes.size(), (int)_level.beams.size(),
            (int)_level.nukeSpawns.size(), (int)_level.beatriceSpawns.size(),
            (int)_level.enemySpawns.size(), (int)_undoStack.size()),
            8, (int)py, 11, { 140,145,160,255 });
        return;
    }

    EditorTool t = (EditorTool)_sel.type;
    int i = _sel.index;

    // Delete button
    Rectangle delBtn = { (float)_sw - 72,py,68,22 };
    bool delHov = CheckCollisionPointRec(GetMousePosition(), delBtn);
    DrawRectangleRec(delBtn, delHov ? Color{ 180,30,30,255 } : Color{ 100,20,20,255 });
    DrawText("DELETE", (int)delBtn.x + 6, (int)delBtn.y + 5, 11, WHITE);
    if (delHov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        _multiSel.empty() ? DeleteSelected() : DeleteMultiSelected();
    }

    char buf[512] = {};
    switch (t)
    {
    case EditorTool::PLATFORM: {
        auto& p = _level.platforms[i];
        snprintf(buf, sizeof(buf), "PLATFORM #%d  x=%.0f  y=%.0f  w=%.0f  h=%.0f  tilt=%.1f", i, p.x, p.y, p.w, p.h, p.tilt);
        DrawText(buf, 8, (int)py, 12, ToolColor(t));

        float bx = 8, brow = py + 16;
        auto SBtn = [&](const char* lbl, float x, float bw, Color c)->Rectangle {
            Rectangle r = { x,brow,bw,18 };
            DrawRectangleRec(r, c);
            int tw = MeasureText(lbl, 10); DrawText(lbl, (int)(x + bw / 2 - tw / 2), (int)brow + 4, 10, WHITE);
            return r;
            };
        Vector2 mp = GetMousePosition(); bool lp = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

        // Tilt controls
        Rectangle rTM = SBtn("-1", bx, 28, { 50,50,70,255 }); bx += 31;
        Rectangle rTP = SBtn("+1", bx, 28, { 50,50,70,255 }); bx += 31;
        Rectangle rTM10 = SBtn("-10", bx, 34, { 40,40,65,255 }); bx += 37;
        Rectangle rTP10 = SBtn("+10", bx, 34, { 40,40,65,255 }); bx += 37;
        Rectangle rT0 = SBtn("0°", bx, 28, { 60,40,40,255 }); bx += 31;
        Rectangle rT90 = SBtn("90°", bx, 34, { 40,60,40,255 }); bx += 37;
        DrawText(TextFormat("=%.0f°", p.tilt), (int)bx, (int)brow + 4, 11, LIGHTGRAY); bx += 50;

        // Width controls
        Rectangle rWM = SBtn("W-", bx, 28, { 50,50,70,255 }); bx += 31;
        Rectangle rWP = SBtn("W+", bx, 28, { 50,50,70,255 }); bx += 31;

        if (lp) {
            if (CheckCollisionPointRec(mp, rTM)) { PushUndo(); p.tilt -= 1.f; }
            if (CheckCollisionPointRec(mp, rTP)) { PushUndo(); p.tilt += 1.f; }
            if (CheckCollisionPointRec(mp, rTM10)) { PushUndo(); p.tilt -= 10.f; }
            if (CheckCollisionPointRec(mp, rTP10)) { PushUndo(); p.tilt += 10.f; }
            if (CheckCollisionPointRec(mp, rT0)) { PushUndo(); p.tilt = 0.f; }
            if (CheckCollisionPointRec(mp, rT90)) { PushUndo(); p.tilt = 90.f; }
            if (CheckCollisionPointRec(mp, rWM) && p.w > GRID_SZ) { PushUndo(); p.w -= (float)GRID_SZ; }
            if (CheckCollisionPointRec(mp, rWP)) { PushUndo(); p.w += (float)GRID_SZ; }
        }
        break;
    }
    case EditorTool::LADDER: {
        auto& l = _level.ladders[i];
        snprintf(buf, sizeof(buf), "LADDER #%d  x=%.0f  y=%.0f  w=%.0f  h=%.0f", i, l.x, l.y, l.w, l.h);
        DrawText(buf, 8, (int)py, 12, ToolColor(t));
        // H controls
        float bx = 8, brow = py + 16;
        auto SBtn = [&](const char* lbl, float x, Color c)->Rectangle {
            Rectangle r = { x,brow,30,18 };
            DrawRectangleRec(r, c);
            int tw = MeasureText(lbl, 10); DrawText(lbl, (int)(x + 15 - tw / 2), (int)brow + 4, 10, WHITE);
            return r;
            };
        Rectangle rHM = SBtn("H-", bx, { 50,50,70,255 }); bx += 33;
        Rectangle rHP = SBtn("H+", bx, { 50,50,70,255 });
        Vector2 mp = GetMousePosition(); bool lp = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (lp) {
            if (CheckCollisionPointRec(mp, rHM) && l.h > GRID_SZ) { PushUndo(); l.h -= (float)GRID_SZ; }
            if (CheckCollisionPointRec(mp, rHP)) { PushUndo(); l.h += (float)GRID_SZ; }
        }
        break;
    }
    case EditorTool::PATH_NODE: {
        auto& n = _level.pathNodes[i];
        snprintf(buf, sizeof(buf), "NODE #%d  pos=(%.0f,%.0f)  split=%s  roll=%d", i, n.x, n.y, n.isSplitNode ? "YES" : "NO", n.rollThreshold);
        DrawText(buf, 8, (int)py, 12, ToolColor(t));

        float bx = 8, brow = py + 16;
        auto SBtn = [&](const char* lbl, float x, Color c)->Rectangle {
            Rectangle r = { x,brow,58,18 };
            DrawRectangleRec(r, c);
            int tw = MeasureText(lbl, 10); DrawText(lbl, (int)(x + r.width / 2 - tw / 2), (int)brow + 4, 10, WHITE);
            return r;
            };
        Vector2 mp = GetMousePosition(); bool lp = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        Rectangle rN0 = SBtn("Set N0", bx, { 180,80,0,255 }); bx += 62;
        Rectangle rN1 = SBtn("Set N1", bx, { 0,100,180,255 }); bx += 62;
        Rectangle rSpl = SBtn(n.isSplitNode ? "Split:ON" : "Split:OFF", bx, { 50,50,80,255 }); bx += 62;
        Rectangle rRM = SBtn("Roll-", bx, { 50,50,70,255 }); bx += 62;
        Rectangle rRP = SBtn("Roll+", bx, { 50,50,70,255 }); bx += 62;
        DrawText(TextFormat("=%d", n.rollThreshold), (int)bx, (int)brow + 4, 11, LIGHTGRAY);

        if (lp) {
            double now = GetTime();
            if (CheckCollisionPointRec(mp, rN0)) { _connectMode = ConnectMode::NEXT0; _connectFrom = i; SetStatus(TextFormat("Click node to set N0 of node %d", i)); }
            if (CheckCollisionPointRec(mp, rN1)) { _connectMode = ConnectMode::NEXT1; _connectFrom = i; SetStatus(TextFormat("Click node to set N1 of node %d", i)); }
            if (CheckCollisionPointRec(mp, rSpl)) { PushUndo(); n.isSplitNode = !n.isSplitNode; }
            if (CheckCollisionPointRec(mp, rRM) && now - _lastThreshClick > 0.15) { PushUndo(); n.rollThreshold = std::max(0, n.rollThreshold - 1); _lastThreshClick = now; }
            if (CheckCollisionPointRec(mp, rRP) && now - _lastThreshClick > 0.15) { PushUndo(); n.rollThreshold = std::min(10, n.rollThreshold + 1); _lastThreshClick = now; }
        }
        char cnx[64]; snprintf(cnx, sizeof(cnx), "N0→%d  N1→%d", n.next[0], n.next[1]);
        DrawText(cnx, (int)(8 + 5 * 62), (int)brow + 4, 11, { 200,200,200,255 });
        break;
    }
    default: {
        snprintf(buf, sizeof(buf), "%s @ (%.0f, %.0f)", ToolName(t), GetSelPos().x, GetSelPos().y);
        DrawText(buf, 8, (int)py, 12, ToolColor(t));
        break;
    }
    }
}

// ── Master Draw ───────────────────────────────────────────────────────────────

void LevelEditor::Draw()
{
    BeginMode2D(_cam);
    DrawBackground();
    DrawGrid();
    DrawLevelEntities();
    DrawPlacementPreview();
    EndMode2D();

    DrawRectangleLinesEx({ 0,(float)TOOLBAR_H,(float)_sw,_canvasH }, 1.f, { 60,70,100,255 });

    DrawToolbarUI();
    DrawBrowserUI();
    DrawPropertiesPanel();

    if (_connectMode != ConnectMode::NONE) {
        const char* hint = (_connectMode == ConnectMode::NEXT0)
            ? ">> Click a node to set NEXT[0]  (ESC = cancel)"
            : ">> Click a node to set NEXT[1]  (ESC = cancel)";
        int tw = MeasureText(hint, 14), tx = _sw / 2 - tw / 2, ty = _sh / 2 - 10;
        DrawRectangle(tx - 8, ty - 6, tw + 16, 26, { 0,0,0,200 });
        DrawText(hint, tx, ty, 14, (_connectMode == ConnectMode::NEXT0) ? Color{ 255,140,0,255 } : Color{ 0,200,255,255 });
    }

    // Multi-sel count overlay
    if (_multiSel.size() > 1) {
        const char* ms = TextFormat("%d selected  [DEL=delete  drag any=move all]", (int)_multiSel.size());
        int tw = MeasureText(ms, 13), tx = _sw / 2 - tw / 2, ty = TOOLBAR_H + 8;
        DrawRectangle(tx - 6, ty - 4, tw + 12, 22, { 0,0,0,180 });
        DrawText(ms, tx, ty, 13, { 255,200,0,255 });
    }
}