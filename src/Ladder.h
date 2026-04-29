#pragma once
#include "raylib.h"
#include <vector>
using std::vector;

// ─────────────────────────────────────────────────────────────────────────────
//  Ladder
//  · y      = screen-top of the ladder  (smaller y → higher on screen)
//  · height = total visual pixel length  (texture reaches the floor)
//  · The BOTTOM_TRIM bottom pixels are decorative only — the interactive /
//    climbable zone stops above them so the player never phases through floor.
//  · progress 0.0 = player bottom is flush with the interactive bottom
//  · progress 1.0 = player bottom is flush with ladder top (upper platform)
// ─────────────────────────────────────────────────────────────────────────────
struct Ladder
{
    float x;
    float y;        // top of ladder in screen space
    float width;
    float height;   // TOTAL visual height (includes decorative bottom trim)

    // Bottom pixels that are visual-only — no interaction, no player stop here.
    static constexpr float BOTTOM_TRIM = 18.f;

    // Effective climbable height (shorter than the visual height)
    float ClimbHeight() const { return height - BOTTOM_TRIM; }

    // ── Factory ───────────────────────────────────────────────────────────
    static Ladder Make(float x, float y, float w, float h)
    {
        return { x, y, w, h };
    }

    // ── Hitbox — only covers the interactive (climbable) zone ─────────────
    Rectangle GetHitbox() const
    {
        return { x, y, width, ClimbHeight() };
    }

    // ── Player world-y when at a given progress ───────────────────────────
    //  Uses ClimbHeight so progress=0 lands the player at the trim boundary,
    //  not at the visual floor — preventing floor phase-through.
    float PlayerYAtProgress(float progress, float playerHeight) const
    {
        return y + ClimbHeight() * (1.0f - progress) - playerHeight;
    }

    // ── Infer progress from player's current y ────────────────────────────
    float ProgressFromPlayerY(float playerY, float playerHeight) const
    {
        return 1.0f - (playerY + playerHeight - y) / ClimbHeight();
    }

    // ── Optional debug draw (comment out in release) ──────────────────────
    void DrawDebug() const
    {
        // Interactive zone in green
        DrawRectangleLinesEx(GetHitbox(), 2, { 0, 230, 60, 160 });
        // Decorative trim in orange
        DrawRectangleLinesEx({ x, y + ClimbHeight(), width, BOTTOM_TRIM }, 1, { 255, 140, 0, 120 });
    }
};