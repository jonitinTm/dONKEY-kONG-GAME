#pragma once
// ============================================================
//  Lighting.h
//  2D raytraced lighting system for raylib.
//
//  Approach (per-pixel software raytracing in a fragment shader):
//    1. You render your normal scene into our offscreen "scene" target.
//    2. You render light-blocking silhouettes into our "occluder" target.
//    3. We run a fragment shader that, for every screen pixel, casts a
//       ray through the occluder mask towards every light to determine
//       direct visibility, accumulates volumetric in-scattering along
//       the ray for fog, and applies skylight as a directional ray.
//    4. A second blur pass simulates a single GI bounce.
//    5. The lit result is drawn to the active framebuffer.
//
//  This is NOT GPU hardware raytracing (raylib has no DXR/RTX path).
//  Visually it gives you crisp shadows, soft falloff, spot cones,
//  god rays through gaps, and color bleed — the "UE-ish" 2D look.
// ============================================================
#include "raylib.h"
#include "LevelData.h"

class LightingSystem
{
public:
    // Init with the screen size you render at (matches main.cpp's screenWidth/Height).
    void Init(int screenW, int screenH);
    void Shutdown();

    // Set how dark the world is when no light reaches a pixel (0 = pitch black, 1 = unlit).
    // Default 0.08 — almost black, dramatic. Bump to 0.25 for "evening" instead of "night".
    void SetGlobalAmbient(float a) { _globalAmbient = a; }

    // How aggressively the unlit areas are darkened (0 = lights add only / no darkening,
    // 1 = scene starts pitch black and lights reveal). Default 0.85.
    void SetGlobalDarkness(float d) { _globalDarkness = d; }

    // Optional: tint the global ambient (sky color, distant haze). Default warmish dark blue.
    void SetAmbientColor(Color c) { _ambientColor = c; }

    // ── Frame flow ──────────────────────────────────────────────────────────
    // Wrap everything that should be lit:
    //     lighting.BeginScene(cam);
    //         <... your normal BeginMode2D draws: bg, platforms, beams, player, etc ...>
    //     lighting.EndScene();
    //
    // Wrap the silhouettes that block light (platforms, beams, kill zones).
    // Draw them in solid white — the pixel value's red channel is the occlusion mask.
    //     lighting.BeginOccluders(cam);
    //         <... draw filled rects of every light-blocking object in WHITE ...>
    //     lighting.EndOccluders();
    //
    // Either order works. After both are populated, call Composite once:
    //     lighting.Composite(level, cam);   // draws final lit image to current target

    void BeginScene(Camera2D cam);
    void EndScene();

    void BeginOccluders(Camera2D cam);
    void EndOccluders();

    // Convenience: draws the standard occluders for you (platforms, beams, kill zones).
    // If you want full control, skip this and use Begin/EndOccluders directly.
    void BakeOccludersFromLevel(const LevelData& lv, Camera2D cam);

    // Final pass: applies lighting and draws to whatever target is currently active.
    // Call this between BeginDrawing() and EndDrawing(), but OUTSIDE any BeginMode2D.
    void Composite(const LevelData& lv, Camera2D cam);

    // Optional debug: draws the occluder mask in the corner so you can see what's
    // being treated as light-blocker.
    void DebugDrawOccluder(Rectangle dst);
    // Draws the raw scene texture (no lighting) in dst — useful for A/B comparison.
    void DebugDrawScene(Rectangle dst);

private:
    int  _w = 0, _h = 0;
    bool _ready = false;

    RenderTexture2D _sceneRT = {};
    RenderTexture2D _occluderRT = {};
    RenderTexture2D _directLitRT = {};   // direct lighting result (input to bounce)
    RenderTexture2D _bounceRT = {};      // blurred direct → 1-bounce approximation
    RenderTexture2D _scratchRT = {};     // separable-blur scratch / general scratch

    Shader _lightShader = {};            // raytracing pass
    Shader _blurShader = {};             // gaussian blur for bounce / softening

    // Uniform locations cached for speed
    int _locLightCount = -1;
    int _locLightPos = -1;
    int _locLightCol = -1;
    int _locLightExtra = -1;
    int _locResolution = -1;
    int _locCamOffset = -1;
    int _locAmbient = -1;
    int _locAmbientCol = -1;
    int _locDarkness = -1;
    int _locTime = -1;
    int _locOccluderTex = -1;
    int _locBounceTex = -1;
    int _locUseBounce = -1;

    int _locBlurDir = -1;
    int _locBlurRes = -1;

    float _globalAmbient = 0.08f;
    float _globalDarkness = 0.85f;
    Color _ambientColor = { 40, 50, 80, 255 };  // dark cool tint
    float _time = 0.f;

    void RunBlur(RenderTexture2D src, RenderTexture2D dst, Vector2 dir);
    void UploadLights(const LevelData& lv);
};
