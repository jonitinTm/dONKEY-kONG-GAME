#include "Collision.h"

Vector2 CollisionManager::ToLocal(Vector2 worldPoint, Vector2 center, float rotDeg)
{
    float rad  = -rotDeg * (3.14159265f / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    float dx   = worldPoint.x - center.x;
    float dy   = worldPoint.y - center.y;
    return { dx * cosA - dy * sinA, dx * sinA + dy * cosA };
}

Vector2 CollisionManager::RotateToWorld(Vector2 localDir, float rotDeg)
{
    float rad  = rotDeg * (3.14159265f / 180.0f);
    float cosA = cosf(rad);
    float sinA = sinf(rad);
    return {
        localDir.x * cosA - localDir.y * sinA,
        localDir.x * sinA + localDir.y * cosA
    };
}

bool CollisionManager::PrevOverlapX(Rectangle prev, Rectangle lp)
{
    return !(prev.x + prev.width <= lp.x || prev.x >= lp.x + lp.width);
}

bool CollisionManager::PrevOverlapY(Rectangle prev, Rectangle lp)
{
    return !(prev.y + prev.height <= lp.y || prev.y >= lp.y + lp.height);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Resolve – single platform
// ─────────────────────────────────────────────────────────────────────────────
CollisionResult CollisionManager::Resolve(
    Rectangle      &player,
    float          &velocityX,
    float          &velocityY,
    const Platform &platform,
    float           prevX,
    float           prevY)
{
    CollisionResult result;

    Vector2 platCenter = platform.Center();

    // Current player center → local space
    Vector2 curWorldCenter = { player.x + player.width * 0.5f, player.y + player.height * 0.5f };
    Vector2 localCur       = ToLocal(curWorldCenter, platCenter, platform.rotation);

    // Platform as axis-aligned rect centered at origin in local space
    Rectangle localPlat = {
        -platform.width  * 0.5f,
        -platform.height * 0.5f,
         platform.width,
         platform.height
    };

    // Player rect in local space
    Rectangle localPlayer = {
        localCur.x - player.width  * 0.5f,
        localCur.y - player.height * 0.5f,
        player.width,
        player.height
    };

    if (!CheckCollisionRecs(localPlayer, localPlat))
        return result;

    result.hit = true;

    // Previous player center → local space
    Vector2 prevWorldCenter = { prevX + player.width * 0.5f, prevY + player.height * 0.5f };
    Vector2 localPrev       = ToLocal(prevWorldCenter, platCenter, platform.rotation);

    Rectangle localPrevPlayer = {
        localPrev.x - player.width  * 0.5f,
        localPrev.y - player.height * 0.5f,
        player.width,
        player.height
    };

    bool prevOX = PrevOverlapX(localPrevPlayer, localPlat);
    bool prevOY = PrevOverlapY(localPrevPlayer, localPlat);

    // Velocity in local space
    Vector2 localVel = ToLocal({ velocityX, velocityY }, { 0.0f, 0.0f }, platform.rotation);

    Vector2 correction = { 0.0f, 0.0f };

    // ── Entered from top or bottom ────────────────────────────────────────
    if (prevOX && !prevOY)
    {
        if (localVel.y >= 0.0f)  // landed on TOP
        {
            correction.y    = localPlat.y - (localPlayer.y + localPlayer.height);
            result.side    |= SIDE_TOP;
            result.grounded = true;

            Vector2 normal = RotateToWorld({ 0.0f, -1.0f }, platform.rotation);
            float   dot    = velocityX * normal.x + velocityY * normal.y;
            if (dot < 0.0f) { velocityX -= dot * normal.x; velocityY -= dot * normal.y; }
        }
        else  // hit BOTTOM (ceiling)
        {
            correction.y = (localPlat.y + localPlat.height) - localPlayer.y;
            result.side |= SIDE_BOTTOM;

            Vector2 normal = RotateToWorld({ 0.0f, 1.0f }, platform.rotation);
            float   dot    = velocityX * normal.x + velocityY * normal.y;
            if (dot < 0.0f) { velocityX -= dot * normal.x; velocityY -= dot * normal.y; }
        }
    }
    // ── Entered from left or right ────────────────────────────────────────
    else if (prevOY && !prevOX)
    {
        if (localVel.x >= 0.0f)  // hit LEFT wall
        {
            correction.x = localPlat.x - (localPlayer.x + localPlayer.width);
            result.side |= SIDE_LEFT;
        }
        else  // hit RIGHT wall
        {
            correction.x = (localPlat.x + localPlat.width) - localPlayer.x;
            result.side |= SIDE_RIGHT;
        }

        Vector2 normal = RotateToWorld({ (correction.x >= 0.0f ? 1.0f : -1.0f), 0.0f }, platform.rotation);
        float   dot    = velocityX * normal.x + velocityY * normal.y;
        if (dot < 0.0f) { velocityX -= dot * normal.x; velocityY -= dot * normal.y; }
    }
    // ── Fallback: corner / fast movement ─────────────────────────────────
    else
    {
        float penY = fabsf(localPlat.y - (localPlayer.y + localPlayer.height));
        float penX = fabsf(localPlat.x - (localPlayer.x + localPlayer.width));

        if (penY <= penX)
        {
            correction.y    = localPlat.y - (localPlayer.y + localPlayer.height);
            result.side    |= SIDE_TOP;
            result.grounded = true;

            Vector2 normal = RotateToWorld({ 0.0f, -1.0f }, platform.rotation);
            float   dot    = velocityX * normal.x + velocityY * normal.y;
            if (dot < 0.0f) { velocityX -= dot * normal.x; velocityY -= dot * normal.y; }
        }
        else
        {
            correction.x = localPlat.x - (localPlayer.x + localPlayer.width);
            result.side |= SIDE_LEFT;

            Vector2 normal = RotateToWorld({ -1.0f, 0.0f }, platform.rotation);
            float   dot    = velocityX * normal.x + velocityY * normal.y;
            if (dot < 0.0f) { velocityX -= dot * normal.x; velocityY -= dot * normal.y; }
        }
    }

    // Rotate correction back to world space and apply
    Vector2 worldCorrection = RotateToWorld(correction, platform.rotation);
    player.x += worldCorrection.x;
    player.y += worldCorrection.y;

    return result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ResolveAll
// ─────────────────────────────────────────────────────────────────────────────
CollisionResult CollisionManager::ResolveAll(
    Rectangle        &player,
    float            &velocityX,
    float            &velocityY,
    vector<Platform> &platforms,
    float             prevX,
    float             prevY)
{
    CollisionResult merged;

    for (Platform &p : platforms)
    {
        CollisionResult r = Resolve(player, velocityX, velocityY, p, prevX, prevY);
        if (r.hit)
        {
            merged.hit      = true;
            merged.side    |= r.side;
            merged.grounded |= r.grounded;
        }
    }

    return merged;
}

// ─────────────────────────────────────────────────────────────────────────────
//  DrawAll
// ─────────────────────────────────────────────────────────────────────────────
void CollisionManager::DrawAll(const vector<Platform> &platforms)
{
    for (const Platform &p : platforms)
        p.Draw();
}
