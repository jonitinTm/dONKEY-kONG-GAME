#pragma once
// ============================================================
//  Lighting.h
//  2D raytraced lighting system for raylib (optimised).
//
//  Architecture
//  ────────────
//  • Scene is rendered at FULL resolution into _sceneRT.
//  • Lighting math runs at HALF resolution into _directLitRT
//    (4× fewer pixel-shader invocations than full-res).
//  • Optional bounce: blur(_directLitRT) → _bounceRT, then a
//    second lighting pass adds bounce.  Result: _finalLightRT.
//  • Compose pass runs at FULL res: scene * lighting (bilinear
//    upsampled) + ambient floor.
//
//  This is software per-pixel raytracing in a fragment shader,
//  not GPU hardware RT (raylib has no DXR/RTX path). Visually:
//  crisp shadows, spot cones, god rays, 1-bounce GI.
//
//  Two-instance pattern: instantiate one LightingSystem for
//  gameplay (sized to screen) and another for the editor canvas
//  (sized to the editor's canvas region) — they're independent.
// ============================================================
#include "raylib.h"
#include "LevelData.h"

class LightingSystem
{
public:
    enum class Quality { LOW = 0, MEDIUM = 1, HIGH = 2 };

    LightingSystem() = default;
    ~LightingSystem();
    LightingSystem(const LightingSystem&) = delete;
    LightingSystem& operator=(const LightingSystem&) = delete;

    // rtW/rtH: dimensions of the area to be lit (full resolution of scene).
    // For gameplay: pass screenWidth/screenHeight.
    // For editor:   pass _canvasW / (int)_canvasH.
    void Init(int rtW, int rtH, Quality q = Quality::MEDIUM);
    void Shutdown();

    // Tweakables — safe to call any time.
    void  SetGlobalAmbient(float a) { _globalAmbient = a; }
    void  SetGlobalDarkness(float d) { _globalDarkness = d; }
    void  SetAmbientColor(Color c) { _ambientColor = c; }
    Quality GetQuality() const { return _quality; }

    // ── Frame flow ──────────────────────────────────────────────────────────
    void BeginScene(Camera2D cam);
    void EndScene();

    void BeginOccluders(Camera2D cam);
    void EndOccluders();
    void BakeOccludersFromLevel(const LevelData& lv, Camera2D cam);

    // dst.width <= 0 means "full RT size at (0,0)"
    void Composite(const LevelData& lv, Camera2D cam, Rectangle dst = { -1.f, -1.f, 0.f, 0.f });

    // Debug
    void DebugDrawOccluder(Rectangle dst);
    void DebugDrawScene(Rectangle dst);

private:
    bool    _ready = false;
    Quality _quality = Quality::MEDIUM;

    int _rtW = 0, _rtH = 0;     // scene RT dims (full canvas/screen)
    int _liW = 0, _liH = 0;     // lighting RT dims (half-res by default)

    // Render targets
    RenderTexture2D _sceneRT = {};
    RenderTexture2D _occluderRT = {};
    RenderTexture2D _directLitRT = {};
    RenderTexture2D _scratchRT = {};
    RenderTexture2D _bounceRT = {};
    RenderTexture2D _finalLightRT = {};

    // Shaders
    Shader _lightShader = {};
    Shader _blurShader = {};
    Shader _composeShader = {};

    // Cached uniform locations
    int _locLP_count = -1, _locLP_pos = -1, _locLP_col = -1, _locLP_extra = -1;
    int _locLP_resolution = -1, _locLP_worldOrigin = -1, _locLP_worldPerPx = -1;
    int _locLP_ambient = -1, _locLP_ambientCol = -1;
    int _locLP_time = -1, _locLP_useBounce = -1;
    int _locLP_occTex = -1, _locLP_bounceTex = -1;

    int _locBL_dir = -1;

    int _locCM_lightTex = -1, _locCM_ambient = -1, _locCM_ambientCol = -1, _locCM_darkness = -1;

    float _globalAmbient = 0.08f;
    float _globalDarkness = 0.85f;
    Color _ambientColor = { 40, 50, 80, 255 };
    float _time = 0.f;

    void RunBlur(RenderTexture2D src, RenderTexture2D dst, Vector2 dir);
    void UploadLights(const LevelData& lv);
    void DoLightingPass(Camera2D cam, RenderTexture2D dst, bool useBounce);
};