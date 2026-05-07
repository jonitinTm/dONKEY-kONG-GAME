// ============================================================
//  Lighting.cpp  (v3 — simpler, blend-mode compose)
//
//  Pipeline:
//   1. Scene  → _sceneRT     (full res, your in-world content)
//   2. Occluders → _occluderRT (half res, white = blocker)
//   3. Lighting (ray-march)  → _finalLightRT (half res, with
//      ambient floor BAKED IN — so the RT is a complete
//      multiplier you can apply to the scene with no compose
//      shader needed).
//   4. Composite: blit _sceneRT to backbuffer, then blit
//      _finalLightRT on top with BLEND_MULTIPLIED.
//
//  Why this is more robust than v2:
//   • No compose shader to fail to compile.
//   • Standard raylib blend modes — well-tested by the engine.
//   • The lighting RT alone is a complete "darkness map" — you
//     can also DebugDraw it and it'll look right on its own.
// ============================================================
#include "Lighting.h"
#include <cmath>
#include <cstring>
#include <cstdio>

static constexpr int MAX_LIGHTS = 32;

// ─────────────────────────────────────────────────────────────────────────────
//  Lighting shader.  Runs at HALF resolution.  Outputs a multiplier the
//  scene will be multiplied by — with ambient floor pre-baked into it.
//  finalColor.rgb = max(ambientFloor, ambientColor*globalAmbient + Σ lightContrib)
// ─────────────────────────────────────────────────────────────────────────────
static const char* LIGHTING_FS = R"GLSL(
#version 330

in  vec2 fragTexCoord;
in  vec4 fragColor;
out vec4 finalColor;

uniform sampler2D texture0;        // carrier — unused by us, but raylib binds it
uniform sampler2D occluderTex;
uniform sampler2D bounceTex;

uniform vec2  resolution;
uniform vec2  worldOrigin;
uniform vec2  worldPerPx;
uniform float globalAmbient;       // 0..1
uniform float globalDarkness;      // 0..1   (final floor = (1-darkness) + darkness*globalAmbient)
uniform vec3  ambientColor;
uniform float uTime;
uniform int   useBounce;

#define MAX_LIGHTS 32
uniform int  lightCount;
uniform vec4 lightPos  [MAX_LIGHTS];   // xy=worldPos, z=radius, w=type
uniform vec4 lightCol  [MAX_LIGHTS];   // rgb=color, a=intensity
uniform vec4 lightExtra[MAX_LIGHTS];   // x=dirRad, y=halfAngRad, z=fogStrength, w=bounces

float sampleOccluder(vec2 worldPos) {
    vec2 screen = (worldPos - worldOrigin) / worldPerPx;
    vec2 uv = screen / resolution;
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return 0.0;
    return texture(occluderTex, vec2(uv.x, 1.0 - uv.y)).r;
}

float visibility(vec2 from, vec2 to) {
    const int STEPS = 16;
    vec2 d = (to - from) / float(STEPS);
    vec2 p = from + d * 1.5;
    for (int i = 2; i < STEPS - 1; i++) {
        if (sampleOccluder(p) > 0.5) return 0.0;
        p += d;
    }
    return 1.0;
}

vec3 directContribution(int i, vec2 pixelPos) {
    int type = int(lightPos[i].w + 0.5);
    if (type == 2) {
        // Skylight
        float ang = lightExtra[i].x;
        vec2 toSky = vec2(cos(ang), sin(ang));
        float maxDist = lightPos[i].z;
        const int SKY_STEPS = 12;
        vec2 d = toSky * (maxDist / float(SKY_STEPS));
        vec2 p = pixelPos + d * 1.5;
        for (int s = 0; s < SKY_STEPS; s++) {
            if (sampleOccluder(p) > 0.5) return vec3(0.0);
            p += d;
        }
        return lightCol[i].rgb * lightCol[i].a;
    }

    vec2  lp     = lightPos[i].xy;
    float radius = lightPos[i].z;
    vec2  toL    = lp - pixelPos;
    float dist   = length(toL);
    if (dist > radius) return vec3(0.0);

    float coneFactor = 1.0;
    if (type == 1) {
        vec2  spotDir  = vec2(cos(lightExtra[i].x), sin(lightExtra[i].x));
        float halfAng  = lightExtra[i].y;
        vec2  toLN     = toL / max(dist, 0.0001);
        float cosA     = dot(-toLN, spotDir);
        float coneEdge = cos(halfAng);
        if (cosA < coneEdge) return vec3(0.0);
        coneFactor = smoothstep(coneEdge, mix(coneEdge, 1.0, 0.25), cosA);
    }

    float vis = visibility(pixelPos, lp);
    if (vis < 0.001) return vec3(0.0);

    float t = clamp(1.0 - dist / radius, 0.0, 1.0);
    float atten = t * t;

    return lightCol[i].rgb * lightCol[i].a * atten * vis * coneFactor;
}

