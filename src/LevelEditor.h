#pragma once
// ============================================================
//  LevelEditor.h
//  Layout: Toolbar(45) | Canvas(left) + RightPanel(210) | Browser(150)
//  Sequencer panel replaces Browser when open (Tab key)
// ============================================================
#include "raylib.h"
#include "LevelData.h"
#include "Lighting.h"
#include "CinematicData.h"
#include <vector>
#include <string>

enum class EditorTool : int {
    SELECT = 0, PLAYER_SPAWN, REGULUS, CAVE,
    PLATFORM, LADDER, BEAM, PATH_NODE,
    NUKE_SPAWN, BEATRICE_SPAWN, ENEMY_SPAWN,
    ELEVATOR,
    WIN_ZONE,
    KILL_ZONE,
    CONVEYOR,
    POINT_LIGHT,
    SPOT_LIGHT,
    SKY_LIGHT,
    PROP,           // ← NEW: decorative/collision prop object
    TOOL_COUNT
};
enum class GizmoMode { SELECT = 0, MOVE, ROTATE, SCALE };
enum class GizmoAxis { NONE, X, Y, FREE, RING };
enum class ConnectMode { NONE, NEXT0, NEXT1, NEXT2 };

struct SelEdge {
    int from = -1;
    int slot = -1;  // 0/1/2 = which next[] slot this edge comes from
    bool valid() const { return from >= 0 && slot >= 0; }
    void clear() { from = -1; slot = -1; }
};

// SelectedEnt is identical to EntityRef (defined in LevelData.h).
using SelectedEnt = EntityRef;

struct OutlineRow {
    SelectedEnt ent = {};
    const char* icon = "[?]";
    char        name[48] = {};
    Color       color = { 180, 185, 210, 255 };
    int         depth = 0;
    bool        hasChildren = false;
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

    void SetConveyorTextures(Texture2D* side1, Texture2D* side2, Texture2D* side3,
        Texture2D* mid1, Texture2D* mid2, Texture2D* mid3)
    {
        _convSide[0] = side1; _convSide[1] = side2; _convSide[2] = side3;
        _convM[0] = mid1;  _convM[1] = mid2;  _convM[2] = mid3;
    }

    bool WantsMenu()  const { return _wantsMenu; }
    bool WantsPlay()  const { return _wantsPlay; }
    bool WantsEmote() const { return _wantsEmote; }
    void ClearFlags() { _wantsMenu = false; _wantsPlay = false; _wantsEmote = false; }

    void SetGameTextures(Texture2D* bg, Texture2D* beam, Texture2D* ladder,
        Texture2D* player, Texture2D* regulus, Texture2D* cave,
        Texture2D* rope = nullptr, Texture2D* goldenPiston = nullptr)
    {
        _bgTex = bg; _beamTex = beam; _ladderTex = ladder;
        _playerTex = player; _regulusTex = regulus; _caveTex = cave;
        _ropeTex = rope; _goldenPistonTex = goldenPiston;
    }

    // Pass the 12 variant beam textures (Dk_FloorPart1..10, TransFloor, TransFloor2)
    void SetBeamVariantTextures(Texture2D* variants[12])
    {
        for (int i = 0; i < 12; i++) _beamVariantTex[i] = variants[i];
    }

    // Pass prop textures (variant 0 = Light.png, extend the array for more)
    // Call once after loading the textures in main.cpp.
    void SetPropTextures(Texture2D** textures, int count);

    void SetTextures(Texture2D* bg, Texture2D* beam, Texture2D* ladder)
    {
        _bgTex = bg; _beamTex = beam; _ladderTex = ladder;
    }

private:
    // ── Layout ───────────────────────────────────────────────────────────────
    int _sw = 875, _sh = 950;
    static constexpr int TOOLBAR_H = 45;
    static constexpr int BROWSER_H = 150;
    static constexpr int RIGHT_W = 210;
    static constexpr int GRID_SZ = 16;
    static constexpr int OUTLINE_ROW = 18;

    // ── Cinematic Sequencer panel ─────────────────────────────────────────────
    static constexpr int   SEQ_H = 300;
    static constexpr int   SEQ_BW = 160;
    static constexpr int   SEQ_HDR = 110;
    static constexpr int   SEQ_CTRL_H = 32;
    static constexpr float SEQ_RH = 22.f;
    static constexpr float SEQ_TH = 22.f;
    static constexpr float SEQ_AUDIO_H = 24.f;

