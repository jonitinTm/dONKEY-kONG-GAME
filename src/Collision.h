#pragma once
#include "raylib.h"
#include <vector>
#include <cmath>

using namespace std;

// ─────────────────────────────────────────────
//  CollisionSide bitmask
// ─────────────────────────────────────────────
enum CollisionSide
{
    SIDE_NONE   = 0,
    SIDE_TOP    = 1,
    SIDE_BOTTOM = 2,
    SIDE_LEFT   = 4,
    SIDE_RIGHT  = 8
};

// ─────────────────────────────────────────────
//  CollisionResult
// ─────────────────────────────────────────────
struct CollisionResult
{
    bool hit      = false;
    int  side     = SIDE_NONE;
    bool grounded = false;
};

// ─────────────────────────────────────────────
//  Platform
//
//  Platform::Make(x, y, w, h)                   // flat
//  Platform::Make(x, y, w, h, 20.0f)            // 20 degree tilt
//  Platform::Make(x, y, w, h, -15.0f, RED)      // tilt + custom color
// ─────────────────────────────────────────────
struct Platform
{
    float x;
    float y;
    float width;
    float height;
    float rotation; // degrees, rotates around center
    Color color;

    static Platform Make(
        float x, float y,
        float w, float h,
        float rotDeg = 0.0f,
        Color col    = DARKBROWN)
    {
        Platform p;
        p.x        = x;
        p.y        = y;
        p.width    = w;
        p.height   = h;
        p.rotation = rotDeg;
        p.color    = col;
        return p;
    }

    Vector2 Center() const
    {
        return { x + width * 0.5f, y + height * 0.5f };
    }

    void GetCorners(Vector2 corners[4]) const
    {
        float cx   = x + width  * 0.5f;
        float cy   = y + height * 0.5f;
        float rad  = rotation * (3.14159265f / 180.0f);
        float cosA = cosf(rad);
        float sinA = sinf(rad);
        float hw   = width  * 0.5f;
        float hh   = height * 0.5f;

        float lx[4] = { -hw,  hw,  hw, -hw };
        float ly[4] = { -hh, -hh,  hh,  hh };

        for (int i = 0; i < 4; i++)
        {
            corners[i].x = cx + lx[i] * cosA - ly[i] * sinA;
            corners[i].y = cy + lx[i] * sinA + ly[i] * cosA;
        }
    }

    void Draw() const
    {
        Vector2 c[4];
        GetCorners(c);
        DrawTriangle(c[0], c[2], c[1], color);
        DrawTriangle(c[0], c[3], c[2], color);
    }
};

// ─────────────────────────────────────────────
//  CollisionManager
// ─────────────────────────────────────────────
class CollisionManager
{
public:

    static CollisionResult Resolve(
        Rectangle      &player,
        float          &velocityX,
        float          &velocityY,
        const Platform &platform,
        float           prevX,
        float           prevY);

    static CollisionResult ResolveAll(
        Rectangle        &player,
        float            &velocityX,
        float            &velocityY,
        vector<Platform> &platforms,
        float             prevX,
        float             prevY);

    static void DrawAll(const vector<Platform> &platforms);

private:

    static Vector2 ToLocal(Vector2 worldPoint, Vector2 center, float rotDeg);
    static Vector2 RotateToWorld(Vector2 localDir, float rotDeg);
    static bool PrevOverlapX(Rectangle prev, Rectangle localPlat);
    static bool PrevOverlapY(Rectangle prev, Rectangle localPlat);
};
