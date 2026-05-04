// ============================================================
//  LevelData.cpp
// ============================================================
#include "LevelData.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <sys/stat.h>
#ifdef _WIN32
#  include <direct.h>
#  define MKDIR(p) _mkdir(p)
#else
#  define MKDIR(p) mkdir(p, 0755)
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────

static void EnsureDir(const char* folder)
{
    struct stat st = {};
    if (stat(folder, &st) != 0)
        MKDIR(folder);
}

static std::string LevelPath(int id, const char* folder)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s/level_%d.lvl", folder, id);
    return buf;
}

// ── Save ─────────────────────────────────────────────────────────────────────

bool SaveLevel(const LevelData& lv, const char* folder)
{
    EnsureDir(folder);
    std::string path = LevelPath(lv.id, folder);
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;

    fprintf(f, "LEVEL_ID %d\n", lv.id);

    if (lv.hasPlayerSpawn)
        fprintf(f, "PLAYER_SPAWN %.2f %.2f\n", lv.playerSpawn.x, lv.playerSpawn.y);
    if (lv.hasRegulus)
        fprintf(f, "REGULUS %.2f %.2f\n", lv.regulusPos.x, lv.regulusPos.y);
    if (lv.hasCave)
        fprintf(f, "CAVE %.2f %.2f\n", lv.cavePos.x, lv.cavePos.y);

    for (const auto& p : lv.platforms)
        fprintf(f, "PLATFORM %.2f %.2f %.2f %.2f %.2f\n", p.x, p.y, p.w, p.h, p.tilt);

    for (const auto& l : lv.ladders)
        fprintf(f, "LADDER %.2f %.2f %.2f %.2f\n", l.x, l.y, l.w, l.h);

    for (const auto& b : lv.beams)
        fprintf(f, "BEAM %.2f %.2f\n", b.x, b.y);

    for (const auto& n : lv.pathNodes)
        fprintf(f, "PATH_NODE %.2f %.2f %d %d %d %d\n",
            n.x, n.y, n.next[0], n.next[1], n.rollThreshold, n.isSplitNode ? 1 : 0);

    for (const auto& v : lv.nukeSpawns)
        fprintf(f, "NUKE_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& v : lv.beatriceSpawns)
        fprintf(f, "BEATRICE_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& v : lv.enemySpawns)
        fprintf(f, "ENEMY_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& e : lv.elevators)
        fprintf(f, "ELEVATOR %.2f %.2f %.2f %.2f %.2f %d\n",
            e.x, e.y, e.w, e.h, e.speed, e.direction);

    for (const auto& r : lv.relations)
        fprintf(f, "RELATION %d %d %d %d %.2f %.2f\n",
            r.parent.type, r.parent.index, r.child.type, r.child.index,
            r.offsetX, r.offsetY);

    if (lv.hasWinZone)
        fprintf(f, "WIN_ZONE %.2f %.2f %.2f %.2f\n",
            lv.winZone.x, lv.winZone.y, lv.winZone.w, lv.winZone.h);

    for (const auto& kz : lv.killZones)
        fprintf(f, "KILL_ZONE %.2f %.2f %.2f %.2f %d %.2f\n",
            kz.x, kz.y, kz.w, kz.h, (int)kz.texId, kz.tilt);

    fclose(f);
    return true;
}

// ── Load ─────────────────────────────────────────────────────────────────────

bool LoadLevel(LevelData& out, int id, const char* folder)
{
    std::string path = LevelPath(id, folder);
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;

    out = LevelData{};
    out.id = id;

    char tag[64] = {};
    while (fscanf(f, "%63s", tag) == 1)
    {
        if (strcmp(tag, "LEVEL_ID") == 0)
            (void)fscanf(f, "%d", &out.id);
        else if (strcmp(tag, "PLAYER_SPAWN") == 0) {
            (void)fscanf(f, "%f %f", &out.playerSpawn.x, &out.playerSpawn.y);
            out.hasPlayerSpawn = true;
        }
        else if (strcmp(tag, "REGULUS") == 0) {
            (void)fscanf(f, "%f %f", &out.regulusPos.x, &out.regulusPos.y);
            out.hasRegulus = true;
        }
        else if (strcmp(tag, "CAVE") == 0) {
            (void)fscanf(f, "%f %f", &out.cavePos.x, &out.cavePos.y);
            out.hasCave = true;
        }
        else if (strcmp(tag, "PLATFORM") == 0) {
            PlatformData p;
            (void)fscanf(f, "%f %f %f %f %f", &p.x, &p.y, &p.w, &p.h, &p.tilt);
            out.platforms.push_back(p);
        }
        else if (strcmp(tag, "LADDER") == 0) {
            LadderData l;
            (void)fscanf(f, "%f %f %f %f", &l.x, &l.y, &l.w, &l.h);
            out.ladders.push_back(l);
        }
        else if (strcmp(tag, "BEAM") == 0) {
            Vector2 b; (void)fscanf(f, "%f %f", &b.x, &b.y);
            out.beams.push_back(b);
        }
        else if (strcmp(tag, "PATH_NODE") == 0) {
            PathNodeData n; int split;
            (void)fscanf(f, "%f %f %d %d %d %d",
                &n.x, &n.y, &n.next[0], &n.next[1], &n.rollThreshold, &split);
            n.isSplitNode = (split != 0);
            out.pathNodes.push_back(n);
        }
        else if (strcmp(tag, "NUKE_SPAWN") == 0) {
            Vector2 v; (void)fscanf(f, "%f %f", &v.x, &v.y);
            out.nukeSpawns.push_back(v);
        }
        else if (strcmp(tag, "BEATRICE_SPAWN") == 0) {
            Vector2 v; (void)fscanf(f, "%f %f", &v.x, &v.y);
            out.beatriceSpawns.push_back(v);
        }
        else if (strcmp(tag, "ENEMY_SPAWN") == 0) {
            Vector2 v; (void)fscanf(f, "%f %f", &v.x, &v.y);
            out.enemySpawns.push_back(v);
        }
        else if (strcmp(tag, "ELEVATOR") == 0) {
            ElevatorData e;
            (void)fscanf(f, "%f %f %f %f %f %d", &e.x, &e.y, &e.w, &e.h, &e.speed, &e.direction);
            out.elevators.push_back(e);
        }
        else if (strcmp(tag, "RELATION") == 0) {
            ParentChildRelation r;
            (void)fscanf(f, "%d %d %d %d %f %f",
                &r.parent.type, &r.parent.index,
                &r.child.type, &r.child.index,
                &r.offsetX, &r.offsetY);
            out.relations.push_back(r);
        }
        else if (strcmp(tag, "WIN_ZONE") == 0) {
            (void)fscanf(f, "%f %f %f %f",
                &out.winZone.x, &out.winZone.y,
                &out.winZone.w, &out.winZone.h);
            out.hasWinZone = true;
        }
        else if (strcmp(tag, "KILL_ZONE") == 0) {
            KillZoneData kz; int texId = 0;
            int parsed = fscanf(f, "%f %f %f %f %d %f", &kz.x, &kz.y, &kz.w, &kz.h, &texId, &kz.tilt);
            if (parsed < 6) kz.tilt = 0.f;   // backward-compat: old files have no tilt
            kz.texId = (KillZoneTexture)texId;
            out.killZones.push_back(kz);
        }
        // unknown tag: skip rest of line
        else { char buf[512]; fgets(buf, sizeof(buf), f); }
    }

    fclose(f);
    out.valid = true;
    return true;
}

// ── Default level 1 (verbatim from main_patch.cpp) ───────────────────────────

LevelData GetDefaultLevel1()
{
    LevelData lv;
    lv.id = 1;
    lv.valid = true;

    lv.hasPlayerSpawn = true;
    lv.playerSpawn = { 35.0f + 64.0f * 3.5f + 10.0f, 817.0f };   // = 269

    lv.hasRegulus = true;
    lv.regulusPos = { 22.0f, 225.0f };

    lv.hasCave = true;
    lv.cavePos = { 35.0f, 768.0f };   // houseX, houseY (880 - 32*3.5)

    // ── Platforms ────────────────────────────────────────────────────────────
    lv.platforms = {
        { 27,  880, 412, 0,  0.0f },
        { 430, 870, 420, 0, -3.0f },
        { 60,  750, 720, 0,  3.0f },
        { 110, 620, 720, 0, -3.0f },
        { 60,  490, 720, 0,  3.0f },
        { 110, 360, 720, 0, -3.0f },
        { 460, 246, 320, 0,  3.0f },
        { 60,  240, 400, 0,  0.0f },
    };

    // ── Ladders ──────────────────────────────────────────────────────────────
    lv.ladders = {
        { 675, 245, 40, 104 },
        { 160, 375, 40, 102 },
        { 300, 365, 40, 117 },
        { 680, 495, 40, 110 },
        { 430, 489, 40, 128 },
        { 380, 621, 40, 124 },
        { 160, 632, 40, 101 },
        { 670, 760, 40, 105 },
    };

    // ── Barrel path (28 nodes) ────────────────────────────────────────────────
    lv.pathNodes = {
        { 125, 210, { 1,-1},  9, false },
        { 438, 210, { 2,-1},  5, false },
        { 680, 219, { 3, 4},  5, true  },
        { 680, 319, { 6,-1},  5, false },
        { 780, 224, { 5,-1}, 10, true  },
        { 800, 313, { 6,-1},  5, false },
        { 550, 326, { 7,-1},  5, false },
        { 305, 339, { 8, 9},  5, true  },
        { 305, 454, {11,-1},  5, false },
        { 110, 349, {10,-1}, 10, true  },
        {  70, 442, {11,-1},  5, false },
        { 430, 461, {12,-1},  5, false },
        { 685, 474, {13,14},  5, true  },
        { 685, 579, {16,-1},  5, false },
        { 780, 479, {15,-1}, 10, true  },
        { 800, 573, {16,-1},  5, false },
        { 550, 586, {17,-1},  5, false },
        { 165, 606, {18,19},  5, true  },
        { 165, 707, {21,-1},  5, false },
        { 110, 609, {20,-1}, 10, true  },
        {  70, 702, {21,-1},  5, false },
        { 430, 721, {22,-1},  5, false },
        { 675, 733, {23,24},  5, true  },
        { 675, 838, {26,-1},  5, false },
        { 780, 739, {25,-1}, 10, true  },
        { 800, 832, {26,-1},  5, false },
        { 400, 850, {27,-1},  5, false },
        { 148, 850, {-1,-1},  5, false },
    };

    // ── Beam positions (decorative) ───────────────────────────────────────────
    lv.beams = {
        {  50,225},{114,225},{178,225},{212,225},{276,225},{340,225},
        {372,225},{436,230},{468,230},{532,235},{564,235},{628,240},
        {660,240},{724,245},
        {110,365},{142,365},{206,360},{238,360},{302,355},{334,355},
        {398,350},{430,350},{494,345},{526,345},{590,340},{622,340},
        {686,335},{718,335},{782,330},
        { 54,460},{ 86,460},{150,465},{182,465},{246,470},{278,470},
        {342,475},{374,475},{438,480},{470,480},{534,485},{566,485},
        {630,490},{662,490},{726,495},
        {110,625},{142,625},{206,620},{238,620},{302,615},{334,615},
        {398,605},{430,605},{494,600},{526,600},{590,595},{622,595},
        {686,590},{718,590},{782,585},
        { 54,715},{ 86,715},{150,720},{182,720},{246,725},{278,725},
        {342,730},{374,730},{438,735},{470,735},{534,740},{566,740},
        {630,745},{662,745},{726,750},
        { 30,865},{ 94,865},{158,865},{192,865},{256,865},{320,865},
        {352,865},{416,860},{448,860},{512,855},{544,855},{608,850},
        {640,850},{704,845},{768,845},
        {360,120},{424,120},{456,120},{296,150},{264,150},
    };

    // ── Nuke spawns ───────────────────────────────────────────────────────────
    lv.nukeSpawns = {
        {150,845},{330,845},{180,693},{480,693},{650,693},
        {250,563},{570,563},{150,433},{500,433},{300,303},
    };

    // ── Beatrice spawns ───────────────────────────────────────────────────────
    lv.beatriceSpawns = {
        {250,750},{500,750},{150,620},{420,620},{660,620},
        {200,490},{480,490},{310,360},{560,360},
    };

    // ── Enemy spawns ──────────────────────────────────────────────────────────
    lv.enemySpawns = {
        {180,706},{620,706},{200,576},{550,576},
        {160,446},{520,446},{200,316},{500,316},
    };

    // ── Win zone (top of the level, near Regulus) ─────────────────────────────
    lv.hasWinZone = true;
    lv.winZone = { 22.f, 180.f, 160.f, 60.f };

    return lv;
}

// ── C++ export ────────────────────────────────────────────────────────────────

void ExportLevelAsCpp(const LevelData& lv, const char* outFile)
{
    FILE* f = fopen(outFile, "w");
    if (!f) return;

    fprintf(f, "// ── Auto-generated level %d ─────────────────────────────\n\n", lv.id);

    if (lv.hasPlayerSpawn)
        fprintf(f, "// Player spawn\nRectangle player = { %.2ff, %.2ff, 63, 63 };\n\n",
            lv.playerSpawn.x, lv.playerSpawn.y);

    fprintf(f, "// Platforms\nvector<Platform> platforms = {\n");
    for (const auto& p : lv.platforms)
        fprintf(f, "    Platform::Make(%.0f, %.0f, %.0f, %.0f, %.1ff),\n",
            p.x, p.y, p.w, p.h, p.tilt);
    fprintf(f, "};\n\n");

    fprintf(f, "// Ladders\nvector<Ladder> ladders = {\n");
    for (const auto& l : lv.ladders)
        fprintf(f, "    Ladder::Make(%.0f, %.0f, %.0f, %.0f),\n",
            l.x, l.y, l.w, l.h);
    fprintf(f, "};\n\n");

    fprintf(f, "// Barrel path\nvector<PathNode> barrelPath = {\n");
    for (int i = 0; i < (int)lv.pathNodes.size(); i++) {
        const auto& n = lv.pathNodes[i];
        fprintf(f, "    /* %2d */ { {%.0f,%.0f}, {%2d,%2d}, %2d, %s },\n",
            i, n.x, n.y, n.next[0], n.next[1], n.rollThreshold,
            n.isSplitNode ? "true " : "false");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "// Beam positions\nvector<Vector2> beamPositions = {\n");
    for (int i = 0; i < (int)lv.beams.size(); i++) {
        fprintf(f, "    { %.0f, %.0f },", lv.beams[i].x, lv.beams[i].y);
        if ((i + 1) % 6 == 0) fprintf(f, "\n");
    }
    fprintf(f, "\n};\n\n");

    fprintf(f, "// Nuke spawns\nvector<Vector2> nukeSpawnNodes = {\n");
    for (const auto& v : lv.nukeSpawns)
        fprintf(f, "    { %.0f, %.0f },\n", v.x, v.y);
    fprintf(f, "};\n\n");

    fprintf(f, "// Beatrice spawns\nvector<Vector2> beatriceSpawnNodes = {\n");
    for (const auto& v : lv.beatriceSpawns)
        fprintf(f, "    { %.0f, %.0f },\n", v.x, v.y);
    fprintf(f, "};\n\n");

    fprintf(f, "// Enemy spawns\nvector<Vector2> enemySpawnPositions = {\n");
    for (const auto& v : lv.enemySpawns)
        fprintf(f, "    { %.0f, %.0f },\n", v.x, v.y);
    fprintf(f, "};\n\n");

    fclose(f);
}