    float _canvasH = 0.f;
    int   _canvasW = 0;
    float _zoom = 1.f;
    Camera2D _cam = {};

    // ── Level ─────────────────────────────────────────────────────────────────
    int       _levelId = 1;
    LevelData _level;

    // ── Lighting preview ─────────────────────────────────────────────────────
    LightingSystem _lighting;
    bool           _lightingPreview = false;

    std::vector<LevelData> _undoStack, _redoStack;
    static constexpr int MAX_UNDO = 60;
    void PushUndo(); void Undo(); void Redo();

    struct ClipboardEntry {
        int          type = -1;
        PlatformData plat = {};
        LadderData   lad = {};
        PathNodeData node = {};
        BeamData     beam = {};
        Vector2      pos = {};
        LightData    light = {};
        PropData     prop = {};   // ← NEW
    };
    std::vector<ClipboardEntry> _clipboard;
    void CopySelected();
    void PasteClipboard(bool grabAfter = true);
    void DuplicateSelected();

    // ── Textures ──────────────────────────────────────────────────────────────
    Texture2D* _bgTex = nullptr;
    Texture2D* _beamTex = nullptr;
    Texture2D* _beamVariantTex[12] = {};
    Texture2D* _ladderTex = nullptr;
    Texture2D* _playerTex = nullptr;
    Texture2D* _regulusTex = nullptr;
    Texture2D* _caveTex = nullptr;
    Texture2D* _ropeTex = nullptr;
    Texture2D* _goldenPistonTex = nullptr;
    Texture2D* _convSide[3] = {};
    Texture2D* _convM[3] = {};

    // Prop textures — variant 0 = Assets/Textures/Lighting/Light.png
    static constexpr int PROP_TEX_MAX = 16;
    Texture2D* _propTex[PROP_TEX_MAX] = {};
    int        _propTexCount = 0;

    // ── Tool / gizmo state ────────────────────────────────────────────────────
    EditorTool  _tool = EditorTool::SELECT;
    GizmoMode   _gizmo = GizmoMode::SELECT;
    SelectedEnt _sel;
    bool _gridOn = true, _wantsMenu = false, _wantsPlay = false, _wantsEmote = false;
    int  _gridDiv = 1;

    GizmoAxis _gizmoHot = GizmoAxis::NONE;
    GizmoAxis _gizmoDragAxis = GizmoAxis::NONE;
    bool      _gizmoDragging = false;
    Vector2   _gizmoDragStart = {};
    float     _gizmoValStart = 0.f;
    Vector2   _gizmoPosStart = {};

    enum class DirectOp { NONE, MOVE, ROTATE, SCALE };
    DirectOp _directOp = DirectOp::NONE;
    bool    _grabAxisX = false, _grabAxisY = false;
    Vector2 _grabMouseStart = {};
    Vector2 _grabSingleOrigin = {};
    float   _grabValOrigin = 0.f;
    std::vector<Vector2> _grabOrigins;
    std::vector<float>   _grabValOrigins;

    bool    _boxSelecting = false;
    Vector2 _boxStart = {}, _boxEnd = {};
    std::vector<SelectedEnt> _multiSel;

    bool    _multiDragging = false;
    Vector2 _multiDragAnchor = {};
    std::vector<Vector2> _multiDragOrigins;

    bool    _placingPlatform = false; Vector2 _platStart = {};
    bool    _placingLadder = false; Vector2 _ladStart = {};

    bool    _dragging = false;
    Vector2 _dragOffset = {};

    ConnectMode _connectMode = ConnectMode::NONE;
    int         _connectFrom = -1;
    double      _lastThreshClick = -999.0;
    SelEdge     _selEdge;

    std::vector<OutlineRow> _outline;
    int   _outlineScroll = 0;
    int   _dataPanelScroll = 0;
    float _dataPanelContentH = 0.f;

    bool    _camPanning = false;
    bool    _camPanMoved = false;
    Vector2 _camPanStart = {};
    Vector2 _camTargetAtPanStart = {};

    // Outliner parent-child interaction
    bool _outlCtxMenu = false;
    int  _outlCtxRow = -1;
    bool _outlParentPick = false;
    SelectedEnt _outlPickTarget = {};
    bool _outlDragging = false;
    int  _outlDragRow = -1;
    int  _outlDropRow = -1;

