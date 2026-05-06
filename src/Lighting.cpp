// ============================================================
//  Lighting.cpp
// ============================================================
#include "Lighting.h"
#include "rlgl.h"
#include <cmath>
#include <vector>
#include <cstring>

// ── Maximum lights uploaded per frame ─────────────────────────────────────────
// Must match the array size in the shader.
static constexpr int MAX_LIGHTS = 32;

// ─────────────────────────────────────────────────────────────────────────────
//  GLSL: per-pixel raytracing pass
//  Inputs : sceneTex (rendered scene), occluderTex (white = blocks light),
//           bounceTex (blurred direct-lit pass for 1-bounce GI).
//  Output : final lit pixel.
// ─────────────────────────────────────────────────────────────────────────────
static const char* LIGHTING_FS = R"GLSL(
#version 330

in vec2 fragTexCoord;
in vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;       // scene (raylib's default texture slot)
uniform sampler2D occluderTex;
uniform sampler2D bounceTex;

uniform vec2  resolution;
uniform vec2  camOffset;          // worldX = pixelX + camOffset.x  (for ray math in world space)
uniform float globalAmbient;
uniform vec3  ambientColor;
uniform float globalDarkness;
uniform float uTime;
uniform int   useBounce;

#define MAX_LIGHTS 32
uniform int  lightCount;
// xy = world pos, z = radius (or sky max-distance), w = type (0=point,1=spot,2=sky)
uniform vec4 lightPos  [MAX_LIGHTS];
// rgb = linear color, a = intensity
uniform vec4 lightCol  [MAX_LIGHTS];
// x = spot/sky direction (radians), y = spot half-angle (radians),
// z = fog strength, w = bounces (>0 means contributes to bounce pass)
uniform vec4 lightExtra[MAX_LIGHTS];

// Sample occluder at a world-space position. Y is flipped because raylib RTs
// have inverted Y when read back via texture().
float sampleOccluder(vec2 worldPos) {
    vec2 screen = worldPos - camOffset;
    vec2 uv = screen / resolution;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return texture(occluderTex, vec2(uv.x, 1.0 - uv.y)).r;
}

// Visibility from `from` to `to` — 1.0 = clear LOS, 0.0 = blocked.
// We bias the start away from the source pixel so a pixel inside an occluder
// (e.g. the surface of a platform) can still receive light from outside.
float visibility(vec2 from, vec2 to) {
    const int STEPS = 28;
    vec2 d = (to - from) / float(STEPS);
    vec2 p = from + d * 1.5;            // bias away from `from`
    for (int i = 2; i < STEPS - 1; i++) {
        if (sampleOccluder(p) > 0.5) return 0.0;
        p += d;
    }
    return 1.0;
}

// Soft visibility — averages a few rays slightly perpendicular for penumbra.
float softVisibility(vec2 from, vec2 to) {
    vec2 dir = normalize(to - from);
    vec2 perp = vec2(-dir.y, dir.x);
    float v = 0.0;
    v += visibility(from, to);
    v += visibility(from, to + perp * 4.0);
    v += visibility(from, to - perp * 4.0);
    return v / 3.0;
}

