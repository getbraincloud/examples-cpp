//-----------------------------------------------------------------------------
// Copyright 2026 bitHeads inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-----------------------------------------------------------------------------
// File: coverage.cpp
// Desc: Canvas coverage % + win-ranking calculation (BCLOUD-14472 / BCLOUD-14490).
//-----------------------------------------------------------------------------

#include "coverage.h"

#include <algorithm>
#include <cmath>
#include <map>

std::vector<CoverageEntry> computeCoverage(const std::vector<Splotch> &splotches,
                                            const std::vector<User> &members)
{
    std::vector<CoverageEntry> result;
    result.reserve(members.size());

    std::map<std::string, int> indexByCxId;
    for (const auto &m : members)
    {
        indexByCxId[m.cxId] = (int)result.size();
        CoverageEntry e;
        e.cxId = m.cxId;
        e.colorIndex = m.colorIndex;
        result.push_back(e);
    }

    const int N = (int)splotches.size();
    if (N > 0)
    {
        // Ownership grid: each cell records which player (by result[] index) most
        // recently painted over it. Splotches are stamped in paint order (a filled circle
        // of radius SPLOTCH_RADIUS), so a later splotch always overwrites an earlier one
        // wherever they overlap — exactly matching what's rendered on screen (paint order
        // = draw order = "last one wins"). -1 = unpainted, or painted by something that
        // couldn't be attributed to any current member (still overwrites the grid, so it
        // correctly obscures whoever was there before, it just credits no one).
        const float CELL = COVERAGE_GRID_CELL_SIZE;
        const int gridW = (int)(CANVAS_W / CELL);
        const int gridH = (int)(CANVAS_H / CELL);

        static std::vector<int> owner; // reused across calls (this runs every ~250ms mid-match)
        owner.assign((size_t)gridW * gridH, -1);

        const float R2 = SPLOTCH_RADIUS * SPLOTCH_RADIUS;
        const int cellRadius = (int)std::ceil(SPLOTCH_RADIUS / CELL);

        for (int i = 0; i < N; ++i)
        {
            const Splotch &s = splotches[i];

            int idx = -1;
            if (!s.ownerCxId.empty())
            {
                auto it = indexByCxId.find(s.ownerCxId);
                if (it != indexByCxId.end())
                    idx = it->second;
            }
            if (idx < 0)
            {
                // Unattributed (legacy sender / departed player) — fall back to a
                // colorIndex match against a live member, matching pre-attribution behavior.
                for (size_t k = 0; k < result.size(); ++k)
                {
                    if (result[k].colorIndex == s.colorIndex)
                    {
                        idx = (int)k;
                        break;
                    }
                }
            }

            int cx = (int)(s.pos.x / CELL);
            int cy = (int)(s.pos.y / CELL);
            for (int dy = -cellRadius; dy <= cellRadius; ++dy)
            {
                int gy = cy + dy;
                if (gy < 0 || gy >= gridH) continue;
                float py = (gy + 0.5f) * CELL;
                float ddy = py - (float)s.pos.y;
                for (int dx = -cellRadius; dx <= cellRadius; ++dx)
                {
                    int gx = cx + dx;
                    if (gx < 0 || gx >= gridW) continue;
                    float px = (gx + 0.5f) * CELL;
                    float ddx = px - (float)s.pos.x;
                    if (ddx * ddx + ddy * ddy <= R2)
                        owner[gx + gy * gridW] = idx; // may be -1 — still overwrites, credits no one
                }
            }
        }

        for (int c = 0, count = gridW * gridH; c < count; ++c)
        {
            int idx = owner[c];
            if (idx >= 0)
                result[idx].visibleCount++;
        }
    }

    const float cellArea = COVERAGE_GRID_CELL_SIZE * COVERAGE_GRID_CELL_SIZE;
    const float canvasArea = CANVAS_W * CANVAS_H;
    for (auto &e : result)
        e.coveragePct = std::min(100.0f, e.visibleCount * cellArea / canvasArea * 100.0f);

    std::sort(result.begin(), result.end(), [](const CoverageEntry &a, const CoverageEntry &b) {
        if (a.coveragePct != b.coveragePct) return a.coveragePct > b.coveragePct;
        if (a.visibleCount != b.visibleCount) return a.visibleCount > b.visibleCount;
        return a.cxId < b.cxId;
    });

    for (size_t i = 0; i < result.size(); ++i)
    {
        if (i > 0 &&
            result[i].coveragePct == result[i - 1].coveragePct &&
            result[i].visibleCount == result[i - 1].visibleCount)
            result[i].rank = result[i - 1].rank; // tie shares rank
        else
            result[i].rank = (int)i + 1;
    }

    for (size_t i = 0; i < result.size(); ++i)
    {
        int beaten = 0;
        for (size_t j = 0; j < result.size(); ++j)
            if (result[j].rank > result[i].rank) ++beaten;
        result[i].beaten = beaten;
    }

    return result;
}
