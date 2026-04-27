#pragma once
// ============================================================
//  LevelEditor.h  –  In-game level editor (raylib)
// ============================================================
#include "raylib.h"
#include "LevelData.h"
#include <vector>
#include <string>

enum class EditorTool : int {
    SELECT = 0, PLAYER_SPAWN, REGULUS, CAVE,
    PLATFORM, LADDER, BEAM, PATH_NODE,
    NUKE_SPAWN, BEATRICE_SPAWN, ENEMY_SPAWN,
    TOOL_COUNT
};
enum class ConnectMode { NONE, NEXT0, NEXT1 };

struct SelectedEnt {
    int type = -1;
    int index = -1;
    bool valid() const { return type >= 0 && index >= 0; }
    void clear() { type = -1; index = -1; }
    bool operator==(const SelectedEnt& o) const { return type == o.type && index == o.index; }
};

class LevelEditor
{
public:
    void Init(int screenW = 875, int screenH = 950);
    void Update(float dt);
    void Draw();

    void LoadLevel(int id);
    void SaveCurrentLevel();
    int  GetCurrentLevelId()   const { return _levelId; }
    const LevelData& GetLevel() const { return _level; }

    bool WantsMenu() const { return _wantsMenu; }
    bool WantsPlay() const { return _wantsPlay; }
    void ClearFlags() { _wantsMenu = false; _wantsPlay = false; }

    // playerTex = imgMarioIdle, regulusTex = RegulusIdle1, caveTex = House1
    void SetGameTextures(Texture2D* bg, Texture2D* beam, Texture2D* ladder,
        Texture2D* player, Texture2D* regulus, Texture2D* cave)
    {
        _bgTex = bg; _beamTex = beam; _ladderTex = ladder;
        _playerTex = player; _regulusTex = regulus; _caveTex = cave;
    }
    void SetTextures(Texture2D* bg, Texture2D* beam, Texture2D* ladder)
    {
        _bgTex = bg; _beamTex = beam; _ladderTex = ladder;
    }

private:
    int _sw = 875, _sh = 950;
    static constexpr int TOOLBAR_H = 45, BROWSER_H = 165, GRID_SZ = 16;
    float    _canvasH = 740.f, _zoom = 1.f;
    Camera2D _cam = {};

    int       _levelId = 1;
    LevelData _level;

    // Undo/Redo
    std::vector<LevelData> _undoStack, _redoStack;
    static constexpr int MAX_UNDO = 50;
    void PushUndo(); void Undo(); void Redo();

    // Textures
    Texture2D* _bgTex = nullptr, * _beamTex = nullptr, * _ladderTex = nullptr;
    Texture2D* _playerTex = nullptr, * _regulusTex = nullptr, * _caveTex = nullptr;

    // Editor state
    EditorTool  _tool = EditorTool::SELECT;
    SelectedEnt _sel;
    bool _gridOn = true;
    int  _gridDiv = 1;               // 1 / 2 / 4
    bool _wantsMenu = false, _wantsPlay = false;

    // Single drag
    bool    _dragging = false;
    Vector2 _dragOffset = {};

    // Box selection
    bool    _boxSelecting = false;
    Vector2 _boxStart = {}, _boxEnd = {};
    std::vector<SelectedEnt> _multiSel;

    // Multi-drag
    bool                 _multiDragging = false;
    Vector2              _multiDragAnchor = {};
    std::vector<Vector2> _multiDragOrigins;

    // Placement
    bool _placingPlatform = false; Vector2 _platStart = {};
    bool _placingLadder = false;   Vector2 _ladStart = {};

    ConnectMode _connectMode = ConnectMode::NONE;
    int         _connectFrom = -1;
    double      _lastThreshClick = -999.0;

    char  _status[256] = {};
    float _statusTimer = 0.f;

    // Helpers
    Vector2   WorldMouse() const;
    Vector2   Snap(Vector2 v) const;
    bool InCanvas() const; bool InBrowser() const; bool InToolbar() const;

    Rectangle PlatRect(const PlatformData& p) const;
    Rectangle LadRect(const LadderData& l) const;
    Rectangle BeamRect(Vector2 pos)           const;

    Vector2 GetEntPos(const SelectedEnt& e)           const;
    void    SetEntPos(const SelectedEnt& e, Vector2 p);

    bool PickEntity(Vector2 p);
    bool IsInMultiSel(const SelectedEnt& e)            const;
    void BoxSelectEntities(Rectangle box);
    void DeleteSelected();
    void DeleteMultiSelected();

    Vector2 GetSelPos()          const { return GetEntPos(_sel); }
    void    SetSelPos(Vector2 p) { SetEntPos(_sel, p); }

    void UpdateToolbar(); void UpdateBrowser(); void UpdateCanvas();

    void DrawBackground()       const;
    void DrawGrid()             const;
    void DrawLevelEntities();
    void DrawPlacementPreview() const;
    void DrawToolbarUI()        const;
    void DrawBrowserUI();
    void DrawPropertiesPanel();

    void DrawPlatEnt(const PlatformData& p, bool sel, bool msel) const;
    void DrawLadEnt(const LadderData& l, bool sel, bool msel) const;
    void DrawCircEnt(Vector2 pos, float r, Color c,
        bool sel, bool msel, const char* lbl)        const;
    void DrawBeamEnt(Vector2 pos, bool sel, bool msel)            const;
    void DrawPathNodes();
    void DrawPlayerSpawn(Vector2 pos, bool sel, bool msel) const;
    void DrawRegulusEnt(Vector2 pos, bool sel, bool msel) const;
    void DrawCaveEnt(Vector2 pos, bool sel, bool msel) const;

    Rectangle BrowserBtn(int row, int col, int cols) const;
    static const char* ToolName(EditorTool t);
    static Color       ToolColor(EditorTool t);
    void SetStatus(const char* msg, float dur = 2.5f);
};