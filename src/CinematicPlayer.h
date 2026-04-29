#pragma once
// ============================================================
//  CinematicPlayer.h  —  runtime cinematic playback
//
//  Usage in main.cpp:
//      CinematicPlayer cinema;
//      cinema.LoadAll();                     // load all from Cinematics/
//
//      cinema.Play("MyIntro");               // start a sequence by name
//      // or: CinematicSequencer::Play("MyIntro") if you prefer a namespace
//
//      // In update loop:
//      cinema.Update(dt, level);             // applies states to LevelData
//
//      // Optional: direct callback (move live game objects instead of LevelData)
//      cinema.SetApplyCallback([&](const CinematicEntityState& s){ ... });
// ============================================================
#include "CinematicData.h"
#include "LevelData.h"
#include <functional>

class CinematicPlayer
{
public:
    /// Load all sequences from folder (called once at startup, or after editing)
    void LoadAll(const char* folder = "Cinematics");

    /// Start a named sequence. Returns false if not found.
    bool Play(const char* name);

    void Pause()  { _paused = !_paused; }
    void Resume() { _paused = false; }
    void Stop()   { _current = -1; _time = 0.f; _paused = false; }

    bool IsPlaying()  const { return _current >= 0 && !_paused; }
    bool IsPaused()   const { return _current >= 0 && _paused; }
    bool HasSequence(const char* name) const;

    const char* CurrentName() const;
    float       CurrentTime() const { return _time; }
    float       Duration()    const;

    /// Call every frame in the update loop.
    /// Applies interpolated entity states directly to the LevelData snapshot;
    /// your game then reads from that LevelData for positions/rendering.
    void Update(float dt, LevelData& lv);

    /// Optional direct callback — called for each evaluated entity state.
    /// Useful when you want to move live Platform/Ladder objects directly.
    using ApplyFn = std::function<void(const CinematicEntityState&)>;
    void SetApplyCallback(ApplyFn fn) { _applyFn = fn; }
    void ClearCallback()              { _applyFn = nullptr; }

    int                    Count()     const { return (int)_seqs.size(); }
    const CinematicSequence& GetAt(int i) const { return _seqs[i]; }

private:
    std::vector<CinematicSequence> _seqs;
    int   _current = -1;
    float _time    = 0.f;
    bool  _paused  = false;
    ApplyFn _applyFn;

    void ApplyState(const CinematicEntityState& st, LevelData& lv);
};

// ── Convenience namespace so you can write: Cinematic::Play("Name") ──────────
namespace Cinematic {
    extern CinematicPlayer Global;   // define in your .cpp: CinematicPlayer Cinematic::Global;
    inline bool Play(const char* name) { return Global.Play(name); }
    inline void Stop()                 { Global.Stop(); }
    inline bool IsPlaying()            { return Global.IsPlaying(); }
}
