#pragma once
// ============================================================
//  LevelEditor.h
//  Layout: Toolbar(45) | Canvas(left) + RightPanel(210) | Browser(150)
// ============================================================
#include "raylib.h"
#include "LevelData.h"
#include "CinematicData.h"
#include <vector>
#include <string>

enum class EditorTool : int {
    SELECT = 0, PLAYER_SPAWN, REGULUS, CAVE,
    PLATFORM, LADDER, BEAM, PATH_NODE,
    NUKE_SPAWN, BEATRICE_SPAWN, ENEMY_SPAWN,
    TOOL_COUNT
};
enum class GizmoMode { SELECT = 0, MOVE, ROTATE, SCALE };
enum class GizmoAxis { NONE, X, Y, FREE, RING };
enum class ConnectMode { NONE, NEXT0, NEXT1 };

struct SelectedEnt {
    int type = -1, index = -1;
    bool valid()  const { return type >= 0 && index >= 0; }
    void clear() { type = -1; index = -1; }
    bool operator==(const SelectedEnt& o) const { return type == o.type && index == o.index; }
};

// One row in the outliner
struct OutlineRow {
    SelectedEnt ent;
    const char* icon;
    char        name[48];
    Color       color;
};

class LevelEditor
{
public:
    void Init(int sw = 875, int sh = 950);
    void Update(float dt);
    void Draw();

    void LoadLevel(int id);
    void SaveCurrentLevel();
    int  GetCurrentLevelId()    const { return _levelId; }
    const LevelData& GetLevel() const { return _level; }

    bool WantsMenu() const { return _wantsMenu; }
    bool WantsPlay() const { return _wantsPlay; }
    void ClearFlags() { _wantsMenu = false; _wantsPlay = false; }

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
    // ── Layout ───────────────────────────────────────────────────────────────
    int _sw = 875, _sh = 950;
    static constexpr int TOOLBAR_H = 45;
    static constexpr int BROWSER_H = 150;
    static constexpr int RIGHT_W = 210;   // outliner + data panel
    static constexpr int GRID_SZ = 16;
    static constexpr int OUTLINE_ROW = 18;    // px per outliner row

    // ── Cinematic Sequencer panel ─────────────────────────────────────────
    static constexpr int SEQ_H = 270;   // total sequencer panel height
    static constexpr int SEQ_BW = 158;   // content browser column width
    static constexpr int SEQ_HDR = 108;   // track name column width
    static constexpr int SEQ_CTRL_H = 28;    // controls bar height
    static constexpr float SEQ_RH = 18.f;  // ruler height
    static constexpr float SEQ_TH = 22.f;  // track row height

    float _canvasH = 0.f;   // _sh - TOOLBAR_H - BROWSER_H
    int   _canvasW = 0;     // _sw - RIGHT_W
    float _zoom = 1.f;
    Camera2D _cam = {};

    // ── Level ─────────────────────────────────────────────────────────────────
    int       _levelId = 1;
    LevelData _level;

    std::vector<LevelData> _undoStack, _redoStack;
    static constexpr int MAX_UNDO = 60;
    void PushUndo(); void Undo(); void Redo();

    // Clipboard — one entry per copied entity
    struct ClipboardEntry {
        int          type = -1;
        PlatformData plat = {};
        LadderData   lad = {};
        PathNodeData node = {};
        Vector2      pos = {};   // for spawn/beam/nuke/beatrice/enemy
    };
    std::vector<ClipboardEntry> _clipboard;
    void CopySelected();
    void PasteClipboard(bool grabAfter = true);
    void DuplicateSelected();

    // ── Textures ──────────────────────────────────────────────────────────────
    Texture2D* _bgTex = nullptr, * _beamTex = nullptr, * _ladderTex = nullptr;
    Texture2D* _playerTex = nullptr, * _regulusTex = nullptr, * _caveTex = nullptr;

