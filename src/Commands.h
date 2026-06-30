#pragma once

#include <cstdlib>
#include <cstring>

// ---- Mixage différentiel ----------------------------------------------------

struct DriveOutput { int left; int right; };

inline int clampPct(int v) {
    if (v > 100) return 100;
    if (v < -100) return -100;
    return v;
}

inline DriveOutput mixDifferential(int x, int y) {
    DriveOutput o;
    o.left = clampPct(y + x);
    o.right = clampPct(y - x);
    return o;
}

// ---- Parsing des commandes BLE ----------------------------------------------

enum class CmdType { None, Drive, Pose, Stop };
struct Command { CmdType type; int x; int y; int pose; };

inline Command parseCommand(const char* line) {
    Command c{CmdType::None, 0, 0, 0};
    if (line == nullptr || line[0] == '\0') return c;
    if (line[0] == 'S') { c.type = CmdType::Stop; return c; }
    if (line[0] == 'P' && line[1] >= '1' && line[1] <= '4') {
        c.type = CmdType::Pose; c.pose = line[1] - '0'; return c;
    }
    if (line[0] == 'D' && line[1] == ',') {
        const char* p = line + 2;
        char* end = nullptr;
        long x = strtol(p, &end, 10);
        if (end == p || *end != ',') return c;
        p = end + 1;
        long y = strtol(p, &end, 10);
        if (end == p) return c;
        c.type = CmdType::Drive; c.x = (int)x; c.y = (int)y; return c;
    }
    return c;
}
