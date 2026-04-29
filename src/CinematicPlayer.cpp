// ============================================================
//  CinematicPlayer.cpp
// ============================================================
#include "CinematicPlayer.h"
#include <cstring>
#include <cmath>

// ── Global convenience instance ───────────────────────────────────────────────
namespace Cinematic { CinematicPlayer Global; }

// ── EditorTool enum values (duplicated to avoid circular dep) ─────────────────
static constexpr int ET_PLAYER  = 1;
static constexpr int ET_REGULUS = 2;
static constexpr int ET_CAVE    = 3;
static constexpr int ET_PLAT    = 4;
static constexpr int ET_LAD     = 5;
static constexpr int ET_BEAM    = 6;
static constexpr int ET_NODE    = 7;
static constexpr int ET_NUKE    = 8;
static constexpr int ET_BEAT    = 9;
static constexpr int ET_ENEMY   = 10;

// ─────────────────────────────────────────────────────────────────────────────

void CinematicPlayer::LoadAll(const char* folder){
    _seqs.clear();
    auto names = ListCinematics(folder);
    for(const auto& n : names){
        CinematicSequence seq;
        if(LoadCinematic(seq, n.c_str(), folder))
            _seqs.push_back(seq);
    }
}

bool CinematicPlayer::HasSequence(const char* name) const {
    for(const auto& s : _seqs) if(strcmp(s.name,name)==0) return true;
    return false;
}

bool CinematicPlayer::Play(const char* name){
    for(int i=0;i<(int)_seqs.size();i++){
        if(strcmp(_seqs[i].name,name)==0){
            _current=i; _time=0.f; _paused=false;
            return true;
        }
    }
    return false;  // not found — call LoadAll() first
}

const char* CinematicPlayer::CurrentName() const {
    if(_current<0||_current>=(int)_seqs.size()) return "";
    return _seqs[_current].name;
}

float CinematicPlayer::Duration() const {
    if(_current<0||_current>=(int)_seqs.size()) return 0.f;
    return _seqs[_current].duration;
}

void CinematicPlayer::Update(float dt, LevelData& lv){
    if(_current<0 || _paused) return;
    const auto& seq=_seqs[_current];

    _time += dt;

    float t = _time;
    if(_time >= seq.duration){
        switch(seq.endMode){
        case CinematicEndMode::LOOP:
            _time = fmodf(_time, seq.duration);
            t = _time;
            break;
        case CinematicEndMode::STAY:
            t = seq.duration;
            break;
        case CinematicEndMode::RESET:
            Stop();
            return;
        }
    }

    auto states = EvaluateCinematic(seq, t);
    for(const auto& st : states){
        if(_applyFn) _applyFn(st);
        else         ApplyState(st, lv);
    }
}

void CinematicPlayer::ApplyState(const CinematicEntityState& st, LevelData& lv){
    int i=st.entityIndex;
    switch(st.entityType){
    case ET_PLAT:
        if(i>=0&&i<(int)lv.platforms.size()){
            lv.platforms[i].x=st.x;
            lv.platforms[i].y=st.y;
            if(st.tilt!=0.f||st.width>0.f){
                lv.platforms[i].tilt=st.tilt;
                if(st.width>0.f) lv.platforms[i].w=st.width;
            }
        } break;
    case ET_LAD:
        if(i>=0&&i<(int)lv.ladders.size()){
            lv.ladders[i].x=st.x;
            lv.ladders[i].y=st.y;
            if(st.height>0.f) lv.ladders[i].h=st.height;
        } break;
    case ET_BEAM:
        if(i>=0&&i<(int)lv.beams.size())
            lv.beams[i]={st.x,st.y};
        break;
    case ET_NODE:
        if(i>=0&&i<(int)lv.pathNodes.size()){
            lv.pathNodes[i].x=st.x;
            lv.pathNodes[i].y=st.y;
        } break;
    case ET_PLAYER: lv.playerSpawn={st.x,st.y};     break;
    case ET_REGULUS: lv.regulusPos={st.x,st.y};     break;
    case ET_CAVE:    lv.cavePos={st.x,st.y};         break;
    case ET_NUKE:
        if(i>=0&&i<(int)lv.nukeSpawns.size())
            lv.nukeSpawns[i]={st.x,st.y};
        break;
    case ET_BEAT:
        if(i>=0&&i<(int)lv.beatriceSpawns.size())
            lv.beatriceSpawns[i]={st.x,st.y};
        break;
    case ET_ENEMY:
        if(i>=0&&i<(int)lv.enemySpawns.size())
            lv.enemySpawns[i]={st.x,st.y};
        break;
    }
}
