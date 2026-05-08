// ============================================================
//  Lighting.cpp  (v7 — dynamic lights, flicker/pulse shader animation,
//                      runtime-light API, wider GI bounce blur)
//
//  Pipeline:
//   1. Scene       → _sceneRT      (full res)
//   2. Occluders   → _occluderRT   (half res, white = blocker)
//      Rebuilt every frame so moving geometry casts correct shadows.
//      NOTE: elevators are intentionally omitted (no collision geometry).
//   3. Lighting    → _finalLightRT (half res)
//        • Bounce path:
//            a. Raw direct pass → _directLitRT  (no ambient, no floor)
//            b. Wide blur (4 px) → _bounceRT
//            c. Final pass (direct + bounce, floor applied)
//        • Single pass: direct + floor
//   4. Tight smooth blur on _finalLightRT (feathers half-res edges)
//   5. Composite: scene × _finalLightRT (BLEND_MULTIPLIED, bilinear upscale)
//
//  v7 fixes & improvements over v6:
//    BUG – Flicker/pulse were serialised in LightData but never uploaded to
//          the GPU.  The shader now receives a lightAnim[MAX_LIGHTS] uniform
//          (flickerFreq, flickerAmp, pulseFreq, pulseAmp) and animates each
//          light's intensity every frame using uTime — zero CPU cost.
//    BUG – GI bounce blur step was 1/liW ≈ 1 half-res pixel — far too narrow
//          for any visible indirect bleed.  Now 4.0/liW for real spread.
//    BUG – _locCM_darkness was looked up against _lightShader but stored in
//          the compose-shader variable slot (which was never created).  Fixed:
//          the location is now _locLP_darkness and used correctly.
//    NEW – AddRuntimeLight() / ClearRuntimeLights() API for per-frame ephemeral
//          lights (player torch, explosion flash, etc.).  Runtime list is merged
//          with lv.lights inside UploadLights() and auto-cleared each frame.
// ============================================================
#include "Lighting.h"
#include <cmath>
#include <cstring>
#include <cstdio>

static constexpr int MAX_LIGHTS = 32;

// ─────────────────────────────────────────────────────────────────────────────
//  LIGHTING FRAGMENT SHADER
//
//  Animation model (GPU-side, zero CPU cost):
//
//    lightAnim[i] = (flickerFreq, flickerAmp, pulseFreq, pulseAmp)
//
//    Pulse  : smooth sine-wave breath.
//             intensity *= 1 + pulseAmp * sin(uTime * pulseFreq * 2π)
//
//    Flicker: hash-based pseudo-random step function that changes at flickerFreq Hz.
//             Each light gets a unique hash seed so they don't all snap together.
//             intensity *= 1 - flickerAmp * hash(floor(uTime * flickerFreq), i)
//
//    Both effects stack.  Either can be zero (disabled).
// ─────────────────────────────────────────────────────────────────────────────
static const char* LIGHTING_FS = R"GLSL(
#version 330

in  vec2 fragTexCoord;
in  vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;
uniform sampler2D occluderTex;
uniform sampler2D bounceTex;

uniform vec2  resolution;
uniform vec2  worldOrigin;
uniform vec2  worldPerPx;
uniform float globalAmbient;
uniform float globalDarkness;
uniform vec3  ambientColor;
uniform float uTime;
uniform int   useBounce;
uniform int   applyFloor;

#define MAX_LIGHTS 32
uniform int  lightCount;
uniform vec4 lightPos   [MAX_LIGHTS];  // xy=pos, z=outerRadius, w=type(0/1/2)
uniform vec4 lightCol   [MAX_LIGHTS];  // rgb=color(linear), a=intensity
uniform vec4 lightExtra [MAX_LIGHTS];  // x=direction(rad), y=halfAngle(rad), z=fogStr, w=bounces
uniform vec4 lightFalloff[MAX_LIGHTS]; // x=innerRadius, yzw=unused
uniform vec4 lightAnim  [MAX_LIGHTS];  // x=flickerFreq, y=flickerAmp, z=pulseFreq, w=pulseAmp

// ── Helpers ───────────────────────────────────────────────────────────────────

float sampleOccluder(vec2 worldPos) {
    vec2 screen = (worldPos - worldOrigin) / worldPerPx;
    vec2 uv     = screen / resolution;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return texture(occluderTex, vec2(uv.x, 1.0 - uv.y)).r;
}

