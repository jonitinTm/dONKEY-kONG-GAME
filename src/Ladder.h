#pragma once
#include "raylib.h"
#include <vector>
using std::vector;

// ─────────────────────────────────────────────────────────────────────────────
//  Ladder
//  · y      = screen-top of the ladder  (smaller y → higher on screen)
//  · height = pixel length of the climbable section
//  · progress 0.0 = player bottom is flush with ladder bottom (ground level)
//  · progress 1.0 = player bottom is flush with ladder top    (upper platform)
// ─────────────────────────────────────────────────────────────────────────────
struct Ladder
{
    float x;
    float y;        // top of ladder in screen space
    float width;
    float height;   // total climbable pixel length

    // ── Factory ───────────────────────────────────────────────────────────
    static Ladder Make(float x, float y, float w, float h)
    {
        return { x, y, w, h };
    }

    // ── Hitbox (no collision, only overlap detection) ─────────────────────
    Rectangle GetHitbox() const
    {
        return { x, y, width, height };
    }

    // ── Player world-y when at a given progress ───────────────────────────
    //  progress 0 → bottom: player.y = y + height - playerHeight
    //  progress 1 → top   : player.y = y            - playerHeight
    float PlayerYAtProgress(float progress, float playerHeight) const
    {
        return y + height * (1.0f - progress) - playerHeight;
    }

    // ── Infer progress from player's current y ────────────────────────────
    float ProgressFromPlayerY(float playerY, float playerHeight) const
    {
        return 1.0f - (playerY + playerHeight - y) / height;
    }

    // ── Optional debug draw (comment out in release) ──────────────────────
    void DrawDebug() const
    {
        DrawRectangleLinesEx(GetHitbox(), 2, { 0, 230, 60, 160 });
    }
};
