#pragma once
#include "raylib.h"
#include <vector>
#include <cmath>
using namespace std;

enum CollisionSide { SIDE_NONE = 0, SIDE_TOP = 1, SIDE_BOTTOM = 2, SIDE_LEFT = 4, SIDE_RIGHT = 8 };
struct CollisionResult { bool hit = false; int side = SIDE_NONE; bool grounded = false; };

// ?????????????????????????????????????????????????????????????????????????????
//  Platform — parallelogram geometry matching the level editor's tilt/shear model.
//
//  The "rotation" field stores the tilt angle in degrees (same as PlatformData::tilt).
//  Given top-left origin (x, y), width w, height h and tilt t (degrees), the four
//  corners in world space are:
//
//      yr  = w * tan(t)          <- total vertical shear across the full width
//      TL  = (x,        y      )
//      TR  = (x+w,      y+yr   )
//      BL  = (x,        y+h    )
//      BR  = (x+w,      y+yr+h )
//
//  This is exactly what DrawPlatEnt() in the editor renders, so collision now
//  matches the visual perfectly.
// ?????????????????????????????????????????????????????????????????????????????
struct Platform
{
    float x, y, width, height, rotation;   // rotation = tilt degrees (shear model)
    Color color;

    static Platform Make(float x, float y, float w, float h,
        float tiltDeg = 0.f, Color col = DARKBROWN)
    {
        return { x, y, w, h, tiltDeg, col };
    }

    // Total vertical shear offset at the right edge
    float YR() const { return width * tanf(rotation * (3.14159265f / 180.f)); }

    // The four corners of the parallelogram (TL, TR, BR, BL order)
    void GetCorners(Vector2 c[4]) const
    {
        float yr = YR();
        c[0] = { x,              y };   // TL
        c[1] = { x + width,      y + yr };   // TR
        c[2] = { x + width,      y + yr + height }; // BR
        c[3] = { x,              y + height };   // BL
    }

    // Geometric center of the parallelogram
    Vector2 Center() const { return { x + width * 0.5f, y + YR() * 0.5f + height * 0.5f }; }

    void Draw() const
    {
        Vector2 c[4]; GetCorners(c);
        DrawTriangle(c[0], c[3], c[1], color);   // TL, BL, TR
        DrawTriangle(c[3], c[2], c[1], color);   // BL, BR, TR
    }
};

class CollisionManager
{
public:
    static CollisionResult Resolve(Rectangle&, float&, float&,
        const Platform&, float prevX, float prevY);
    static CollisionResult ResolveAll(Rectangle&, float&, float&,
        vector<Platform>&, float prevX, float prevY);
    static void DrawAll(const vector<Platform>&);
};