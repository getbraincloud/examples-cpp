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
// File: coverage.h
// Desc: Canvas coverage % + win-ranking calculation (BCLOUD-14472 / BCLOUD-14490).
//       Pure logic — no ImGui, no brainCloud, no globals — so this is the exact
//       artifact other RelayTestApp ports (dotnet/godot/java/react) should copy.
//-----------------------------------------------------------------------------
#pragma once

#include "globals.h"

// Computes each member's canvas coverage and ranks them.
//
// Algorithm: rasterize a COVERAGE_GRID_CELL_SIZE-resolution ownership grid over the canvas
// by stamping every splotch, in paint order, as a filled circle of radius SPLOTCH_RADIUS
// centered on it — so a later splotch always overwrites an earlier one wherever they
// overlap. This is exactly what ends up rendered on screen (paint order = draw order =
// "last one wins"), not an approximation of it — it replaced an earlier center-point/
// obscure-radius heuristic that could diverge sharply from the actual visible area once
// splotches were dense/overlapping (e.g. a long stress-test match).
//
// coveragePct is each player's owned-cell-count * cell-area / canvas-area (clamped to 100)
// — ABSOLUTE canvas-area coverage, NOT a share of painted area, so a solo match doesn't
// trivially score 100%. Splotches whose ownerCxId doesn't match any current member (a
// not-yet-ported legacy sender, or a departed player) fall back to a colorIndex match
// against a live member; if that also fails, the splotch still overwrites the grid
// (correctly obscuring whatever was under it) but credits no one.
//
// Every current member gets a seeded zero-coverage entry, so painters-of-nothing still
// appear on the board, ranked last. Result is sorted best-first (coveragePct desc,
// visibleCount desc, cxId asc for determinism); ties share a rank and don't "beat" each
// other in CoverageEntry::beaten. (visibleCount is now an owned-cell count, not a splotch
// count — the field wasn't renamed since nothing outside this file inspects it directly.)
std::vector<CoverageEntry> computeCoverage(const std::vector<Splotch> &splotches,
                                            const std::vector<User> &members);