vec3 volumetricContribution(int i, vec2 pixelPos) {
    float fogStr = lightExtra[i].z;
    if (fogStr <= 0.001) return vec3(0.0);
    int type = int(lightPos[i].w + 0.5);
    if (type == 2) return vec3(0.0);

    vec2  lp     = lightPos[i].xy;
    float radius = lightPos[i].z;
    vec2  toL    = lp - pixelPos;
    float dist   = length(toL);
    if (dist > radius * 1.2) return vec3(0.0);

    const int FOG_STEPS = 12;
    vec2 d = toL / float(FOG_STEPS);
    vec2 p = pixelPos + d * 0.5;
    vec3 accum = vec3(0.0);
    float stepLen = length(d);

    bool  isSpot  = (type == 1);
    vec2  spotDir = vec2(0.0);
    float halfAng = 0.0;
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
                vec2  fromL = (p - lp) / max(dl, 0.0001);
                float c     = dot(fromL, spotDir);
                cone = smoothstep(cos(halfAng) - 0.05, cos(halfAng), c);
            }
            accum += lightCol[i].rgb * lightCol[i].a * at * cone;
        }
        p += d;
    }
    return accum * (stepLen / 64.0) * fogStr;
}

void main() {
    vec2 screenPx = vec2(fragTexCoord.x, 1.0 - fragTexCoord.y) * resolution;
    vec2 pixelPos = screenPx * worldPerPx + worldOrigin;

    // Ambient term
    vec3 lighting = ambientColor * globalAmbient;

    int n = min(lightCount, MAX_LIGHTS);
    for (int i = 0; i < n; i++) {
        lighting += directContribution(i, pixelPos);
        lighting += volumetricContribution(i, pixelPos);
    }

    if (useBounce == 1) {
        vec3 b = texture(bounceTex, vec2(fragTexCoord.x, 1.0 - fragTexCoord.y)).rgb;
        lighting += b * 0.6;
    }

    // Bake the floor: even pure-dark areas multiply scene by this much.
    // floor = (1 - darkness) + darkness * globalAmbient
    //   darkness=0 → floor=1 → no darkening
    //   darkness=1 → floor=globalAmbient → full darkening
    float floorVal = mix(1.0, globalAmbient, globalDarkness);
    lighting = max(lighting, vec3(floorVal));

    // Mild headroom for lights brighter than 1.0
    lighting = min(lighting, vec3(1.6));

    finalColor = vec4(lighting, 1.0);
}
)GLSL";