    // ── Tool / gizmo state ────────────────────────────────────────────────────
    EditorTool _tool = EditorTool::SELECT;
    GizmoMode  _gizmo = GizmoMode::SELECT;  // Q/W/E/R
    SelectedEnt _sel;
    bool _gridOn = true, _wantsMenu = false, _wantsPlay = false;
    int  _gridDiv = 1;

    // Gizmo drag
    GizmoAxis _gizmoHot = GizmoAxis::NONE;
    GizmoAxis _gizmoDragAxis = GizmoAxis::NONE;
    bool      _gizmoDragging = false;
    Vector2   _gizmoDragStart = {};
    float     _gizmoValStart = 0.f;
    Vector2   _gizmoPosStart = {};

    // Direct operations: G=move, R=rotate, S=scale  (Blender-style)
    enum class DirectOp { NONE, MOVE, ROTATE, SCALE };
    DirectOp _directOp = DirectOp::NONE;
    bool    _grabAxisX = false, _grabAxisY = false;
    Vector2 _grabMouseStart = {};
    Vector2 _grabSingleOrigin = {};
    float   _grabValOrigin = 0.f;           // original tilt/width/height
    std::vector<Vector2> _grabOrigins;    // multi-sel origins
    std::vector<float>   _grabValOrigins; // multi-sel scalar origins

    // Box selection
    bool    _boxSelecting = false;
    Vector2 _boxStart = {}, _boxEnd = {};
    std::vector<SelectedEnt> _multiSel;

    // Multi-drag (via gizmo)
    bool    _multiDragging = false;
    Vector2 _multiDragAnchor = {};
    std::vector<Vector2> _multiDragOrigins;

    // Placement
    bool    _placingPlatform = false; Vector2 _platStart = {};
    bool    _placingLadder = false;   Vector2 _ladStart = {};

    // Single-drag fallback (no gizmo)
    bool    _dragging = false;
    Vector2 _dragOffset = {};

    ConnectMode _connectMode = ConnectMode::NONE;
    int         _connectFrom = -1;
    double      _lastThreshClick = -999.0;

    // Outliner
    std::vector<OutlineRow> _outline;     // rebuilt each frame
    int _outlineScroll = 0;                 // first visible row index

    // Data-panel field drag (click+drag on numeric field like Blender)
    bool    _fieldDrag = false;
    float* _fieldPtr = nullptr;
    float   _fieldDragStartX = 0.f;
    float   _fieldDragStartVal = 0.f;
    float   _fieldDragSens = 1.f;
    float   _fieldMin = 0.f, _fieldMax = 0.f;

    char  _status[256] = {};
    float _statusTimer = 0.f;

    // ── Cinematic Sequencer state ─────────────────────────────────────────
    bool   _seqOpen = false;          // sequencer panel visible
    int    _activeSeq = -1;            // index in _seqList (-1=none)
    std::vector<CinematicSequence> _seqList;   // all loaded sequences
    LevelData _seqSnapshot;             // pre-preview level state for restore

    float  _seqTime = 0.f;           // playhead position (seconds)
    bool   _seqPlaying = false;         // preview playback active
    float  _seqZoom = 80.f;          // px/second on timeline
    float  _seqScrollX = 0.f;           // horizontal scroll (seconds offset)
    int    _seqTrackScroll = 0;         // first visible track index
    int    _selTrack = -1;            // selected track index
    int    _selKey = -1;            // selected keyframe index within track

    bool   _seqDragPlayhead = false;
    bool   _seqDragKey = false;
    float  _seqKeyDragOrig = 0.f;

    int    _seqNameCounter = 1;        // for auto-naming new sequences

    void SeqLoad();
    void SeqSave();
    void SeqNew();
    void SeqDelete();
    void SeqAddKeyframe();              // S key: record entity state at _seqTime
    void SeqDeleteKeyframe();
    void SeqPreviewUpdate(float dt);
    void SeqPreviewStop();