vec3 directContribution(int i, vec2 pixelPos) {
    int type = int(lightPos[i].w + 0.5);

    // ── Skylight: directional ray cast in -direction; if blocked, occluded ──
    if (type == 2) {
        float ang = lightExtra[i].x;
        vec2 toSky = vec2(cos(ang), sin(ang));   // direction toward "sky"
        float maxDist = lightPos[i].z;
        const int SKY_STEPS = 18;
        vec2 d = toSky * (maxDist / float(SKY_STEPS));
        vec2 p = pixelPos + d * 1.5;
        for (int s = 0; s < SKY_STEPS; s++) {
            if (sampleOccluder(p) > 0.5) return vec3(0.0);
            p += d;
        }
        return lightCol[i].rgb * lightCol[i].a;
    }

    vec2 lp = lightPos[i].xy;
    float radius = lightPos[i].z;
    vec2 toL = lp - pixelPos;
    float dist = length(toL);
    if (dist > radius) return vec3(0.0);

    float coneFactor = 1.0;
    if (type == 1) {
        // Spotlight cone
        vec2 spotDir = vec2(cos(lightExtra[i].x), sin(lightExtra[i].x));
        float halfAng = lightExtra[i].y;
        vec2 toLN = toL / max(dist, 0.0001);
        // Note: spotDir points where the light shines, so the vector FROM light TO pixel
        // should align with spotDir. That vector is -toLN.
        float cosA = dot(-toLN, spotDir);
        float coneEdge = cos(halfAng);
        if (cosA < coneEdge) return vec3(0.0);
        // Soft falloff at the cone edge
        coneFactor = smoothstep(coneEdge, mix(coneEdge, 1.0, 0.25), cosA);
    }

    float vis = softVisibility(pixelPos, lp);
    if (vis < 0.001) return vec3(0.0);

    // Inverse-quadratic-ish falloff with hard cutoff at radius.
    float t = clamp(1.0 - dist / radius, 0.0, 1.0);
    float atten = t * t;

    return lightCol[i].rgb * lightCol[i].a * atten * vis * coneFactor;
}

vec3 volumetricContribution(int i, vec2 pixelPos) {
    float fogStr = lightExtra[i].z;
    if (fogStr <= 0.001) return vec3(0.0);
    int type = int(lightPos[i].w + 0.5);
    if (type == 2) return vec3(0.0); // skip sky for volumetric (would tint everything)

    vec2 lp = lightPos[i].xy;
    float radius = lightPos[i].z;
    vec2 toL = lp - pixelPos;
    float dist = length(toL);
    if (dist > radius * 1.2) return vec3(0.0);

    const int FOG_STEPS = 24;
    vec2 d = toL / float(FOG_STEPS);
    vec2 p = pixelPos + d * 0.5;
    vec3 accum = vec3(0.0);
    float stepLen = length(d);

    vec2 spotDir = vec2(0.0);
    float halfAng = 0.0;
    bool isSpot = (type == 1);
    if (isSpot) {
        spotDir = vec2(cos(lightExtra[i].x), sin(lightExtra[i].x));
        halfAng = lightExtra[i].y;
    }

    for (int s = 0; s < FOG_STEPS; s++) {
        if (sampleOccluder(p) < 0.5) {
            float dl = length(lp - p);
            float at = clamp(1.0 - dl / radius, 0.0, 1.0);
            at *= at;

            float cone = 1.0;
            if (isSpot) {
                vec2 fromL = (p - lp) / max(dl, 0.0001);
                float c = dot(fromL, spotDir);
                cone = smoothstep(cos(halfAng) - 0.05, cos(halfAng), c);
            }

            // Anisotropic-ish in-scatter: brighter when looking toward light.
            // In 2D we approximate by just using attenuation.
            accum += lightCol[i].rgb * lightCol[i].a * at * cone;
        }
        p += d;
    }
    return accum * (stepLen / 64.0) * fogStr;
}

