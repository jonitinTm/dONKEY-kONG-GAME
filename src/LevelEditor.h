#pragma once
// ============================================================
//  LevelEditor.h  –  In-game level editor (raylib)
//  Layout  (875 × 950 screen):
//    Toolbar  y  0 – 45      level nav, grid, save, play, menu
//    Canvas   y 45 – 785     875×740 → shows 875×950 world (zoom ≈ 0.779)
//    Browser  y 785 – 950    tool palette + properties panel
// ============================================================
#include "raylib.h"
#include "LevelData.h"
#include <vector>
#include <string>

// ── Tool enum ─────────────────────────────────────────────────────────────────

enum class EditorTool : int {
    SELECT = 0,
    PLAYER_SPAWN,
    REGULUS,
    CAVE,
    PLATFORM,       // drag to set width
    LADDER,         // drag to set height
    BEAM,
    PATH_NODE,
    NUKE_SPAWN,
    BEATRICE_SPAWN,
    ENEMY_SPAWN,
    TOOL_COUNT
};

// ── Connection mode for path-node editing ─────────────────────────────────────

enum class ConnectMode { NONE, NEXT0, NEXT1 };

// ── Selected-entity token ────────────────────────────────────────────────────

struct SelectedEnt {
    int type  = -1;    // cast to (int)EditorTool; -1 = nothing selected
    int index = -1;
    bool valid() const { return type >= 0 && index >= 0; }
    void clear()       { type = -1; index = -1; }
};

// ── Editor class ─────────────────────────────────────────────────────────────

class LevelEditor
{
public:
    // ── Lifecycle ────────────────────────────────────────────────────────────
    void Init(int screenW = 875, int screenH = 950);
    void Update(float dt);

    /// Call between BeginDrawing() … EndDrawing()
    void Draw();

    // ── Level management ─────────────────────────────────────────────────────
    void LoadLevel(int id);
    void SaveCurrentLevel();
    int  GetCurrentLevelId() const { return _levelId; }

    /// Returns the level data ready to be applied to the game.
    const LevelData& GetLevel() const { return _level; }

    // ── Screen-transition flags ───────────────────────────────────────────────
    bool WantsMenu() const { return _wantsMenu; }
    bool WantsPlay() const { return _wantsPlay; }
    void ClearFlags()      { _wantsMenu = false; _wantsPlay = false; }

    // ── Optional textures for visual rendering in canvas ─────────────────────
    /// Pass the game's background, beam-tile and ladder-tile textures so the
    /// editor canvas renders them instead of plain primitives.
    void SetTextures(Texture2D* bg, Texture2D* beamTile, Texture2D* ladderTile)
    {
        _bgTex      = bg;
        _beamTex    = beamTile;
        _ladderTex  = ladderTile;
    }

private:
    // ── Layout constants ──────────────────────────────────────────────────────
    int   _sw = 875, _sh = 950;
    static constexpr int TOOLBAR_H  = 45;
    static constexpr int BROWSER_H  = 165;
    static constexpr int GRID_SZ    = 16;

    float    _canvasH = 740.f;
    float    _zoom    = 1.f;
    Camera2D _cam     = {};

    // ── Level state ───────────────────────────────────────────────────────────
    int       _levelId = 1;
    LevelData _level;

    // ── Optional render textures ──────────────────────────────────────────────
    Texture2D* _bgTex     = nullptr;   // game background
    Texture2D* _beamTex   = nullptr;   // beam floor tile
    Texture2D* _ladderTex = nullptr;   // ladder tile

    // ── Editor state ─────────────────────────────────────────────────────────
    EditorTool  _tool   = EditorTool::SELECT;
    SelectedEnt _sel;
    bool        _gridOn = true;

    bool _wantsMenu = false;
    bool _wantsPlay = false;

    // Entity drag
    bool    _dragging   = false;
    Vector2 _dragOffset = {};

    // Platform drag-to-create
    bool    _placingPlatform = false;
    Vector2 _platStart       = {};

    // Ladder drag-to-create
    bool    _placingLadder = false;
    Vector2 _ladStart      = {};

    // Path-node connections
    ConnectMode _connectMode = ConnectMode::NONE;
    int         _connectFrom = -1;   // index of source node

    // Roll threshold editing (+/- buttons)
    double _lastThreshClick = -999.0;

    // Status bar
    char   _status[256] = {};
    float  _statusTimer  = 0.f;

    // ── Private helpers ───────────────────────────────────────────────────────

    Vector2 WorldMouse()          const;
    Vector2 Snap(Vector2 v)       const;
    bool    InCanvas()            const;
    bool    InBrowser()           const;
    bool    InToolbar()           const;

    // Entity geometry
    Rectangle PlatRect(const PlatformData& p) const;
    Rectangle LadRect (const LadderData&   l) const;
    Rectangle BeamRect(Vector2 pos)           const;

    // Pick the topmost entity at world position p; fills _sel.
    bool PickEntity(Vector2 p);

    // Delete the currently selected entity.
    void DeleteSelected();

    // Get / set world position of the selected entity.
    Vector2 GetSelPos()         const;
    void    SetSelPos(Vector2 p);

    // ── Update sub-routines ───────────────────────────────────────────────────
    void UpdateToolbar();
    void UpdateBrowser();
    void UpdateCanvas();

    // ── Draw sub-routines ─────────────────────────────────────────────────────
    void DrawBackground()        const;
    void DrawGrid()              const;
    void DrawLevelEntities();
    void DrawPlacementPreview()  const;
    void DrawToolbarUI()         const;
    void DrawBrowserUI();
    void DrawPropertiesPanel();

    // Entity draw helpers
    void DrawPlatEnt (const PlatformData& p, bool sel) const;
    void DrawLadEnt  (const LadderData&   l, bool sel) const;
    void DrawCircEnt (Vector2 pos, float r, Color c, bool sel, const char* lbl) const;
    void DrawBeamEnt (Vector2 pos, bool sel) const;
    void DrawPathNodes();

    // Browser layout helper  (row 0/1, col 0..(cols-1))
    Rectangle BrowserBtn(int row, int col, int cols) const;

    // Tool metadata
    static const char* ToolName (EditorTool t);
    static Color       ToolColor(EditorTool t);

    void SetStatus(const char* msg, float dur = 2.5f);
};