    bool    _fieldDrag = false;
    float* _fieldPtr = nullptr;
    float   _fieldDragStartX = 0.f;
    float   _fieldDragStartVal = 0.f;
    float   _fieldDragSens = 1.f;
    float   _fieldMin = 0.f, _fieldMax = 0.f;

    bool    _fieldTyping = false;
    float* _fieldTypingPtr = nullptr;
    char    _fieldTypeBuf[32] = {};
    double  _fieldLastClick = -999.0;

    char  _status[256] = {};
    float _statusTimer = 0.f;

    // ── Level range settings (game-wide, not per-level) ─────────────────────
    int          _levelRangeMin = 1;
    int          _levelRangeMax = 10;
    float        _volMusic   = 0.8f;
    float        _volSFX     = 1.f;
    float        _volAbility = 2.f;
    float        _volUI      = 1.f;
    unsigned int _highScore  = 0;
    int          _highLevels = 0;

    // ── Properties clipboard ─────────────────────────────────────────────────
    struct PropClipboard {
        int          type = -1;
        PlatformData plat = {};
        LadderData   lad = {};
        KillZoneData kz = {};
        WinZoneData  wz = {};
        ElevatorData elev = {};
        LightData    light = {};
        PropData     prop = {};   // ← NEW
    } _propClip;
    void CopyProps();
    void PasteProps();

    // ─────────────────────────────────────────────────────────────────────────
    // Cinematic Sequencer state
    // ─────────────────────────────────────────────────────────────────────────
    bool   _seqOpen = false;
    int    _activeSeq = -1;
    std::vector<CinematicSequence> _seqList;
    LevelData _seqSnapshot;

    float  _seqTime = 0.f;
    bool   _seqPlaying = false;
    float  _seqZoom = 80.f;
    float  _seqScrollX = 0.f;
    int    _seqTrackScroll = 0;
    int    _selTrack = -1;
    int    _selKey = -1;

    bool   _seqDragPlayhead = false;
    bool   _seqDragKey = false;
    float  _seqKeyDragOrig = 0.f;

    bool    _seqPanning = false;
    float   _seqPanStartX = 0.f;
    float   _seqPanStartSX = 0.f;

    float  _seqInPoint = 0.f;
    float  _seqOutPoint = -1.f;
    bool   _seqDragIn = false;
    bool   _seqDragOut = false;

    struct SeqAudio {
        char  path[256] = {};
        bool  loaded = false;
        Music music = {};
    };
    std::vector<SeqAudio> _seqAudio;

    bool _seqAudioDrop = false;

    int    _seqNameCounter = 1;

    // ── Sequencer methods ──────────────────────────────────────────────────
    void SeqLoad();
    void SeqSave();
    void SeqNew();
    void SeqDelete();
    void SeqAddKeyframe();
    void SeqDeleteKeyframe();
    void SeqPreviewUpdate(float dt);
    void SeqPreviewStop();
    void SeqEnsureAudio(int idx);
    void SeqUnloadAudio(int idx);
    void SeqLoadAudioFile(int idx, const char* path);

    void UpdateSequencer();
    void DrawSequencer();
    void DrawSeqBrowser();
    void DrawSeqTimeline();
    void DrawSeqControls();
    void DrawSeqAudioBar();

    float     SeqTimeToX(float t)  const;
    float     SeqXToTime(float px) const;
    Rectangle SeqTimelineRect() const;
    Rectangle SeqBrowserRect()  const;
    Rectangle SeqControlsRect() const;
    Rectangle SeqTrackRowRect(int row) const;
    Rectangle SeqAudioRowRect() const;

    // ── Helpers ───────────────────────────────────────────────────────────────
    Vector2   WorldMouse() const;
    Vector2   Snap(Vector2) const;
    bool InCanvas()    const;
    bool InBrowser()   const;
    bool InToolbar()   const;
    bool InRightPanel()const;
    Rectangle RightPanelRect()  const;
    Rectangle OutlinerRect()    const;
    Rectangle DataPanelRect()   const;

    Rectangle PlatRect(const PlatformData& p)                const;
    Rectangle LadRect(const LadderData& l)                   const;
    Rectangle BeamRect(const BeamData& b)                    const;
    bool      PointInPlatform(Vector2 pt, const PlatformData& p) const;
    Vector2   PlatformCenter(const PlatformData& p)          const;
    Vector2   EntityCenter(const SelectedEnt& e)             const;