    void UpdateSequencer();
    void DrawSequencer();
    void DrawSeqBrowser();
    void DrawSeqTimeline();
    void DrawSeqControls();

    // Timeline geometry helpers
    float SeqTimeToX(float t)  const;   // t (seconds) -> screen x
    float SeqXToTime(float px) const;   // screen x -> t
    Rectangle SeqTimelineRect() const;
    Rectangle SeqBrowserRect()  const;
    Rectangle SeqControlsRect() const;
    Rectangle SeqTrackRowRect(int row) const;

    // ── Helpers ───────────────────────────────────────────────────────────────
    Vector2 WorldMouse()  const;
    Vector2 Snap(Vector2) const;
    bool InCanvas()   const;  // excludes right panel and browser
    bool InBrowser()  const;
    bool InToolbar()  const;
    bool InRightPanel() const;
    Rectangle RightPanelRect()  const;
    Rectangle OutlinerRect()    const;
    Rectangle DataPanelRect()   const;

    Rectangle PlatRect(const PlatformData& p)           const;
    Rectangle LadRect(const LadderData& l)             const;
    Rectangle BeamRect(Vector2 pos)                     const;
    bool      PointInPlatform(Vector2 pt, const PlatformData& p) const;
    Vector2   PlatformCenter(const PlatformData& p)     const;
    Vector2   EntityCenter(const SelectedEnt& e)        const;

    Vector2 GetEntPos(const SelectedEnt& e)     const;
    void    SetEntPos(const SelectedEnt& e, Vector2 p);

    bool PickEntity(Vector2 p);
    bool IsInMultiSel(const SelectedEnt& e) const;
    void BoxSelectEntities(Rectangle box);
    void DeleteSelected(); void DeleteMultiSelected();
    void SelectEnt(SelectedEnt e);   // select and scroll outliner

    Vector2 GetSelPos() const { return GetEntPos(_sel); }
    void    SetSelPos(Vector2 p) { SetEntPos(_sel, p); }

    // Direct ops (G/R/S like Blender)
    void StartDirectOp(DirectOp op);
    void UpdateDirectOp();
    void ConfirmDirectOp();
    void CancelDirectOp();

    // Gizmo
    void      UpdateGizmo();
    void      DrawGizmo()    const;
    GizmoAxis GizmoHitTest(Vector2 center, Vector2 wm) const;
    static constexpr float GIZMO_R = 60.f;

    void BuildOutline();

    void UpdateToolbar(); void UpdateBrowser();
    void UpdateCanvas();  void UpdateRightPanel();

    void DrawBackground()       const;
    void DrawGrid()             const;
    void DrawLevelEntities();
    void DrawPlacementPreview() const;
    void DrawToolbarUI()        const;
    void DrawBrowserUI();
    void DrawOutliner();
    void DrawDataPanel();

    void DrawPlatEnt(const PlatformData& p, bool sel, bool msel) const;
    void DrawLadEnt(const LadderData& l, bool sel, bool msel) const;
    void DrawCircEnt(Vector2 pos, float r, Color c, bool sel, bool msel, const char* lbl) const;
    void DrawBeamEnt(Vector2 pos, bool sel, bool msel)            const;
    void DrawPathNodes();
    void DrawPlayerSpawn(Vector2 pos, bool sel, bool msel) const;
    void DrawRegulusEnt(Vector2 pos, bool sel, bool msel) const;
    void DrawCaveEnt(Vector2 pos, bool sel, bool msel) const;

    // Numeric field widget: click+drag to change value
    // Returns true if value changed this frame
    bool NumField(const char* label, float& val, float sens,
        float minV, float maxV, float x, float y, float fw);

    Rectangle BrowserBtn(int row, int col, int cols) const;
    static const char* ToolName(EditorTool t);
    static Color       ToolColor(EditorTool t);
    void SetStatus(const char* msg, float dur = 2.5f);
};