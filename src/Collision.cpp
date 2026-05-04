#include "Collision.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Parallelogram collision resolve — shear/tilt model
//
//  Platform corners (screen space, Y grows downward):
//      yr  = w * tan(tilt)
//      TL  = (x,        y      )
//      TR  = (x+w,      y+yr   )
//      BL  = (x,        y+h    )
//      BR  = (x+w,      y+yr+h )
//
//  The top surface runs TL→TR. We need the outward (upward) normal.
//  Edge vector  e  = TR - TL = ( w,  yr )
//  Rotate 90° CCW to get a normal:  n_raw = ( -yr, w )
//  In screen space (Y down), a player ABOVE the platform has a smaller Y than
//  the surface, so "above" means the normal should have negative ny component.
//  n_raw.y = w > 0  → points downward → flip: n = ( yr, -w ), normalise.
//
//  Signed distance of point P above the top surface:
//      d = dot( P - TL, n_hat )
//  Positive d  → player is above the surface (correct side)
//  Negative d  → player is below / inside the platform
// ─────────────────────────────────────────────────────────────────────────────

// Returns signed distance of P above the top surface line TL→TR.
// Positive = above (where a player standing on top would be).
static float DistAboveSurface(Vector2 p, Vector2 TL, Vector2 TR)
{
    float ex = TR.x - TL.x;   // edge x
    float ey = TR.y - TL.y;   // edge y
    float len = sqrtf(ex * ex + ey * ey);
    if (len < 1e-6f) return 0.f;
    // Outward (upward) normal: rotate edge 90° CW then negate to point upward.
    // n = (ey, -ex) gives a normal. Check: for a flat platform (ey=0, ex=w>0),
    // n = (0, -w/len) → points in -Y direction = upward in screen space. Correct.
    float nx = ey / len;
    float ny = -ex / len;
    return (p.x - TL.x) * nx + (p.y - TL.y) * ny;
}

// Parametric projection of P onto the TL→TR edge (0 = TL, 1 = TR).
static float EdgeParam(Vector2 p, Vector2 TL, Vector2 TR)
{
    float dx = TR.x - TL.x, dy = TR.y - TL.y;
    float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f) return 0.f;
    return ((p.x - TL.x) * dx + (p.y - TL.y) * dy) / len2;
}

CollisionResult CollisionManager::Resolve(
    Rectangle& player, float& velocityX, float& velocityY,
    const Platform& plat, float prevX, float prevY)
{
    CollisionResult result;

    // ── Platform corners ──────────────────────────────────────────────────────
    Vector2 corners[4]; plat.GetCorners(corners);
    Vector2 TL = corners[0], TR = corners[1], BR = corners[2], BL = corners[3];

    // ── Broad-phase AABB ──────────────────────────────────────────────────────
    float pMinX = fminf(TL.x, BL.x);          // left edge (always x)
    float pMaxX = fmaxf(TR.x, BR.x);          // right edge (always x+w)
    float pMinY = fminf(TL.y, TR.y);          // top of bounding box
    float pMaxY = fmaxf(BL.y, BR.y);          // bottom of bounding box

    if (player.x + player.width <= pMinX) return result;
    if (player.x >= pMaxX) return result;
    if (player.y + player.height <= pMinY) return result;
    if (player.y >= pMaxY) return result;

    // ── Player bottom-edge sample points ─────────────────────────────────────
    Vector2 curBotL = { player.x,               player.y + player.height };
    Vector2 curBotR = { player.x + player.width, player.y + player.height };
    Vector2 prevBotL = { prevX,                   prevY + player.height };
    Vector2 prevBotR = { prevX + player.width,     prevY + player.height };

    // Signed distance above the top surface (positive = above, negative = below)
    float dCurL = DistAboveSurface(curBotL, TL, TR);
    float dCurR = DistAboveSurface(curBotR, TL, TR);
    float dPrevL = DistAboveSurface(prevBotL, TL, TR);
    float dPrevR = DistAboveSurface(prevBotR, TL, TR);

    // Use the deepest (most negative) current, and the least-negative previous
    // so that even a partial overlap triggers resolution.
    float dCur = fminf(dCurL, dCurR);   // most penetrated point this frame
    float dPrev = fmaxf(dPrevL, dPrevR);  // highest point last frame

    // ── One-way check ─────────────────────────────────────────────────────────
    // Only block if the player was above (or on) the surface last frame.
    // Tolerance of 2 px handles standing-still jitter from gravity re-applying.
    if (dPrev < -2.f) return result;   // was below → pass through (jumped from under)
    if (dCur > 0.f)  return result;   // still above → no collision

    // ── Horizontal extent check ───────────────────────────────────────────────
    // At least one bottom corner must project onto the platform span (±tolerance).
    const float EDGE_TOL = 6.f;
    float edgeLen = sqrtf((TR.x - TL.x) * (TR.x - TL.x) + (TR.y - TL.y) * (TR.y - TL.y));
    float tolParam = (edgeLen > 1e-6f) ? EDGE_TOL / edgeLen : 0.f;
    float tL = EdgeParam(curBotL, TL, TR);
    float tR = EdgeParam(curBotR, TL, TR);
    if (tL > 1.f + tolParam && tR > 1.f + tolParam) return result;
    if (tL < -tolParam && tR < -tolParam)       return result;

    // ── Resolve ───────────────────────────────────────────────────────────────
    result.hit = true;
    result.grounded = true;
    result.side |= SIDE_TOP;

    // Surface outward (upward) normal, same as in DistAboveSurface
    float ex = TR.x - TL.x, ey = TR.y - TL.y;
    float len = sqrtf(ex * ex + ey * ey);
    float nx = ey / len;
    float ny = -ex / len;

    // Cancel the velocity component into the surface
    float vDotN = velocityX * nx + velocityY * ny;
    if (vDotN < 0.f) {
        velocityX -= vDotN * nx;
        velocityY -= vDotN * ny;
    }

    // Push the player out by the penetration depth along the normal
    float penetration = -dCur;   // dCur < 0, so penetration > 0
    player.x += nx * penetration;
    player.y += ny * penetration;

    return result;
}

CollisionResult CollisionManager::ResolveAll(
    Rectangle& player, float& velocityX, float& velocityY,
    vector<Platform>& platforms, float prevX, float prevY)
{
    CollisionResult merged;
    for (Platform& p : platforms)
    {
        CollisionResult r = Resolve(player, velocityX, velocityY, p, prevX, prevY);
        if (r.hit) {
            merged.hit = true;
            merged.side |= r.side;
            merged.grounded |= r.grounded;
        }
    }
    return merged;
}

void CollisionManager::DrawAll(const vector<Platform>& platforms)
{
    for (const Platform& p : platforms) p.Draw();
}