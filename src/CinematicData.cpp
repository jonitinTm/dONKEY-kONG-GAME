// ============================================================
//  CinematicData.cpp
// ============================================================
#include "CinematicData.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <sys/stat.h>
#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  define MKDIR(p) mkdir(p,0755)
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static void EnsureDir(const char* folder){
    struct stat st={};
    if(stat(folder,&st)!=0) MKDIR(folder);
}

// Sanitise a sequence name for use as a filename
static void SafeName(const char* src, char* dst, int dstLen){
    int j=0;
    for(int i=0;src[i]&&j<dstLen-1;i++){
        char c=src[i];
        dst[j++]=(c==' '||c=='/'||c=='\\'||c==':')?'_':c;
    }
    dst[j]='\0';
}

static std::string SeqPath(const char* name, const char* folder){
    char safe[64]; SafeName(name,safe,sizeof(safe));
    char buf[512]; snprintf(buf,sizeof(buf),"%s/%s.cin",folder,safe);
    return buf;
}
static std::string ManifestPath(const char* folder){
    return std::string(folder)+"/manifest.txt";
}

// ─────────────────────────────────────────────────────────────────────────────
//  Manifest helpers
// ─────────────────────────────────────────────────────────────────────────────
static std::vector<std::string> ReadManifest(const char* folder){
    std::vector<std::string> names;
    FILE* f=fopen(ManifestPath(folder).c_str(),"r");
    if(!f) return names;
    char buf[64];
    while(fscanf(f,"%63s",buf)==1) names.push_back(buf);
    fclose(f); return names;
}
static void WriteManifest(const char* folder, const std::vector<std::string>& names){
    FILE* f=fopen(ManifestPath(folder).c_str(),"w");
    if(!f) return;
    for(const auto& n:names) fprintf(f,"%s\n",n.c_str());
    fclose(f);
}
static void ManifestAdd(const char* folder, const char* name){
    auto names=ReadManifest(folder);
    for(const auto& n:names) if(n==name) return; // already present
    names.push_back(name);
    WriteManifest(folder,names);
}
static void ManifestRemove(const char* folder, const char* name){
    auto names=ReadManifest(folder);
    names.erase(std::remove(names.begin(),names.end(),std::string(name)),names.end());
    WriteManifest(folder,names);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Save
// ─────────────────────────────────────────────────────────────────────────────
bool SaveCinematic(const CinematicSequence& seq, const char* folder){
    EnsureDir(folder);
    FILE* f=fopen(SeqPath(seq.name,folder).c_str(),"w");
    if(!f) return false;
    fprintf(f,"CINEMATIC_VERSION 1\n");
    fprintf(f,"NAME %s\n",seq.name);
    fprintf(f,"DURATION %.4f\n",seq.duration);
    fprintf(f,"END_MODE %d\n",(int)seq.endMode);
    for(const auto& tr:seq.tracks){
        fprintf(f,"TRACK %d %d %s\n",tr.entityType,tr.entityIndex,tr.name);
        for(const auto& k:tr.keys)
            fprintf(f,"KEY %.5f %.3f %.3f %.3f %.3f %.3f\n",
                    k.time,k.x,k.y,k.tilt,k.width,k.height);
        fprintf(f,"TRACK_END\n");
    }
    fprintf(f,"END\n");
    fclose(f);
    ManifestAdd(folder,seq.name);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Load
// ─────────────────────────────────────────────────────────────────────────────
bool LoadCinematic(CinematicSequence& out, const char* name, const char* folder){
    FILE* f=fopen(SeqPath(name,folder).c_str(),"r");
    if(!f) return false;
    out=CinematicSequence{};
    char tag[64];
    while(fscanf(f,"%63s",tag)==1){
        if     (strcmp(tag,"NAME")==0)     fscanf(f,"%63s",out.name);
        else if(strcmp(tag,"DURATION")==0) fscanf(f,"%f",&out.duration);
        else if(strcmp(tag,"END_MODE")==0){int em;fscanf(f,"%d",&em);out.endMode=(CinematicEndMode)em;}
        else if(strcmp(tag,"TRACK")==0){
            CinematicTrack tr;
            fscanf(f,"%d %d %47s",&tr.entityType,&tr.entityIndex,tr.name);
            char t2[64];
            while(fscanf(f,"%63s",t2)==1){
                if(strcmp(t2,"TRACK_END")==0) break;
                if(strcmp(t2,"KEY")==0){
                    CinematicKeyframe k;
                    fscanf(f,"%f %f %f %f %f %f",&k.time,&k.x,&k.y,&k.tilt,&k.width,&k.height);
                    tr.keys.push_back(k);
                }
            }
            std::sort(tr.keys.begin(),tr.keys.end(),[](const CinematicKeyframe& a,const CinematicKeyframe& b){return a.time<b.time;});
            out.tracks.push_back(tr);
        }
        else if(strcmp(tag,"END")==0) break;
        else{char skip[512];fgets(skip,sizeof(skip),f);}
    }
    fclose(f);
    out.valid=true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Delete
// ─────────────────────────────────────────────────────────────────────────────
bool DeleteCinematic(const char* name, const char* folder){
    std::string p=SeqPath(name,folder);
    ManifestRemove(folder,name);
    return remove(p.c_str())==0;
}

// ─────────────────────────────────────────────────────────────────────────────
//  List
// ─────────────────────────────────────────────────────────────────────────────
std::vector<std::string> ListCinematics(const char* folder){
    return ReadManifest(folder);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Evaluate — linear interpolation between adjacent keyframes
// ─────────────────────────────────────────────────────────────────────────────
std::vector<CinematicEntityState> EvaluateCinematic(const CinematicSequence& seq, float t){
    std::vector<CinematicEntityState> result;
    for(const auto& tr:seq.tracks){
        if(tr.keys.empty()) continue;
        CinematicEntityState st;
        st.entityType=tr.entityType; st.entityIndex=tr.entityIndex;
        auto Apply=[&](const CinematicKeyframe& k){
            st.x=k.x; st.y=k.y; st.tilt=k.tilt; st.width=k.width; st.height=k.height;
        };
        if(t<=tr.keys.front().time){
            Apply(tr.keys.front());
        } else if(t>=tr.keys.back().time){
            Apply(tr.keys.back());
        } else {
            for(int i=0;i<(int)tr.keys.size()-1;i++){
                const auto& a=tr.keys[i]; const auto& b=tr.keys[i+1];
                if(t>=a.time&&t<b.time){
                    float al=(t-a.time)/(b.time-a.time);
                    // Smoothstep easing
                    float s=al*al*(3.f-2.f*al);
                    st.x    =a.x    +(b.x    -a.x    )*s;
                    st.y    =a.y    +(b.y    -a.y    )*s;
                    st.tilt =a.tilt +(b.tilt -a.tilt )*s;
                    st.width=a.width+(b.width-a.width)*s;
                    st.height=a.height+(b.height-a.height)*s;
                    break;
                }
            }
        }
        result.push_back(st);
    }
    return result;
}
