#pragma once
#include "raylib.h"
#include <vector>
#include <cmath>
using namespace std;
enum CollisionSide { SIDE_NONE = 0, SIDE_TOP = 1, SIDE_BOTTOM = 2, SIDE_LEFT = 4, SIDE_RIGHT = 8 };
struct CollisionResult { bool hit = false; int side = SIDE_NONE; bool grounded = false; };
struct Platform
{
    float x, y, width, height, rotation;
    Color color;
    static Platform Make(float x, float y, float w, float h,
        float rot = 0.f, Color col = DARKBROWN)
    {
        return { x, y, w, h, rot, col };
    }
    Vector2 Center() const { return { x + width * .5f, y + height * .5f }; }
    void GetCorners(Vector2 c[4]) const
    {
        float cx = x + width * .5f, cy = y + height * .5f;
        float rad = rotation * (3.14159265f / 180.f);
        float ca = cosf(rad), sa = sinf(rad), hw = width * .5f, hh = height * .5f;
        float lx[4] = { -hw,hw,hw,-hw }, ly[4] = { -hh,-hh,hh,hh };
        for (int i = 0; i < 4; i++) c[i] = { cx + lx[i] * ca - ly[i] * sa, cy + lx[i] * sa + ly[i] * ca };
    }
    void Draw() const
    {
        Vector2 c[4]; GetCorners(c);
        DrawTriangle(c[0], c[2], c[1], color);
        DrawTriangle(c[0], c[3], c[2], color);
    }
};
class CollisionManager
{
public:
    static CollisionResult Resolve(Rectangle&, float&, float&, const Platform&, float, float);
    static CollisionResult ResolveAll(Rectangle&, float&, float&, vector<Platform>&, float, float);
    static void DrawAll(const vector<Platform>&);
private:
    static Vector2 ToLocal(Vector2 p, Vector2 center, float rot);
    static Vector2 ToWorld(Vector2 v, float rot);
};