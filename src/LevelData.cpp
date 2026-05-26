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
        (void)MKDIR(folder);
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
    if (lv.hasCave) {
        fprintf(f, "CAVE %.2f %.2f %d\n", lv.cavePos.x, lv.cavePos.y, lv.caveVisible ? 1 : 0);
        if (lv.caveSpawnEnabled)
            fprintf(f, "CAVE_SPAWN %.2f\n", lv.caveSpawnRate);
        if (!lv.barrelEndSpawnBunnies)
            fprintf(f, "BARREL_BUNNY_SPAWN 0\n");
    }

    for (const auto& p : lv.platforms)
        fprintf(f, "PLATFORM %.2f %.2f %.2f %.2f %.2f\n", p.x, p.y, p.w, p.h, p.tilt);

    for (const auto& l : lv.ladders)
        fprintf(f, "LADDER %.2f %.2f %.2f %.2f\n", l.x, l.y, l.w, l.h);

    for (const auto& b : lv.beams)
        fprintf(f, "BEAM %.2f %.2f %d %d %d %d %d\n",
            b.x, b.y, b.texVariant, b.renderLayer, b.flipX ? 1 : 0, b.transparent ? 1 : 0, b.soundMaterial);

    for (const auto& n : lv.pathNodes)
        // 11th token = enderDir+2: 1=normal(-1), 2=down(0), 3=right(1), 4=left(2), 5=none(3)
        fprintf(f, "PATH_NODE %.2f %.2f %d %d %d %d %d %d %d %d %d\n",
            n.x, n.y, n.next[0], n.next[1], n.next[2],
            n.edgeType[0], n.edgeType[1], n.edgeType[2],
            n.rollThreshold, n.isSplitNode ? 1 : 0, n.enderDir + 2);

    for (const auto& v : lv.nukeSpawns)
        fprintf(f, "NUKE_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& v : lv.beatriceSpawns)
        fprintf(f, "BEATRICE_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& v : lv.enemySpawns)
        fprintf(f, "ENEMY_SPAWN %.2f %.2f\n", v.x, v.y);

    for (const auto& e : lv.elevators)
        fprintf(f, "ELEVATOR %.2f %.2f %.2f %.2f %.2f %d %d %d %d\n",
            e.x, e.y, e.w, e.h, e.speed, e.direction,
            e.invisible ? 1 : 0, e.horizontal ? 1 : 0, e.backAndForth ? 1 : 0);

    for (const auto& cv : lv.conveyors)
        fprintf(f, "CONVEYOR %.2f %.2f %.2f %.2f %d %.2f %.2f %.2f %d\n",
            cv.x, cv.y, cv.length, cv.speed, cv.direction,
            cv.rotation, cv.endCapW, cv.beltH, cv.soundMaterial);

    // PROP format (15 tokens):
    // x y width height rotation hasCollision lightAffect renderLayer texVariant
    // bobAmp bobSpeed flickerIntens flickerSpeed swayAngle swaySpeed
    for (const auto& pr : lv.props)
        fprintf(f, "PROP %.2f %.2f %.2f %.2f %.2f %d %.3f %d %d %.2f %.2f %.3f %.2f %.2f %.2f\n",
            pr.x, pr.y, pr.width, pr.height, pr.rotation,
            pr.hasCollision ? 1 : 0,
            pr.lightAffect, pr.renderLayer, pr.texVariant,
            pr.bobAmp, pr.bobSpeed, pr.flickerIntens, pr.flickerSpeed,
            pr.swayAngle, pr.swaySpeed);

    for (const auto& r : lv.relations)
        fprintf(f, "RELATION %d %d %d %d %.2f %.2f\n",
            r.parent.type, r.parent.index,
            r.child.type, r.child.index,
            r.offsetX, r.offsetY);

    if (lv.hasWinZone)
        fprintf(f, "WIN_ZONE %.2f %.2f %.2f %.2f\n",
            lv.winZone.x, lv.winZone.y, lv.winZone.w, lv.winZone.h);

    for (const auto& kz : lv.killZones)
        fprintf(f, "KILL_ZONE %.2f %.2f %.2f %.2f %d %.2f %d\n",
            kz.x, kz.y, kz.w, kz.h,
            static_cast<int>(kz.texId), kz.rotation, kz.renderLayer);

    // LIGHT format (18 tokens):
    // type x y r g b intensity radius innerRadius angle direction
    // bounces fogStrength enabled flickerFreq flickerAmp pulseFreq pulseAmp
    for (const auto& L : lv.lights)
        fprintf(f, "LIGHT %d %.2f %.2f %.4f %.4f %.4f %.3f %.2f %.2f %.2f %.2f %d %.3f %d %.3f %.3f %.3f %.3f\n",
            static_cast<int>(L.type), L.x, L.y, L.r, L.g, L.b,
            L.intensity, L.radius, L.innerRadius, L.angle, L.direction,
            L.bounces, L.fogStrength, L.enabled ? 1 : 0,
            L.flickerFreq, L.flickerAmp, L.pulseFreq, L.pulseAmp);

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
            int vis = 1;
            (void)fscanf(f, "%f %f %d", &out.cavePos.x, &out.cavePos.y, &vis);
            out.hasCave = true;
            out.caveVisible = (vis != 0);
        }
        else if (strcmp(tag, "CAVE_SPAWN") == 0) {
            (void)fscanf(f, "%f", &out.caveSpawnRate);
            out.caveSpawnEnabled = true;
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
            BeamData b;
            int flipXi = 0, transi = 0;
            int parsed = fscanf(f, "%f %f %d %d %d %d %d",
                &b.x, &b.y, &b.texVariant, &b.renderLayer, &flipXi, &transi, &b.soundMaterial);
            if (parsed < 3) b.texVariant = 0;
            if (parsed < 4) b.renderLayer = 0;
            b.flipX = (parsed >= 5 && flipXi != 0);
            b.transparent = (parsed >= 6 && transi != 0);
            if (parsed < 7) b.soundMaterial = 0;
            out.beams.push_back(b);
        }
        else if (strcmp(tag, "PATH_NODE") == 0) {
            char line[512]; fgets(line, sizeof(line), f);
            PathNodeData n; int roll = 5, split = 0, enderFall = 0;
            // Format v3 (11 values): x y next0 next1 next2 et0 et1 et2 roll isSplit isEnderFall
            // Format v2 (10 values): x y next0 next1 next2 et0 et1 et2 roll isSplit
            // Format v1 (6 values):  x y next0 next1 roll isSplit
            int cnt = sscanf(line, "%f %f %d %d %d %d %d %d %d %d %d",
                &n.x, &n.y, &n.next[0], &n.next[1], &n.next[2],
                &n.edgeType[0], &n.edgeType[1], &n.edgeType[2], &roll, &split, &enderFall);
            if (cnt == 6) {
                roll  = n.next[2];
                split = n.edgeType[0];
                n.next[2] = -1;
                n.edgeType[0] = n.edgeType[1] = n.edgeType[2] = 0;
            }
            n.rollThreshold = roll;
            n.isSplitNode   = (split != 0);
            // 11th token: 0=old-not-ender, 1=old-ender-down, 2+=new(raw-2)
            n.enderDir = (cnt >= 11) ? (enderFall <= 1 ? (enderFall - 1) : (enderFall - 2)) : -1;
            out.pathNodes.push_back(n);
        }
        else if (strcmp(tag, "BARREL_BUNNY_SPAWN") == 0) {
            int v = 1; (void)fscanf(f, "%d", &v);
            out.barrelEndSpawnBunnies = (v != 0);
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
            int inv = 0, horiz = 0, baf = 0;
            int cnt = fscanf(f, "%f %f %f %f %f %d %d %d %d",
                &e.x, &e.y, &e.w, &e.h, &e.speed, &e.direction, &inv, &horiz, &baf);
            e.invisible    = (cnt >= 7 && inv   != 0);
            e.horizontal   = (cnt >= 8 && horiz != 0);
            e.backAndForth = (cnt >= 9 && baf   != 0);
            out.elevators.push_back(e);
        }
        else if (strcmp(tag, "CONVEYOR") == 0) {
            ConveyorData cv;
            int cnt = fscanf(f, "%f %f %f %f %d %f %f %f %d",
                &cv.x, &cv.y, &cv.length, &cv.speed, &cv.direction,
                &cv.rotation, &cv.endCapW, &cv.beltH, &cv.soundMaterial);
            if (cnt < 9) cv.soundMaterial = 0;
            out.conveyors.push_back(cv);
        }
        else if (strcmp(tag, "PROP") == 0) {
            PropData pr;
            int colFlag = 0;
            int cnt = fscanf(f, "%f %f %f %f %f %d %f %d %d %f %f %f %f %f %f",
                &pr.x, &pr.y, &pr.width, &pr.height, &pr.rotation,
                &colFlag, &pr.lightAffect, &pr.renderLayer, &pr.texVariant,
                &pr.bobAmp, &pr.bobSpeed, &pr.flickerIntens, &pr.flickerSpeed,
                &pr.swayAngle, &pr.swaySpeed);
            pr.hasCollision = (colFlag != 0);
            // Older files (< 15 tokens): animation fields keep their struct defaults
            (void)cnt;
            out.props.push_back(pr);
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
            KillZoneData kz; int texId = 0, rl = 1;
            int parsed = fscanf(f, "%f %f %f %f %d %f %d",
                &kz.x, &kz.y, &kz.w, &kz.h, &texId, &kz.rotation, &rl);
            if (parsed < 6) kz.rotation = 0.f;
            if (parsed < 7) rl = 1;
            kz.texId = static_cast<KillZoneTexture>(texId);
            kz.renderLayer = rl;
            out.killZones.push_back(kz);
        }
        else if (strcmp(tag, "LIGHT") == 0) {
            LightData L;
            int t = 0, en = 1;
            int parsed = fscanf(f, "%d %f %f %f %f %f %f %f %f %f %f %d %f %d",
                &t, &L.x, &L.y, &L.r, &L.g, &L.b,
                &L.intensity, &L.radius, &L.innerRadius,
                &L.angle, &L.direction,
                &L.bounces, &L.fogStrength, &en);
            if (parsed < 14) L.innerRadius = 0.f;
            L.type = static_cast<LightType>(t);
            L.enabled = (parsed < 13) ? true : (en != 0);
            if (parsed >= 14)
                (void)fscanf(f, " %f %f %f %f",
                    &L.flickerFreq, &L.flickerAmp,
                    &L.pulseFreq, &L.pulseAmp);
            out.lights.push_back(L);
        }
        else {
            char buf[512];
            fgets(buf, sizeof(buf), f);
        }
    }

    fclose(f);
    out.valid = true;
    return true;
}

