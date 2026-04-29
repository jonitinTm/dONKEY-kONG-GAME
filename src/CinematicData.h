#pragma once
// ============================================================
//  CinematicData.h  —  cinematic sequence data + serialisation
// ============================================================
#include <vector>
#include <string>

// ── One captured keyframe on a track ──────────────────────────────────────────
struct CinematicKeyframe {
    float time   = 0.f;   // position on timeline (seconds)
    float x      = 0.f;
    float y      = 0.f;
    float tilt   = 0.f;   // platform rotation (degrees)
    float width  = 0.f;   // platform/ladder width
    float height = 0.f;   // ladder height
};

// ── One entity track (one entity animated per track) ──────────────────────────
struct CinematicTrack {
    int  entityType  = -1;   // matches EditorTool int
    int  entityIndex = -1;
    char name[48]    = {};
    std::vector<CinematicKeyframe> keys;
};

// ── End-of-sequence behaviour ────────────────────────────────────────────────
enum class CinematicEndMode : int { LOOP = 0, STAY = 1, RESET = 2 };

// ── Full named sequence ───────────────────────────────────────────────────────
struct CinematicSequence {
    char             name[64]  = {};
    float            duration  = 5.f;
    CinematicEndMode endMode   = CinematicEndMode::LOOP;
    bool             valid     = false;
    std::vector<CinematicTrack> tracks;
};

// ── Interpolated entity state at time t ──────────────────────────────────────
struct CinematicEntityState {
    int   entityType  = -1;
    int   entityIndex = -1;
    float x = 0.f, y = 0.f;
    float tilt = 0.f, width = 0.f, height = 0.f;
};

// ── Persistence ───────────────────────────────────────────────────────────────
bool                     SaveCinematic(const CinematicSequence& seq,
                                       const char* folder = "Cinematics");
bool                     LoadCinematic(CinematicSequence& out,
                                       const char* name,
                                       const char* folder = "Cinematics");
bool                     DeleteCinematic(const char* name,
                                         const char* folder = "Cinematics");
std::vector<std::string> ListCinematics(const char* folder = "Cinematics");

// ── Evaluate all tracks at time t → list of states to apply ──────────────────
std::vector<CinematicEntityState> EvaluateCinematic(const CinematicSequence& seq,
                                                     float t);
