#include "Collision.h"

// Converts a world-space point into the platform's local axis-aligned space
// by translating to the platform center, then rotating by -angle.
// This lets us treat any rotated platform as a flat AABB.
Vector2 CollisionManager::ToLocal(Vector2 p, Vector2 c, float rot)
{
    float r = -rot * (3.14159265f / 180.f);
    float ca = cosf(r), sa = sinf(r);
    float dx = p.x - c.x, dy = p.y - c.y;
    return { dx * ca - dy * sa, dx * sa + dy * ca };
}

// Rotates a local-space direction/offset back into world space (+angle).
// Used to turn the axis-aligned correction vector back to world coordinates.
Vector2 CollisionManager::ToWorld(Vector2 v, float rot)
{
    float r = rot * (3.14159265f / 180.f);
    float ca = cosf(r), sa = sinf(r);
    return { v.x * ca - v.y * sa, v.x * sa + v.y * ca };
}

CollisionResult CollisionManager::Resolve(
    Rectangle& player, float& velocityX, float& velocityY,
    const Platform& plat, float prevX, float prevY)
{
    CollisionResult result;
    Vector2 pc = plat.Center();

    // ── Step 1: Move everything into the platform's local space ──────────
    // Transforming into local space turns the rotated platform into a flat
    // axis-aligned box at the origin, so all overlap math is trivial.
    Vector2 cur = ToLocal({ player.x + player.width * .5f, player.y + player.height * .5f }, pc, plat.rotation);
    Vector2 prev = ToLocal({ prevX + player.width * .5f, prevY + player.height * .5f }, pc, plat.rotation);

    // Platform AABB centered at origin in local space
    Rectangle lp = { -plat.width * .5f, -plat.height * .5f, plat.width, plat.height };

    // Player rect in local space — current and previous frame
    Rectangle lc = { cur.x - player.width * .5f, cur.y - player.height * .5f, player.width, player.height };
    Rectangle lp2 = { prev.x - player.width * .5f, prev.y - player.height * .5f, player.width, player.height };

    // ── Step 2: Broad-phase — skip if no overlap this frame ──────────────
    if (!CheckCollisionRecs(lc, lp)) return result;

    // ── Step 3: One-way check ─────────────────────────────────────────────
    // Platforms are one-way (like Mario): only the TOP surface blocks.
    // If the player's previous bottom edge was below the platform's top edge
    // they were coming from underneath → let them pass through entirely.
    // The +1.f tolerance handles floating-point jitter when standing still.
    if (lp2.y + lp2.height > lp.y + 1.f) return result;

    // ── Step 4: Resolve — always a top landing ────────────────────────────
    // Since we only reach here when falling onto the surface, resolution is
    // simple: push the player up flush with the platform top.
    result.hit = true;
    result.grounded = true;
    result.side |= SIDE_TOP;

    // KEY SLIDE FIX: zero velocityY so the player doesn't sink into the
    // platform next frame. Without this, gravity re-pushes them in each frame,
    // the correction nudges them along the rotated normal (which has a
    // horizontal component), and that accumulates into visible sideways sliding.
    velocityY = 0.f;

    // Push up in local space, then rotate correction back to world space.
    Vector2 fix = { 0.f, lp.y - (lc.y + lc.height) };
    Vector2 wfix = ToWorld(fix, plat.rotation);
    player.x += wfix.x;
    player.y += wfix.y;

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
        if (r.hit) { merged.hit = true; merged.side |= r.side; merged.grounded |= r.grounded; }
    }
    return merged;
}

void CollisionManager::DrawAll(const vector<Platform>& platforms)
{
    for (const Platform& p : platforms) p.Draw();
}