    Vector2 GetEntPos(const SelectedEnt& e)  const;
    void    SetEntPos(const SelectedEnt& e, Vector2 p);

    bool PickEntity(Vector2 p);
    bool IsInMultiSel(const SelectedEnt& e) const;
    void BoxSelectEntities(Rectangle box);
    void DeleteSelected(); void DeleteMultiSelected();
    void SelectEnt(SelectedEnt e);
    void SelectAllOfSameType();
    void SyncPropertiesFromPrimary();

    Vector2 GetSelPos() const { return GetEntPos(_sel); }
    void    SetSelPos(Vector2 p) { SetEntPos(_sel, p); }

    void StartDirectOp(DirectOp op);
    void UpdateDirectOp();
    void ConfirmDirectOp();
    void CancelDirectOp();

    void      UpdateGizmo();
    void      DrawGizmo() const;
    GizmoAxis GizmoHitTest(Vector2 center, Vector2 wm) const;
    static constexpr float GIZMO_R = 60.f;

    void BuildOutline();

    void UpdateToolbar(); void UpdateBrowser();
    void UpdateCanvas();  void UpdateRightPanel();

    void DrawBackground()        const;
    void DrawGrid()              const;
    void DrawLevelEntities();
    void DrawLevelLitContent();
    void DrawLevelOverlays();
    void DrawPlacementPreview()  const;
    void DrawToolbarUI()         const;
    void DrawBrowserUI();
    void DrawOutliner();
    void DrawDataPanel();

    void DrawPlatEnt(const PlatformData& p, bool sel, bool msel) const;
    void DrawLadEnt(const LadderData& l, bool sel, bool msel) const;
    void DrawCircEnt(Vector2 pos, float r, Color c, bool sel, bool msel, const char* lbl) const;
    void DrawBeamEnt(const BeamData& b, bool sel, bool msel)      const;
    void DrawPathNodes();
    void DrawPlayerSpawn(Vector2 pos, bool sel, bool msel) const;
    void DrawRegulusEnt(Vector2 pos, bool sel, bool msel)  const;
    void DrawCaveEnt(Vector2 pos, bool sel, bool msel)  const;

    // ── Prop draw/pick helpers ─────────────────────────────────────────────
    // DrawPropEnt  — draws the prop in the editor canvas (textured rect or
    //                a tinted placeholder if texture isn't loaded yet).
    // PropRect     — returns an axis-aligned bounding box for hit-testing
    //                (NOTE: for rotated props this is approximate; exact pick
    //                 uses PointInProp instead).
    // PointInProp  — precise OBB point-in-rect test accounting for rotation.
    void DrawPropEnt(const PropData& pr, bool sel, bool msel) const;
    Rectangle PropRect(const PropData& pr) const;
    bool      PointInProp(Vector2 pt, const PropData& pr) const;

    bool NumField(const char* label, float& val, float sens,
        float minV, float maxV, float x, float y, float fw);

    // Parent-child helpers
    SelectedEnt GetParent(SelectedEnt e)              const;
    std::vector<SelectedEnt> GetChildren(SelectedEnt e) const;
    void SetRelationParent(SelectedEnt child, SelectedEnt parent);
    void RemoveRelation(SelectedEnt child);
    void DeleteRelationsFor(SelectedEnt e);
    bool IsAncestor(SelectedEnt anc, SelectedEnt e) const;
    void BuildOutlineTree(SelectedEnt e, int depth);

    void DrawElevatorEnt(const ElevatorData& el, bool sel, bool msel) const;
    Rectangle ElevRect(const ElevatorData& el) const;

    void DrawWinZoneEnt(const WinZoneData& wz, bool sel, bool msel) const;
    Rectangle WinZoneRect(const WinZoneData& wz) const;
    void DrawKillZoneEnt(const KillZoneData& kz, bool sel, bool msel) const;
    Rectangle KillZoneRect(const KillZoneData& kz) const;

    void DrawConveyorEnt(const ConveyorData& cv, bool sel, bool msel) const;
    Rectangle ConveyorRect(const ConveyorData& cv) const;

    void DrawLightEnt(const LightData& L, int idx, bool sel, bool msel) const;

    Rectangle  BrowserBtn(int row, int col, int cols) const;
    static const char* ToolName(EditorTool t);
    static Color       ToolColor(EditorTool t);
    void SetStatus(const char* msg, float dur = 2.5f);
};