// Full-quality 16-step ray for direct shadows
float visibilityHard(vec2 from, vec2 to) {
    const int STEPS = 16;
    vec2 d = (to - from) / float(STEPS);
    vec2 p = from + d * 1.5;
    for (int i = 2; i < STEPS - 1; i++) {
        if (sampleOccluder(p) > 0.5) return 0.0;
        p += d;
    }
    return 1.0;
}

// Quick 8-step ray for fog shaft visibility
float visibilityQuick(vec2 from, vec2 to) {
    const int STEPS = 8;
    vec2 d = (to - from) / float(STEPS);
    vec2 p = from + d * 1.5;
    for (int i = 1; i < STEPS - 1; i++) {
        if (sampleOccluder(p) > 0.5) return 0.0;
        p += d;
    }
    return 1.0;
}

// PCF 3-tap soft shadows
float visibilitySoft(vec2 from, vec2 to) {
    vec2  toDir = to - from;
    float len   = length(toDir);
    if (len < 0.001) return 1.0;
    vec2  dir   = toDir / len;
    vec2  perp  = vec2(-dir.y, dir.x);
    float jitter = len * 0.012;
    float v0 = visibilityHard(from, to);
    float v1 = visibilityHard(from + perp * jitter, to + perp * jitter);
    float v2 = visibilityHard(from - perp * jitter, to - perp * jitter);
    return (v0 * 0.5 + v1 * 0.25 + v2 * 0.25);
}

// UE4-style smooth falloff: max(0, 1 - t^4)^2
// Stays near full intensity through the inner zone, smooth curve to 0 at radius.
// No harsh cutoff, preserves light color at the center rather than saturating white.
float dualRadiusAtten(float dist, float innerR, float outerR) {
    if (dist >= outerR) return 0.0;
    if (innerR > 0.001 && dist <= innerR) return 1.0;
    float effectiveDist   = max(dist - innerR, 0.0);
    float effectiveRadius = max(outerR - innerR, 0.0001);
    float t = effectiveDist / effectiveRadius;
    float f = clamp(1.0 - t * t * t * t, 0.0, 1.0);
    return f * f;
}

// ── Per-light animation multiplier (GPU-driven, zero CPU cost) ────────────────
// Returns a scalar in roughly [0, 2] that modulates the light's intensity.
float animIntensity(int i) {
    float flickFreq = lightAnim[i].x;
    float flickAmp  = lightAnim[i].y;
    float pulseFreq = lightAnim[i].z;
    float pulseAmp  = lightAnim[i].w;

    float mul = 1.0;

    // Pulse: smooth sine-wave breath
    if (pulseFreq > 0.001 && pulseAmp > 0.001) {
        mul += pulseAmp * sin(uTime * pulseFreq * 6.28318);
    }

    // Flicker: pseudo-random step, unique per light index
    if (flickFreq > 0.001 && flickAmp > 0.001) {
        float t     = floor(uTime * flickFreq);
        float seed  = t + float(i) * 127.1 + float(i * i) * 311.7;
        float noise = fract(sin(seed) * 43758.5453);
        mul *= 1.0 - flickAmp * noise;
    }

    return max(mul, 0.0);
}

