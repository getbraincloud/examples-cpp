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
        // Uniform grid over the canvas, cell size = obscure-check neighborhood unit.
        // Only a 3x3 cell neighborhood can contain a splotch within SPLOTCH_RADIUS,
        // since SPLOTCH_RADIUS == cell size / 2.
        const float CELL = SPLOTCH_DISPLAY_SIZE;
        const int gridW = (int)(CANVAS_W / CELL) + 2;
        const int gridH = (int)(CANVAS_H / CELL) + 2;
        std::vector<std::vector<int>> grid(gridW * gridH);

        auto cellX = [&](int x) {
            int cx = (int)(x / CELL);
            return std::max(0, std::min(gridW - 1, cx));
        };
        auto cellY = [&](int y) {
            int cy = (int)(y / CELL);
            return std::max(0, std::min(gridH - 1, cy));
        };

        const int R2 = (int)(SPLOTCH_RADIUS * SPLOTCH_RADIUS); // strict '<' obscure radius, squared
        std::vector<bool> visible(N, true);

        // Sweep last-painted -> first. The grid at step i contains only splotches
        // painted AFTER i (i.e. "on top" of it), which is exactly what "visible on
        // the top layer" needs to check against.
        for (int i = N - 1; i >= 0; --i)
        {
            const Splotch &s = splotches[i];
            int cx = cellX(s.pos.x);
            int cy = cellY(s.pos.y);
            bool obscured = false;

            for (int dy = -1; dy <= 1 && !obscured; ++dy)
            {
                int ny = cy + dy;
                if (ny < 0 || ny >= gridH) continue;
                for (int dx = -1; dx <= 1; ++dx)
                {
                    int nx = cx + dx;
                    if (nx < 0 || nx >= gridW) continue;
                    const auto &cell = grid[nx + ny * gridW];
                    for (int j : cell)
                    {
                        int ddx = splotches[j].pos.x - s.pos.x;
                        int ddy = splotches[j].pos.y - s.pos.y;
                        if (ddx * ddx + ddy * ddy < R2)
                        {
                            obscured = true;
                            break;
                        }
                    }
                    if (obscured) break;
                }
            }

            visible[i] = !obscured;
            grid[cx + cy * gridW].push_back(i);
        }

        for (int i = 0; i < N; ++i)
        {
            if (!visible[i]) continue;
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
            if (idx >= 0)
                result[idx].visibleCount++;
        }
    }

    const float splotchArea = 3.14159265f * SPLOTCH_RADIUS * SPLOTCH_RADIUS;
    const float canvasArea = CANVAS_W * CANVAS_H;
    for (auto &e : result)
        e.coveragePct = std::min(100.0f, e.visibleCount * splotchArea / canvasArea * 100.0f);

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