void main() {
    vec2 pixelPos = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * resolution + camOffset;

    vec3 sceneColor = texture(texture0, fragTexCoord).rgb;

    vec3 lighting = ambientColor * globalAmbient;
    vec3 fog      = vec3(0.0);

    int n = min(lightCount, MAX_LIGHTS);
    for (int i = 0; i < n; i++) {
        lighting += directContribution(i, pixelPos);
        fog      += volumetricContribution(i, pixelPos);
    }

    // 1-bounce GI via blurred direct-lit texture.
    if (useBounce == 1) {
        vec3 b = texture(bounceTex, fragTexCoord).rgb;
        lighting += b * 0.6;
    }

    // Compose: darken the unlit scene, then multiply by accumulated lighting.
    // mix(1, ambient, darkness) means if darkness=1, base contribution is just ambient,
    // and lights have to bring it back up.
    vec3 baseFloor = sceneColor * mix(1.0, globalAmbient, globalDarkness);
    vec3 lit       = sceneColor * lighting;

    // max() avoids "subtractive" double-darkening; lights only add brightness.
    vec3 outCol = max(baseFloor, lit);

    // Volumetric is additive; clamp final result.
    outCol += fog;
    outCol = min(outCol, vec3(1.4));   // small headroom over 1.0 for bloom-ish feel

    finalColor = vec4(outCol, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  GLSL: simple 9-tap gaussian blur (for the bounce pass)
// ─────────────────────────────────────────────────────────────────────────────
static const char* BLUR_FS = R"GLSL(
#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 blurDir;       // (1/w, 0) for horizontal, (0, 1/h) for vertical
uniform vec2 resolution;

void main() {
    vec2 step = blurDir * 1.5;          // 1.5 px tap distance
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
//  Implementation
// ─────────────────────────────────────────────────────────────────────────────
void LightingSystem::Init(int screenW, int screenH)
{
    _w = screenW;
    _h = screenH;

    _sceneRT     = LoadRenderTexture(_w, _h);
    _occluderRT  = LoadRenderTexture(_w, _h);
    _directLitRT = LoadRenderTexture(_w, _h);
    _bounceRT    = LoadRenderTexture(_w, _h);
    _scratchRT   = LoadRenderTexture(_w, _h);

    SetTextureFilter(_sceneRT.texture,     TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_occluderRT.texture,  TEXTURE_FILTER_POINT);    // crisp shadow edges
    SetTextureFilter(_directLitRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_bounceRT.texture,    TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_scratchRT.texture,   TEXTURE_FILTER_BILINEAR);

    _lightShader = LoadShaderFromMemory(nullptr, LIGHTING_FS);
    _blurShader  = LoadShaderFromMemory(nullptr, BLUR_FS);

    // Cache uniform locations
    _locLightCount  = GetShaderLocation(_lightShader, "lightCount");
    _locLightPos    = GetShaderLocation(_lightShader, "lightPos");
    _locLightCol    = GetShaderLocation(_lightShader, "lightCol");
    _locLightExtra  = GetShaderLocation(_lightShader, "lightExtra");
    _locResolution  = GetShaderLocation(_lightShader, "resolution");
    _locCamOffset   = GetShaderLocation(_lightShader, "camOffset");
    _locAmbient     = GetShaderLocation(_lightShader, "globalAmbient");
    _locAmbientCol  = GetShaderLocation(_lightShader, "ambientColor");
    _locDarkness    = GetShaderLocation(_lightShader, "globalDarkness");
    _locTime        = GetShaderLocation(_lightShader, "uTime");
    _locOccluderTex = GetShaderLocation(_lightShader, "occluderTex");
    _locBounceTex   = GetShaderLocation(_lightShader, "bounceTex");
    _locUseBounce   = GetShaderLocation(_lightShader, "useBounce");

    _locBlurDir = GetShaderLocation(_blurShader, "blurDir");
    _locBlurRes = GetShaderLocation(_blurShader, "resolution");

    _ready = true;
}

void LightingSystem::Shutdown()
{
    if (!_ready) return;
    UnloadRenderTexture(_sceneRT);
    UnloadRenderTexture(_occluderRT);
    UnloadRenderTexture(_directLitRT);
    UnloadRenderTexture(_bounceRT);
    UnloadRenderTexture(_scratchRT);
    UnloadShader(_lightShader);
    UnloadShader(_blurShader);
    _ready = false;
}

// ── Frame flow ───────────────────────────────────────────────────────────────
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

void LightingSystem::BeginOccluders(Camera2D cam)
{
    BeginTextureMode(_occluderRT);
    ClearBackground(BLACK);   // black = open space
    BeginMode2D(cam);
}
void LightingSystem::EndOccluders()
{
    EndMode2D();
    EndTextureMode();
}

void LightingSystem::BakeOccludersFromLevel(const LevelData& lv, Camera2D cam)
{
    BeginOccluders(cam);

    // Platforms — solid silhouettes. We approximate tilted platforms as their AABB,
    // which is fine for shadowing purposes.
    for (const auto& p : lv.platforms) {
        float h = (p.h <= 0.f) ? 8.f : p.h;       // matches Platform::Make default
        DrawRectangleRec({ p.x, p.y, p.w, h }, WHITE);
    }
    // Beams — assume a reasonable 64x16 silhouette per beam.
    for (const auto& b : lv.beams) {
        DrawRectangleRec({ b.x, b.y, 64.f, 16.f }, WHITE);
    }
    // Kill zones occlude too (golden pistons block light).
    for (const auto& kz : lv.killZones) {
        if (kz.texId == KillZoneTexture::NONE) continue;
        DrawRectanglePro({ kz.x + kz.w * 0.5f, kz.y + kz.h * 0.5f, kz.w, kz.h },
                         { kz.w * 0.5f, kz.h * 0.5f }, kz.rotation, WHITE);
    }
    // Conveyors block as their bounding rect.
    for (const auto& cv : lv.conveyors) {
        DrawRectanglePro({ cv.x, cv.y, cv.length, cv.beltH },
                         { 0, cv.beltH * 0.5f }, cv.rotation, WHITE);
    }
    // Elevators — solid rectangle.
    for (const auto& el : lv.elevators) {
        DrawRectangleRec({ el.x, el.y, el.w, el.h }, WHITE);
    }

    EndOccluders();
}

// ── Composite (final lit pass) ───────────────────────────────────────────────
void LightingSystem::UploadLights(const LevelData& lv)
{
    float pos[MAX_LIGHTS * 4]   = {};
    float col[MAX_LIGHTS * 4]   = {};
    float ext[MAX_LIGHTS * 4]   = {};

    int n = 0;
    for (size_t k = 0; k < lv.lights.size() && n < MAX_LIGHTS; k++) {
        const LightData& L = lv.lights[k];
        if (!L.enabled) continue;

        pos[n*4 + 0] = L.x;
        pos[n*4 + 1] = L.y;
        pos[n*4 + 2] = L.radius;
        pos[n*4 + 3] = (float)(int)L.type;

        col[n*4 + 0] = L.r;
        col[n*4 + 1] = L.g;
        col[n*4 + 2] = L.b;
        col[n*4 + 3] = L.intensity;

        ext[n*4 + 0] = L.direction * (3.14159265f / 180.f);
        ext[n*4 + 1] = (L.angle * 0.5f) * (3.14159265f / 180.f);
        ext[n*4 + 2] = L.fogStrength;
        ext[n*4 + 3] = (float)L.bounces;
        n++;
    }

    SetShaderValue (_lightShader, _locLightCount, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(_lightShader, _locLightPos,   pos, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLightCol,   col, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLightExtra, ext, SHADER_UNIFORM_VEC4, n);
    }
}

void LightingSystem::RunBlur(RenderTexture2D src, RenderTexture2D dst, Vector2 dir)
{
    BeginTextureMode(dst);
    ClearBackground(BLANK);
    BeginShaderMode(_blurShader);
    float dirArr[2] = { dir.x, dir.y };
    float resArr[2] = { (float)_w, (float)_h };
    SetShaderValue(_blurShader, _locBlurDir, dirArr, SHADER_UNIFORM_VEC2);
    SetShaderValue(_blurShader, _locBlurRes, resArr, SHADER_UNIFORM_VEC2);
    DrawTextureRec(src.texture,
                   { 0, 0, (float)src.texture.width, -(float)src.texture.height },
                   { 0, 0 }, WHITE);
    EndShaderMode();
    EndTextureMode();
}

void LightingSystem::Composite(const LevelData& lv, Camera2D cam)
{
    if (!_ready) return;
    _time += GetFrameTime();

    UploadLights(lv);

    float resArr[2] = { (float)_w, (float)_h };
    // Camera offset converts screen-space to world-space coords (cam.target gives world
    // origin at screen center when offset=center, but our cam typically uses offset=(0,0)
    // and target=(0,0) so screenPx == worldPx). We honour the general case:
    float camOff[2] = {
        cam.target.x - cam.offset.x / cam.zoom,
        cam.target.y - cam.offset.y / cam.zoom
    };
    Color amb = _ambientColor;
    float ambArr[3] = { amb.r / 255.f, amb.g / 255.f, amb.b / 255.f };

    SetShaderValue(_lightShader, _locResolution, resArr,           SHADER_UNIFORM_VEC2);
    SetShaderValue(_lightShader, _locCamOffset,  camOff,           SHADER_UNIFORM_VEC2);
    SetShaderValue(_lightShader, _locAmbient,    &_globalAmbient,  SHADER_UNIFORM_FLOAT);
    SetShaderValue(_lightShader, _locAmbientCol, ambArr,           SHADER_UNIFORM_VEC3);
    SetShaderValue(_lightShader, _locDarkness,   &_globalDarkness, SHADER_UNIFORM_FLOAT);
    SetShaderValue(_lightShader, _locTime,       &_time,           SHADER_UNIFORM_FLOAT);

    // ── Pass 1: direct lighting (no bounce yet) into _directLitRT ───────────
    bool anyBounce = false;
    for (const auto& L : lv.lights) if (L.bounces > 0 && L.enabled) { anyBounce = true; break; }

    int useBounce = 0;
    SetShaderValue(_lightShader, _locUseBounce, &useBounce, SHADER_UNIFORM_INT);

    BeginTextureMode(_directLitRT);
    ClearBackground(BLACK);
    BeginShaderMode(_lightShader);
    SetShaderValueTexture(_lightShader, _locOccluderTex, _occluderRT.texture);
    // Bind a placeholder bounce texture; not sampled because useBounce=0.
    SetShaderValueTexture(_lightShader, _locBounceTex, _occluderRT.texture);
    DrawTextureRec(_sceneRT.texture,
                   { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
                   { 0, 0 }, WHITE);
    EndShaderMode();
    EndTextureMode();

    if (anyBounce) {
        // ── Pass 2: separable blur of the direct-lit image into _bounceRT ──
        //  _directLitRT  --H-->  _scratchRT  --V-->  _bounceRT
        RunBlur(_directLitRT, _scratchRT, { 1.f / (float)_w, 0.f });
        RunBlur(_scratchRT,   _bounceRT,  { 0.f, 1.f / (float)_h });

        // ── Pass 3: re-run lighting with bounce contribution, to backbuffer
        useBounce = 1;
        SetShaderValue(_lightShader, _locUseBounce, &useBounce, SHADER_UNIFORM_INT);
        BeginShaderMode(_lightShader);
        SetShaderValueTexture(_lightShader, _locOccluderTex, _occluderRT.texture);
        SetShaderValueTexture(_lightShader, _locBounceTex,   _bounceRT.texture);
        // Use the ORIGINAL scene as input so we don't double-light surfaces.
        DrawTextureRec(_sceneRT.texture,
                       { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
                       { 0, 0 }, WHITE);
        EndShaderMode();
    } else {
        // No bounces — blit the direct-lit result to the backbuffer.
        DrawTextureRec(_directLitRT.texture,
                       { 0, 0, (float)_directLitRT.texture.width, -(float)_directLitRT.texture.height },
                       { 0, 0 }, WHITE);
    }
}

// ── Debug ────────────────────────────────────────────────────────────────────
void LightingSystem::DebugDrawOccluder(Rectangle dst)
{
    DrawTexturePro(_occluderRT.texture,
                   { 0, 0, (float)_occluderRT.texture.width, -(float)_occluderRT.texture.height },
                   dst, { 0,0 }, 0.f, WHITE);
    DrawRectangleLinesEx(dst, 2, GREEN);
    DrawText("OCCLUDER", (int)dst.x + 4, (int)dst.y + 4, 12, GREEN);
}
void LightingSystem::DebugDrawScene(Rectangle dst)
{
    DrawTexturePro(_sceneRT.texture,
                   { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
                   dst, { 0,0 }, 0.f, WHITE);
    DrawRectangleLinesEx(dst, 2, ORANGE);
    DrawText("SCENE", (int)dst.x + 4, (int)dst.y + 4, 12, ORANGE);
}