// ── Per-light contribution ────────────────────────────────────────────────────
vec3 lightContribution(int i, vec2 pixelPos) {
    int type = int(lightPos[i].w + 0.5);

    float anim = animIntensity(i);

    // Skylight — directional ambient, traces toward sky to test occlusion
    if (type == 2) {
        float ang    = lightExtra[i].x;
        vec2  toSky  = vec2(cos(ang), sin(ang));
        float maxDist = lightPos[i].z;
        const int SKY_STEPS = 12;
        vec2 d = toSky * (maxDist / float(SKY_STEPS));
        vec2 p = pixelPos + d * 1.5;
        for (int s = 0; s < SKY_STEPS; s++) {
            if (sampleOccluder(p) > 0.5) return vec3(0.0);
            p += d;
        }
        return lightCol[i].rgb * (lightCol[i].a * anim);
    }

    vec2  lp     = lightPos[i].xy;
    float outerR = lightPos[i].z;
    float innerR = lightFalloff[i].x;
    vec2  toL    = lp - pixelPos;
    float dist   = length(toL);

    if (dist > outerR * 1.2) return vec3(0.0);

    bool  isSpot     = (type == 1);
    vec2  spotDir    = vec2(0.0);
    float halfAng    = 0.0;
    float coneFactor = 1.0;
    if (isSpot) {
        spotDir = vec2(cos(lightExtra[i].x), sin(lightExtra[i].x));
        halfAng = lightExtra[i].y;
        vec2  toLN   = toL / max(dist, 0.0001);
        float cosA   = dot(-toLN, spotDir);
        float coneEdge = cos(halfAng);
        if (cosA < coneEdge) return vec3(0.0);
        coneFactor = smoothstep(coneEdge, mix(coneEdge, 1.0, 0.25), cosA);
    }

    // Direct illumination
    vec3 direct = vec3(0.0);
    if (dist <= outerR) {
        float vis = visibilitySoft(pixelPos, lp);
        if (vis > 0.001) {
            float atten = dualRadiusAtten(dist, innerR, outerR);
            direct = lightCol[i].rgb * (lightCol[i].a * anim) * atten * vis * coneFactor;
        }
    }

    // Volumetric god-ray fog
    float fogStr = lightExtra[i].z;
    vec3  fog    = vec3(0.0);
    if (fogStr > 0.001) {
        const int FOG_STEPS = 16;
        vec2 d     = toL / float(FOG_STEPS);
        vec2 p     = pixelPos + d * 0.5;
        vec3 accum = vec3(0.0);
        for (int s = 0; s < FOG_STEPS; s++) {
            if (sampleOccluder(p) < 0.5) {
                float shaftVis = visibilityQuick(lp, p);
                if (shaftVis > 0.0) {
                    float dl  = length(lp - p);
                    float at  = dualRadiusAtten(dl, innerR, outerR);
                    float cone = 1.0;
                    if (isSpot) {
                        vec2  fromL = (p - lp) / max(dl, 0.0001);
                        float c     = dot(fromL, spotDir);
                        cone = smoothstep(cos(halfAng) - 0.05, cos(halfAng), c);
                    }
                    accum += lightCol[i].rgb * (lightCol[i].a * anim) * at * cone * shaftVis;
                }
            }
            p += d;
        }
        fog = accum * fogStr / float(FOG_STEPS);
    }

    return direct + fog;
}