// ── Default level 1 ───────────────────────────────────────────────────────────

LevelData GetDefaultLevel1()
{
    LevelData lv;
    lv.id = 1;
    lv.valid = true;

    lv.hasPlayerSpawn = true;
    lv.playerSpawn = { 35.0f + 64.0f * 3.5f + 10.0f, 817.0f };

    lv.hasRegulus = true;
    lv.regulusPos = { 22.0f, 225.0f };

    lv.hasCave = true;
    lv.cavePos = { 35.0f, 768.0f };

    lv.platforms = {
        {  27.f,  880.f, 412.f, 0.f,  0.0f },
        { 430.f,  870.f, 420.f, 0.f, -3.0f },
        {  60.f,  750.f, 720.f, 0.f,  3.0f },
        { 110.f,  620.f, 720.f, 0.f, -3.0f },
        {  60.f,  490.f, 720.f, 0.f,  3.0f },
        { 110.f,  360.f, 720.f, 0.f, -3.0f },
        { 460.f,  246.f, 320.f, 0.f,  3.0f },
        {  60.f,  240.f, 400.f, 0.f,  0.0f },
    };

    lv.ladders = {
        { 675.f, 245.f, 40.f, 104.f },
        { 160.f, 375.f, 40.f, 102.f },
        { 300.f, 365.f, 40.f, 117.f },
        { 680.f, 495.f, 40.f, 110.f },
        { 430.f, 489.f, 40.f, 128.f },
        { 380.f, 621.f, 40.f, 124.f },
        { 160.f, 632.f, 40.f, 101.f },
        { 670.f, 760.f, 40.f, 105.f },
    };

    lv.pathNodes = {
        { 125.f, 210.f, { 1,-1},  9, false },
        { 438.f, 210.f, { 2,-1},  5, false },
        { 680.f, 219.f, { 3, 4},  5, true  },
        { 680.f, 319.f, { 6,-1},  5, false },
        { 780.f, 224.f, { 5,-1}, 10, true  },
        { 800.f, 313.f, { 6,-1},  5, false },
        { 550.f, 326.f, { 7,-1},  5, false },
        { 305.f, 339.f, { 8, 9},  5, true  },
        { 305.f, 454.f, {11,-1},  5, false },
        { 110.f, 349.f, {10,-1}, 10, true  },
        {  70.f, 442.f, {11,-1},  5, false },
        { 430.f, 461.f, {12,-1},  5, false },
        { 685.f, 474.f, {13,14},  5, true  },
        { 685.f, 579.f, {16,-1},  5, false },
        { 780.f, 479.f, {15,-1}, 10, true  },
        { 800.f, 573.f, {16,-1},  5, false },
        { 550.f, 586.f, {17,-1},  5, false },
        { 165.f, 606.f, {18,19},  5, true  },
        { 165.f, 707.f, {21,-1},  5, false },
        { 110.f, 609.f, {20,-1}, 10, true  },
        {  70.f, 702.f, {21,-1},  5, false },
        { 430.f, 721.f, {22,-1},  5, false },
        { 675.f, 733.f, {23,24},  5, true  },
        { 675.f, 838.f, {26,-1},  5, false },
        { 780.f, 739.f, {25,-1}, 10, true  },
        { 800.f, 832.f, {26,-1},  5, false },
        { 400.f, 850.f, {27,-1},  5, false },
        { 148.f, 850.f, {-1,-1},  5, false },
    };

    lv.beams = {
        {  50.f,225.f},{114.f,225.f},{178.f,225.f},{212.f,225.f},{276.f,225.f},{340.f,225.f},
        {372.f,225.f},{436.f,230.f},{468.f,230.f},{532.f,235.f},{564.f,235.f},{628.f,240.f},
        {660.f,240.f},{724.f,245.f},
        {110.f,365.f},{142.f,365.f},{206.f,360.f},{238.f,360.f},{302.f,355.f},{334.f,355.f},
        {398.f,350.f},{430.f,350.f},{494.f,345.f},{526.f,345.f},{590.f,340.f},{622.f,340.f},
        {686.f,335.f},{718.f,335.f},{782.f,330.f},
        { 54.f,460.f},{ 86.f,460.f},{150.f,465.f},{182.f,465.f},{246.f,470.f},{278.f,470.f},
        {342.f,475.f},{374.f,475.f},{438.f,480.f},{470.f,480.f},{534.f,485.f},{566.f,485.f},
        {630.f,490.f},{662.f,490.f},{726.f,495.f},
        {110.f,625.f},{142.f,625.f},{206.f,620.f},{238.f,620.f},{302.f,615.f},{334.f,615.f},
        {398.f,605.f},{430.f,605.f},{494.f,600.f},{526.f,600.f},{590.f,595.f},{622.f,595.f},
        {686.f,590.f},{718.f,590.f},{782.f,585.f},
        { 54.f,715.f},{ 86.f,715.f},{150.f,720.f},{182.f,720.f},{246.f,725.f},{278.f,725.f},
        {342.f,730.f},{374.f,730.f},{438.f,735.f},{470.f,735.f},{534.f,740.f},{566.f,740.f},
        {630.f,745.f},{662.f,745.f},{726.f,750.f},
        { 30.f,865.f},{ 94.f,865.f},{158.f,865.f},{192.f,865.f},{256.f,865.f},{320.f,865.f},
        {352.f,865.f},{416.f,860.f},{448.f,860.f},{512.f,855.f},{544.f,855.f},{608.f,850.f},
        {640.f,850.f},{704.f,845.f},{768.f,845.f},
        {360.f,120.f},{424.f,120.f},{456.f,120.f},{296.f,150.f},{264.f,150.f},
    };

    lv.nukeSpawns = {
        {150.f,845.f},{330.f,845.f},{180.f,693.f},{480.f,693.f},{650.f,693.f},
        {250.f,563.f},{570.f,563.f},{150.f,433.f},{500.f,433.f},{300.f,303.f},
    };
    lv.beatriceSpawns = {
        {250.f,750.f},{500.f,750.f},{150.f,620.f},{420.f,620.f},{660.f,620.f},
        {200.f,490.f},{480.f,490.f},{310.f,360.f},{560.f,360.f},
    };
    lv.enemySpawns = {
        {180.f,706.f},{620.f,706.f},{200.f,576.f},{550.f,576.f},
        {160.f,446.f},{520.f,446.f},{200.f,316.f},{500.f,316.f},
    };

    lv.hasWinZone = true;
    lv.winZone = { 22.f, 180.f, 160.f, 60.f };

    // No props in default level 1 — add them via editor
    lv.props = {};

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
    for (int i = 0; i < static_cast<int>(lv.pathNodes.size()); i++) {
        const auto& n = lv.pathNodes[i];
        fprintf(f, "    /* %2d */ { {%.0f,%.0f}, {%2d,%2d,%2d}, {%d,%d,%d}, %2d, %s },\n",
            i, n.x, n.y, n.next[0], n.next[1], n.next[2],
            n.edgeType[0], n.edgeType[1], n.edgeType[2],
            n.rollThreshold, n.isSplitNode ? "true " : "false");
    }
    fprintf(f, "};\n\n");

    fprintf(f, "// Beam positions\nvector<BeamData> beamPositions = {\n");
    for (int i = 0; i < static_cast<int>(lv.beams.size()); i++) {
        fprintf(f, "    { %.0f, %.0f, %d, %d, %s, %s },",
            lv.beams[i].x, lv.beams[i].y,
            lv.beams[i].texVariant, lv.beams[i].renderLayer,
            lv.beams[i].flipX ? "true" : "false",
            lv.beams[i].transparent ? "true" : "false");
        if ((i + 1) % 6 == 0) fprintf(f, "\n");
    }
    fprintf(f, "\n};\n\n");

    fprintf(f, "// Props\nvector<PropData> propDefs = {\n");
    for (const auto& pr : lv.props)
        fprintf(f, "    { %.2ff, %.2ff, %.2ff, %.2ff, %.2ff, %s, %.3ff, %d, %d },\n",
            pr.x, pr.y, pr.width, pr.height, pr.rotation,
            pr.hasCollision ? "true" : "false",
            pr.lightAffect, pr.renderLayer, pr.texVariant);
    fprintf(f, "};\n\n");

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

// ── Game settings (level range) ───────────────────────────────────────────────

static std::string SettingsPath(const char* folder) {
    char buf[512]; snprintf(buf, sizeof(buf), "%s/game_settings.txt", folder); return buf;
}

bool LoadGameSettings(int& levelRangeMin, int& levelRangeMax,
                      float& volMusic, float& volSFX, float& volAbility, float& volUI,
                      unsigned int& highScore, int& highLevels,
                      const char* folder)
{
    levelRangeMin = 1; levelRangeMax = 10;
    volMusic = 0.8f; volSFX = 1.f; volAbility = 2.f; volUI = 1.f;
    highScore = 0; highLevels = 0;
    std::string path = SettingsPath(folder);
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false;
    char tag[64] = {};
    while (fscanf(f, "%63s", tag) == 1) {
        if      (strcmp(tag, "LEVEL_RANGE_MIN") == 0) (void)fscanf(f, "%d",  &levelRangeMin);
        else if (strcmp(tag, "LEVEL_RANGE_MAX") == 0) (void)fscanf(f, "%d",  &levelRangeMax);
        else if (strcmp(tag, "VOL_MUSIC")       == 0) (void)fscanf(f, "%f",  &volMusic);
        else if (strcmp(tag, "VOL_SFX")         == 0) (void)fscanf(f, "%f",  &volSFX);
        else if (strcmp(tag, "VOL_ABILITY")     == 0) (void)fscanf(f, "%f",  &volAbility);
        else if (strcmp(tag, "VOL_UI")          == 0) (void)fscanf(f, "%f",  &volUI);
        else if (strcmp(tag, "HIGH_SCORE")      == 0) (void)fscanf(f, "%u",  &highScore);
        else if (strcmp(tag, "HIGH_LEVELS")     == 0) (void)fscanf(f, "%d",  &highLevels);
    }
    fclose(f);
    return true;
}

bool SaveGameSettings(int levelRangeMin, int levelRangeMax,
                      float volMusic, float volSFX, float volAbility, float volUI,
                      unsigned int highScore, int highLevels,
                      const char* folder)
{
    EnsureDir(folder);
    std::string path = SettingsPath(folder);
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    fprintf(f, "LEVEL_RANGE_MIN %d\n",   levelRangeMin);
    fprintf(f, "LEVEL_RANGE_MAX %d\n",   levelRangeMax);
    fprintf(f, "VOL_MUSIC %.3f\n",       volMusic);
    fprintf(f, "VOL_SFX %.3f\n",         volSFX);
    fprintf(f, "VOL_ABILITY %.3f\n",     volAbility);
    fprintf(f, "VOL_UI %.3f\n",          volUI);
    fprintf(f, "HIGH_SCORE %u\n",        highScore);
    fprintf(f, "HIGH_LEVELS %d\n",       highLevels);
    fclose(f);
    return true;
}