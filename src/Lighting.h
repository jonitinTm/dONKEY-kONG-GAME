#pragma once
// ============================================================
//  Lighting.h
//  2D raytraced lighting system for raylib (optimised).
//
//  v7 changes vs v6:
//    • Per-light flicker + pulse animation now fully GPU-driven via uTime.
//      flickerFreq/Amp and pulseFreq/Amp in LightData are uploaded every
//      frame and applied inside the shader — zero CPU cost.
//    • AddRuntimeLight() / ClearRuntimeLights() API lets game code inject
//      ephemeral lights (player torch, explosion flash, etc.) each frame
//      without touching the serialised LevelData.
//    • Wider GI bounce blur (4× vs old 1-pixel) for real indirect bleed.
//    • Removed dead _composeShader / _locCM_* declarations that were never
//      loaded.  _locCM_darkness renamed _locLP_darkness (same shader, clearer).
// ============================================================
#include "raylib.h"
#include "LevelData.h"
#include <vector>

class LightingSystem
{
public:
    enum class Quality { LOW = 0, MEDIUM = 1, HIGH = 2 };

    LightingSystem() = default;
    ~LightingSystem();
    LightingSystem(const LightingSystem&) = delete;
    LightingSystem& operator=(const LightingSystem&) = delete;

    void Init(int rtW, int rtH, Quality q = Quality::MEDIUM);
    void Shutdown();

    void    SetGlobalAmbient(float a) { _globalAmbient = a; }
    void    SetGlobalDarkness(float d) { _globalDarkness = d; }
    void    SetAmbientColor(Color c) { _ambientColor = c; }
    void    SetBloom(bool en, float threshold = 0.7f, float intensity = 0.8f)
                { _bloomEnabled = en; _bloomThreshold = threshold; _bloomIntensity = intensity; }
    Quality GetQuality() const { return _quality; }

    // ── Runtime (per-frame) light API ─────────────────────────────────────────
    // Call AddRuntimeLight() for each ephemeral light (player torch, projectile
    // flash, explosion, etc.) every frame BEFORE Composite().
    // Composite() automatically clears the list after rendering so stale entries
    // never accumulate across frames.
    void AddRuntimeLight(const LightData& L) { _runtimeLights.push_back(L); }
    void ClearRuntimeLights() { _runtimeLights.clear(); }

    void BeginScene(Camera2D cam);
    void EndScene();

    void BeginOccluders(Camera2D cam);
    void EndOccluders();
    void BakeOccludersFromLevel(const LevelData& lv, Camera2D cam);

    // Composite() reads lv.lights (level-authored) PLUS any runtime lights added
    // this frame via AddRuntimeLight().  Runtime list is cleared automatically.
    void Composite(const LevelData& lv, Camera2D cam,
        Rectangle dst = { -1.f, -1.f, 0.f, 0.f });

    void DebugDrawOccluder(Rectangle dst);
    void DebugDrawScene(Rectangle dst);

private:
    bool    _ready = false;
    Quality _quality = Quality::MEDIUM;

    int _rtW = 0, _rtH = 0;
    int _liW = 0, _liH = 0;

    RenderTexture2D _sceneRT = {};
    RenderTexture2D _occluderRT = {};
    RenderTexture2D _directLitRT = {};
    RenderTexture2D _scratchRT = {};
    RenderTexture2D _bounceRT = {};
    RenderTexture2D _finalLightRT = {};

    Shader _lightShader = {};
    Shader _blurShader = {};

    // Light-pass uniform locations
    int _locLP_count = -1;
    int _locLP_pos = -1;
    int _locLP_col = -1;
    int _locLP_extra = -1;
    int _locLP_anim = -1;  // NEW: (flickerFreq, flickerAmp, pulseFreq, pulseAmp)
    int _locLP_resolution = -1;
    int _locLP_worldOrigin = -1;
    int _locLP_worldPerPx = -1;
    int _locLP_ambient = -1;
    int _locLP_ambientCol = -1;
    int _locLP_time = -1;
    int _locLP_useBounce = -1;
    int _locLP_occTex = -1;
    int _locLP_bounceTex = -1;
    int _locLP_falloff = -1;
    int _locLP_applyFloor = -1;
    int _locLP_darkness = -1;  // was _locCM_darkness — same shader, clearer name

    // Blur-pass uniform location
    int _locBL_dir = -1;

    float _globalAmbient = 0.08f;
    float _globalDarkness = 0.85f;
    Color _ambientColor = { 40, 50, 80, 255 };
    float _time = 0.f;

    // Per-frame ephemeral lights (auto-cleared after each Composite call)
    std::vector<LightData> _runtimeLights;

    void RunBlur(RenderTexture2D src, RenderTexture2D dst, Vector2 dir);
    void UploadLights(const LevelData& lv);

    bool    _bloomEnabled   = false;
    float   _bloomThreshold = 0.7f;
    float   _bloomIntensity = 0.8f;
    RenderTexture2D _compositeRT = {};
    RenderTexture2D _bloomRT     = {};
    Shader  _bloomExtractShader  = {};
    int     _locBE_threshold     = -1;
    int     _locBE_intensity     = -1;

    // isFinalPass=false  raw direct light only, no ambient/floor (used as bounce source)
    // isFinalPass=true   full output: direct + bounce + ambient + floor clamp
    void DoLightingPass(Camera2D cam, RenderTexture2D dst,
        bool useBounce, bool isFinalPass);
};