// ── Main ──────────────────────────────────────────────────────────────────────
void main() {
    vec2 screenPx  = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * resolution;
    vec2 pixelPos  = screenPx * worldPerPx + worldOrigin;

    // Bounce-source pass: pure direct light (no ambient)
    vec3 lighting = (applyFloor == 1) ? ambientColor * globalAmbient : vec3(0.0);

    int n = min(lightCount, MAX_LIGHTS);
    for (int i = 0; i < n; i++) {
        if (lightCol[i].a < 0.001) continue;  // skip disabled / zero-intensity lights
        lighting += lightContribution(i, pixelPos);
    }

    if (useBounce == 1) {
        vec3 b = texture(bounceTex, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)).rgb;
        // Add bounce at half weight — no Reinhard here either, it desaturates the GI tint.
        lighting += b * 0.5;
    }

    if (applyFloor == 1) {
        // Dark floor: ensures unlit areas are never completely black,
        // tinted by the ambient color. Controlled by globalDarkness (0=bright,1=dark).
        float floorVal = mix(1.0, globalAmbient, globalDarkness);
        lighting = max(lighting, ambientColor * floorVal);

        // Simple clamp — no tone-map.  A Reinhard curve here desaturates colors
        // toward white at the center of bright lights (the "whitewash" bug).
        // Lights that sum above 1.0 just saturate cleanly at white via the RT.
        lighting = clamp(lighting, 0.0, 1.0);
    }

    finalColor = vec4(lighting, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  BLUR FRAGMENT SHADER — 9-tap Gaussian
// ─────────────────────────────────────────────────────────────────────────────
static const char* BLUR_FS = R"GLSL(
#version 330
in  vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 blurDir;

void main() {
    vec2 step = blurDir * 1.5;
    vec3 c = vec3(0.0);
    c += texture(texture0, fragTexCoord - step * 4.0).rgb * 0.05;
    c += texture(texture0, fragTexCoord - step * 3.0).rgb * 0.09;
    c += texture(texture0, fragTexCoord - step * 2.0).rgb * 0.12;
    c += texture(texture0, fragTexCoord - step       ).rgb * 0.15;
    c += texture(texture0, fragTexCoord              ).rgb * 0.18;
    c += texture(texture0, fragTexCoord + step       ).rgb * 0.15;
    c += texture(texture0, fragTexCoord + step * 2.0).rgb * 0.12;
    c += texture(texture0, fragTexCoord + step * 3.0).rgb * 0.09;
    c += texture(texture0, fragTexCoord + step * 4.0).rgb * 0.05;
    finalColor = vec4(c, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  LightingSystem implementation
// ─────────────────────────────────────────────────────────────────────────────

LightingSystem::~LightingSystem() { Shutdown(); }

void LightingSystem::Init(int rtW, int rtH, Quality q)
{
    if (_ready) Shutdown();

    _rtW = rtW; _rtH = rtH;
    _liW = (rtW + 1) / 2;
    _liH = (rtH + 1) / 2;
    _quality = q;

    _sceneRT = LoadRenderTexture(_rtW, _rtH);
    _occluderRT = LoadRenderTexture(_liW, _liH);
    _directLitRT = LoadRenderTexture(_liW, _liH);
    _scratchRT = LoadRenderTexture(_liW, _liH);
    _bounceRT = LoadRenderTexture(_liW, _liH);
    _finalLightRT = LoadRenderTexture(_liW, _liH);

    SetTextureFilter(_sceneRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_occluderRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_directLitRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_scratchRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_bounceRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_finalLightRT.texture, TEXTURE_FILTER_BILINEAR);

    _lightShader = LoadShaderFromMemory(nullptr, LIGHTING_FS);
    _blurShader = LoadShaderFromMemory(nullptr, BLUR_FS);

    // Light shader uniforms
    _locLP_count = GetShaderLocation(_lightShader, "lightCount");
    _locLP_pos = GetShaderLocation(_lightShader, "lightPos");
    _locLP_col = GetShaderLocation(_lightShader, "lightCol");
    _locLP_extra = GetShaderLocation(_lightShader, "lightExtra");
    _locLP_anim = GetShaderLocation(_lightShader, "lightAnim");       // NEW
    _locLP_resolution = GetShaderLocation(_lightShader, "resolution");
    _locLP_worldOrigin = GetShaderLocation(_lightShader, "worldOrigin");
    _locLP_worldPerPx = GetShaderLocation(_lightShader, "worldPerPx");
    _locLP_ambient = GetShaderLocation(_lightShader, "globalAmbient");
    _locLP_ambientCol = GetShaderLocation(_lightShader, "ambientColor");
    _locLP_time = GetShaderLocation(_lightShader, "uTime");
    _locLP_useBounce = GetShaderLocation(_lightShader, "useBounce");
    _locLP_occTex = GetShaderLocation(_lightShader, "occluderTex");
    _locLP_bounceTex = GetShaderLocation(_lightShader, "bounceTex");
    _locLP_falloff = GetShaderLocation(_lightShader, "lightFalloff");
    _locLP_applyFloor = GetShaderLocation(_lightShader, "applyFloor");
    _locLP_darkness = GetShaderLocation(_lightShader, "globalDarkness");  // FIX: was _locCM_darkness

    // Blur shader uniforms
    _locBL_dir = GetShaderLocation(_blurShader, "blurDir");

    printf("[Lighting v7] lightShader.id=%u  blurShader.id=%u  rtW=%d rtH=%d  liW=%d liH=%d\n",
        _lightShader.id, _blurShader.id, _rtW, _rtH, _liW, _liH);
    printf("[Lighting v7] uniforms: count=%d pos=%d col=%d extra=%d anim=%d res=%d "
        "origin=%d wpp=%d amb=%d ambCol=%d time=%d useBnc=%d occ=%d bnc=%d "
        "dark=%d applyFloor=%d\n",
        _locLP_count, _locLP_pos, _locLP_col, _locLP_extra, _locLP_anim,
        _locLP_resolution, _locLP_worldOrigin, _locLP_worldPerPx,
        _locLP_ambient, _locLP_ambientCol, _locLP_time, _locLP_useBounce,
        _locLP_occTex, _locLP_bounceTex, _locLP_darkness, _locLP_applyFloor);

    _ready = true;
}

void LightingSystem::Shutdown()
{
    if (!_ready) return;
    UnloadRenderTexture(_sceneRT);
    UnloadRenderTexture(_occluderRT);
    UnloadRenderTexture(_directLitRT);
    UnloadRenderTexture(_scratchRT);
    UnloadRenderTexture(_bounceRT);
    UnloadRenderTexture(_finalLightRT);
    UnloadShader(_lightShader);
    UnloadShader(_blurShader);
    _runtimeLights.clear();
    _ready = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Scene capture
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::BeginScene(Camera2D cam)
{
    BeginTextureMode(_sceneRT);
    ClearBackground(BLACK);
    BeginMode2D(cam);
}
void LightingSystem::EndScene()
{
    EndMode2D();
    EndTextureMode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Occluder capture
//  FIX: Only halve the zoom for the half-res RT.  The offset must also be
//  halved since it is in screen-pixels and the RT is half the size.
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::BeginOccluders(Camera2D cam)
{
    BeginTextureMode(_occluderRT);
    ClearBackground(BLACK);

    Camera2D halfCam = cam;
    halfCam.zoom *= 0.5f;
    halfCam.offset.x *= 0.5f;
    halfCam.offset.y *= 0.5f;
    BeginMode2D(halfCam);
}
void LightingSystem::EndOccluders()
{
    EndMode2D();
    EndTextureMode();
}

// BakeOccludersFromLevel: called every frame so that moving geometry (platforms,
// conveyors, kill zones) always casts the correct shadows — fully dynamic.
void LightingSystem::BakeOccludersFromLevel(const LevelData& lv, Camera2D cam)
{
    BeginOccluders(cam);

    for (const auto& p : lv.platforms) {
        float h = (p.h <= 0.f) ? 8.f : p.h;
        if (fabsf(p.tilt) > 0.05f) {
            // AABB of tilted platform — conservative over-approximation for shadow purposes.
            float tr = p.tilt * (3.14159265f / 180.f);
            float yr = p.w * tanf(tr);
            float yMin = fminf(0.f, yr);
            float yMax = fmaxf(0.f, yr) + h;
            DrawRectangleRec({ p.x, p.y + yMin, p.w, yMax - yMin }, WHITE);
        }
        else {
            DrawRectangleRec({ p.x, p.y, p.w, h }, WHITE);
        }
    }

    // Beams rendered at scale=4 in the editor; match that footprint here.
    static constexpr float BEAM_SCALE = 4.f;
    static constexpr float BEAM_SPRITE_W = 16.f;
    static constexpr float BEAM_SPRITE_H = 16.f;
    for (const auto& b : lv.beams) {
        DrawRectangleRec({ b.x, b.y,
                           BEAM_SPRITE_W * BEAM_SCALE,
                           BEAM_SPRITE_H * BEAM_SCALE }, WHITE);
    }

    for (const auto& kz : lv.killZones) {
        if (kz.texId == KillZoneTexture::NONE) continue;
        DrawRectanglePro(
            { kz.x + kz.w * 0.5f, kz.y + kz.h * 0.5f, kz.w, kz.h },
            { kz.w * 0.5f, kz.h * 0.5f }, kz.rotation, WHITE);
    }

    for (const auto& cv : lv.conveyors) {
        DrawRectanglePro(
            { cv.x, cv.y, cv.length, cv.beltH },
            { 0, cv.beltH * 0.5f }, cv.rotation, WHITE);
    }

    // Elevators intentionally omitted — no collision geometry, transparent to light.

    EndOccluders();
}

// ─────────────────────────────────────────────────────────────────────────────
//  UploadLights
//  Merges lv.lights (level-authored) with _runtimeLights (per-frame ephemeral)
//  and uploads all four per-light arrays to the shader.
//
//  FIX: lightAnim uniform is now populated with flickerFreq/Amp and
//  pulseFreq/Amp so the shader can animate each light's intensity every frame.
// ─────────────────────────────────────────────────────────────────────────────
// UploadLights — packs lv.lights + _runtimeLights into flat float arrays and
// uploads them.  Written as a plain loop (no lambda) to avoid the MSVC
// __this-capture quirk that fires when a [&] lambda inside a member function
// accesses both a captured member variable and a parameter of the same class.
void LightingSystem::UploadLights(const LevelData& lv)
{
    float pos[MAX_LIGHTS * 4] = {};
    float col[MAX_LIGHTS * 4] = {};
    float ext[MAX_LIGHTS * 4] = {};
    float fal[MAX_LIGHTS * 4] = {};
    float anim[MAX_LIGHTS * 4] = {};

    int n = 0;

    // We iterate both the level-authored list and the per-frame runtime list.
    // Using a two-pass loop so we never need a lambda or a temporary merged vector.
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto& src = (pass == 0) ? lv.lights : _runtimeLights;
        for (size_t k = 0; k < src.size() && n < MAX_LIGHTS; ++k)
        {
            const LightData& L = src[k];
            if (!L.enabled) continue;

            // Explicit static_cast<float> on every int/enum — avoids C4244 / C4305
            pos[n * 4 + 0] = L.x;
            pos[n * 4 + 1] = L.y;
            pos[n * 4 + 2] = L.radius;
            pos[n * 4 + 3] = static_cast<float>(static_cast<int>(L.type));

            col[n * 4 + 0] = L.r;
            col[n * 4 + 1] = L.g;
            col[n * 4 + 2] = L.b;
            col[n * 4 + 3] = L.intensity;

            ext[n * 4 + 0] = L.direction * (3.14159265f / 180.f);
            ext[n * 4 + 1] = (L.angle * 0.5f) * (3.14159265f / 180.f);
            ext[n * 4 + 2] = L.fogStrength;
            ext[n * 4 + 3] = static_cast<float>(L.bounces);  // int → float, explicit

            fal[n * 4 + 0] = fminf(L.innerRadius, L.radius * 0.99f);
            fal[n * 4 + 1] = 0.f;
            fal[n * 4 + 2] = 0.f;
            fal[n * 4 + 3] = 0.f;

            // Animation params — FIX: previously always zero; shader now uses these.
            // If your LightData does not yet have these fields, add them to
            // LightData in LevelData.h (see patch notes) and they default to 0
            // which means "no animation", safe for existing lights.
            anim[n * 4 + 0] = L.flickerFreq;
            anim[n * 4 + 1] = L.flickerAmp;
            anim[n * 4 + 2] = L.pulseFreq;
            anim[n * 4 + 3] = L.pulseAmp;

            ++n;
        }
    }

    SetShaderValue(_lightShader, _locLP_count, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(_lightShader, _locLP_pos, pos, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_col, col, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_extra, ext, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_falloff, fal, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_anim, anim, SHADER_UNIFORM_VEC4, n);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  RunBlur — separable Gaussian pass in one direction
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::RunBlur(RenderTexture2D src, RenderTexture2D dst, Vector2 dir)
{
    BeginTextureMode(dst);
    ClearBackground(BLANK);
    BeginShaderMode(_blurShader);
    float dirArr[2] = { dir.x, dir.y };
    SetShaderValue(_blurShader, _locBL_dir, dirArr, SHADER_UNIFORM_VEC2);
    DrawTextureRec(src.texture,
        { 0, 0, (float)src.texture.width, -(float)src.texture.height },
        { 0, 0 }, WHITE);
    EndShaderMode();
    EndTextureMode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  DoLightingPass
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::DoLightingPass(Camera2D cam, RenderTexture2D dst,
    bool useBounce, bool isFinalPass)
{
    float resArr[2] = { (float)_liW, (float)_liH };
    float worldOrigin[2] = {
        cam.target.x - cam.offset.x / cam.zoom,
        cam.target.y - cam.offset.y / cam.zoom
    };
    // worldPerPx: world units per lighting RT pixel.
    // Lighting RT is half the scene RT, so each lighting pixel covers 2× more
    // scene pixels, and each scene pixel is (1/zoom) world units.
    float wppX = ((float)_rtW / (float)_liW) / cam.zoom;
    float wppY = ((float)_rtH / (float)_liH) / cam.zoom;
    float worldPerPx[2] = { wppX, wppY };

    Color amb = _ambientColor;
    float ambArr[3] = { amb.r / 255.f, amb.g / 255.f, amb.b / 255.f };

    SetShaderValue(_lightShader, _locLP_resolution, resArr, SHADER_UNIFORM_VEC2);
    SetShaderValue(_lightShader, _locLP_worldOrigin, worldOrigin, SHADER_UNIFORM_VEC2);
    SetShaderValue(_lightShader, _locLP_worldPerPx, worldPerPx, SHADER_UNIFORM_VEC2);
    SetShaderValue(_lightShader, _locLP_ambient, &_globalAmbient, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_lightShader, _locLP_ambientCol, ambArr, SHADER_UNIFORM_VEC3);
    SetShaderValue(_lightShader, _locLP_time, &_time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_lightShader, _locLP_darkness, &_globalDarkness, SHADER_UNIFORM_FLOAT);  // FIX

    int ub = useBounce ? 1 : 0;
    int af = isFinalPass ? 1 : 0;
    SetShaderValue(_lightShader, _locLP_useBounce, &ub, SHADER_UNIFORM_INT);
    SetShaderValue(_lightShader, _locLP_applyFloor, &af, SHADER_UNIFORM_INT);

    BeginTextureMode(dst);
    ClearBackground(BLACK);
    BeginShaderMode(_lightShader);
    SetShaderValueTexture(_lightShader, _locLP_occTex, _occluderRT.texture);
    SetShaderValueTexture(_lightShader, _locLP_bounceTex, _bounceRT.texture);

    // Draw occluder as fullscreen quad — gives shader correct 0→1 fragTexCoord
    // UVs for reconstructing world-space pixel positions.
    // Do NOT replace with DrawRectangle — that doesn't produce normalised UVs.
    DrawTextureRec(_occluderRT.texture,
        { 0, 0, (float)_occluderRT.texture.width, -(float)_occluderRT.texture.height },
        { 0, 0 }, WHITE);

    EndShaderMode();
    EndTextureMode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Composite — main entry point called once per frame
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::Composite(const LevelData& lv, Camera2D cam, Rectangle dst)
{
    if (!_ready) return;
    _time += GetFrameTime();

    // Upload level lights + runtime lights.
    // FIX: animation params (flicker/pulse) are now sent to the GPU here.
    UploadLights(lv);

    // Auto-clear runtime lights so stale entries don't persist across frames.
    _runtimeLights.clear();

    // Check whether any light requests a GI bounce pass.
    bool anyBounce = false;
    for (const auto& L : lv.lights)
        if (L.enabled && L.bounces > 0) { anyBounce = true; break; }

    if (anyBounce) {
        // a. Raw direct light → _directLitRT (no ambient, no floor)
        DoLightingPass(cam, _directLitRT, /*useBounce=*/false, /*isFinalPass=*/false);

        // b. Wide separable Gaussian blur → _bounceRT
        //    FIX: step was 1/liW ≈ 1 pixel — too tight for any visible bleed.
        //    Now 4.0/liW for meaningful indirect light spread.
        RunBlur(_directLitRT, _scratchRT, { 4.0f / (float)_liW, 0.f });
        RunBlur(_scratchRT, _bounceRT, { 0.f, 4.0f / (float)_liH });

        // c. Final pass: direct + compressed bounce + ambient + floor
        DoLightingPass(cam, _finalLightRT, /*useBounce=*/true, /*isFinalPass=*/true);
    }
    else {
        DoLightingPass(cam, _finalLightRT, /*useBounce=*/false, /*isFinalPass=*/true);
    }

    // Tight smooth blur to feather half-res shadow edges before upscale.
    RunBlur(_finalLightRT, _scratchRT, { 0.5f / (float)_liW, 0.f });
    RunBlur(_scratchRT, _finalLightRT, { 0.f,  0.5f / (float)_liH });

    Rectangle blitDst = dst;
    if (blitDst.width <= 0.f || blitDst.height <= 0.f)
        blitDst = { 0.f, 0.f, (float)_rtW, (float)_rtH };

    DrawTexturePro(_sceneRT.texture,
        { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
        blitDst, { 0, 0 }, 0.f, WHITE);

    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(_finalLightRT.texture,
        { 0, 0, (float)_finalLightRT.texture.width, -(float)_finalLightRT.texture.height },
        blitDst, { 0, 0 }, 0.f, WHITE);
    EndBlendMode();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Debug helpers
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::DebugDrawOccluder(Rectangle dst)
{
    DrawTexturePro(_occluderRT.texture,
        { 0, 0, (float)_occluderRT.texture.width, -(float)_occluderRT.texture.height },
        dst, { 0, 0 }, 0.f, WHITE);
    DrawRectangleLinesEx(dst, 2, GREEN);
    DrawText("OCCLUDER", static_cast<int>(dst.x) + 4, static_cast<int>(dst.y) + 4, 12, GREEN);
}
void LightingSystem::DebugDrawScene(Rectangle dst)
{
    DrawTexturePro(_sceneRT.texture,
        { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
        dst, { 0, 0 }, 0.f, WHITE);
    DrawRectangleLinesEx(dst, 2, ORANGE);
    DrawText("SCENE", static_cast<int>(dst.x) + 4, static_cast<int>(dst.y) + 4, 12, ORANGE);
}