// ─────────────────────────────────────────────────────────────────────────────
//  Separable 9-tap blur (used only for bounce).
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
//  Implementation
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
    SetTextureFilter(_occluderRT.texture, TEXTURE_FILTER_POINT);
    SetTextureFilter(_directLitRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_scratchRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_bounceRT.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(_finalLightRT.texture, TEXTURE_FILTER_BILINEAR);

    _lightShader = LoadShaderFromMemory(nullptr, LIGHTING_FS);
    _blurShader = LoadShaderFromMemory(nullptr, BLUR_FS);

    // ── Compose shader is no longer used; we use BLEND_MULTIPLIED instead.
    _composeShader.id = 0;

    _locLP_count = GetShaderLocation(_lightShader, "lightCount");
    _locLP_pos = GetShaderLocation(_lightShader, "lightPos");
    _locLP_col = GetShaderLocation(_lightShader, "lightCol");
    _locLP_extra = GetShaderLocation(_lightShader, "lightExtra");
    _locLP_resolution = GetShaderLocation(_lightShader, "resolution");
    _locLP_worldOrigin = GetShaderLocation(_lightShader, "worldOrigin");
    _locLP_worldPerPx = GetShaderLocation(_lightShader, "worldPerPx");
    _locLP_ambient = GetShaderLocation(_lightShader, "globalAmbient");
    _locLP_ambientCol = GetShaderLocation(_lightShader, "ambientColor");
    _locLP_time = GetShaderLocation(_lightShader, "uTime");
    _locLP_useBounce = GetShaderLocation(_lightShader, "useBounce");
    _locLP_occTex = GetShaderLocation(_lightShader, "occluderTex");
    _locLP_bounceTex = GetShaderLocation(_lightShader, "bounceTex");
    // NEW: darkness used by lighting shader
    int locDarkness = GetShaderLocation(_lightShader, "globalDarkness");
    _locCM_darkness = locDarkness;   // reuse this slot

    _locBL_dir = GetShaderLocation(_blurShader, "blurDir");

    // Print diagnostics so the user can tell if a shader failed.
    printf("[Lighting] lightShader.id=%u  blurShader.id=%u  rtW=%d rtH=%d  liW=%d liH=%d\n",
        _lightShader.id, _blurShader.id, _rtW, _rtH, _liW, _liH);
    printf("[Lighting] uniforms: count=%d pos=%d col=%d extra=%d res=%d origin=%d wpp=%d "
        "amb=%d ambCol=%d time=%d useBnc=%d occ=%d bnc=%d dark=%d\n",
        _locLP_count, _locLP_pos, _locLP_col, _locLP_extra, _locLP_resolution,
        _locLP_worldOrigin, _locLP_worldPerPx, _locLP_ambient, _locLP_ambientCol,
        _locLP_time, _locLP_useBounce, _locLP_occTex, _locLP_bounceTex, locDarkness);

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

void LightingSystem::BakeOccludersFromLevel(const LevelData& lv, Camera2D cam)
{
    BeginOccluders(cam);

    for (const auto& p : lv.platforms) {
        float h = (p.h <= 0.f) ? 8.f : p.h;
        DrawRectangleRec({ p.x, p.y, p.w, h }, WHITE);
    }
    for (const auto& b : lv.beams) {
        DrawRectangleRec({ b.x, b.y, 64.f, 16.f }, WHITE);
    }
    for (const auto& kz : lv.killZones) {
        if (kz.texId == KillZoneTexture::NONE) continue;
        DrawRectanglePro({ kz.x + kz.w * 0.5f, kz.y + kz.h * 0.5f, kz.w, kz.h },
            { kz.w * 0.5f, kz.h * 0.5f }, kz.rotation, WHITE);
    }
    for (const auto& cv : lv.conveyors) {
        DrawRectanglePro({ cv.x, cv.y, cv.length, cv.beltH },
            { 0, cv.beltH * 0.5f }, cv.rotation, WHITE);
    }
    for (const auto& el : lv.elevators) {
        DrawRectangleRec({ el.x, el.y, el.w, el.h }, WHITE);
    }

    EndOccluders();
}

// ── Light upload ─────────────────────────────────────────────────────────────
void LightingSystem::UploadLights(const LevelData& lv)
{
    float pos[MAX_LIGHTS * 4] = {};
    float col[MAX_LIGHTS * 4] = {};
    float ext[MAX_LIGHTS * 4] = {};

    int n = 0;
    for (size_t k = 0; k < lv.lights.size() && n < MAX_LIGHTS; k++) {
        const LightData& L = lv.lights[k];
        if (!L.enabled) continue;

        pos[n * 4 + 0] = L.x;
        pos[n * 4 + 1] = L.y;
        pos[n * 4 + 2] = L.radius;
        pos[n * 4 + 3] = (float)(int)L.type;

        col[n * 4 + 0] = L.r;
        col[n * 4 + 1] = L.g;
        col[n * 4 + 2] = L.b;
        col[n * 4 + 3] = L.intensity;

        ext[n * 4 + 0] = L.direction * (3.14159265f / 180.f);
        ext[n * 4 + 1] = (L.angle * 0.5f) * (3.14159265f / 180.f);
        ext[n * 4 + 2] = L.fogStrength;
        ext[n * 4 + 3] = (float)L.bounces;
        n++;
    }

    SetShaderValue(_lightShader, _locLP_count, &n, SHADER_UNIFORM_INT);
    if (n > 0) {
        SetShaderValueV(_lightShader, _locLP_pos, pos, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_col, col, SHADER_UNIFORM_VEC4, n);
        SetShaderValueV(_lightShader, _locLP_extra, ext, SHADER_UNIFORM_VEC4, n);
    }
}

// ── Blur (separable) ─────────────────────────────────────────────────────────
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

// ── One lighting pass ────────────────────────────────────────────────────────
void LightingSystem::DoLightingPass(Camera2D cam, RenderTexture2D dst, bool useBounce)
{
    float resArr[2] = { (float)_liW, (float)_liH };
    float worldOrigin[2] = {
        cam.target.x - cam.offset.x / cam.zoom,
        cam.target.y - cam.offset.y / cam.zoom
    };
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
    SetShaderValue(_lightShader, _locCM_darkness, &_globalDarkness, SHADER_UNIFORM_FLOAT);
    int ub = useBounce ? 1 : 0;
    SetShaderValue(_lightShader, _locLP_useBounce, &ub, SHADER_UNIFORM_INT);

    BeginTextureMode(dst);
    ClearBackground(BLACK);
    BeginShaderMode(_lightShader);
    SetShaderValueTexture(_lightShader, _locLP_occTex, _occluderRT.texture);
    SetShaderValueTexture(_lightShader, _locLP_bounceTex, _bounceRT.texture);
    DrawTextureRec(_occluderRT.texture,
        { 0, 0, (float)_occluderRT.texture.width, -(float)_occluderRT.texture.height },
        { 0, 0 }, WHITE);
    EndShaderMode();
    EndTextureMode();
}

// ── Composite ────────────────────────────────────────────────────────────────
void LightingSystem::Composite(const LevelData& lv, Camera2D cam, Rectangle dst)
{
    if (!_ready) return;
    _time += GetFrameTime();

    UploadLights(lv);

    bool anyBounce = false;
    for (const auto& L : lv.lights) {
        if (L.enabled && L.bounces > 0) { anyBounce = true; break; }
    }

    if (anyBounce) {
        DoLightingPass(cam, _directLitRT, /*useBounce=*/false);
        RunBlur(_directLitRT, _scratchRT, { 1.f / (float)_liW, 0.f });
        RunBlur(_scratchRT, _bounceRT, { 0.f, 1.f / (float)_liH });
        DoLightingPass(cam, _finalLightRT, /*useBounce=*/true);
    }
    else {
        DoLightingPass(cam, _finalLightRT, /*useBounce=*/false);
    }

    Rectangle blitDst = dst;
    if (blitDst.width <= 0.f || blitDst.height <= 0.f)
        blitDst = { 0.f, 0.f, (float)_rtW, (float)_rtH };

    // ── Step A: draw the scene to dst region (full res). ──────────────
    DrawTexturePro(_sceneRT.texture,
        { 0, 0, (float)_sceneRT.texture.width, -(float)_sceneRT.texture.height },
        blitDst, { 0, 0 }, 0.f, WHITE);

    // ── Step B: multiply the lighting term on top with BLEND_MULTIPLIED.
    //           This produces:  output = scene * lighting
    //           Lighting RT already has the ambient floor baked into it,
    //           so scene * lighting >= scene * floor (always darker than scene
    //           when no lights, and brighter than floor when lights illuminate).
    BeginBlendMode(BLEND_MULTIPLIED);
    DrawTexturePro(_finalLightRT.texture,
        { 0, 0, (float)_finalLightRT.texture.width, -(float)_finalLightRT.texture.height },
        blitDst, { 0, 0 }, 0.f, WHITE);
    EndBlendMode();
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