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
// Algorithm (matches the ticket's own rule — "check all splotches whose centers are
// visible on the top layer" — grid-accelerated so it doesn't cost O(N^2)):
//   Sweep splotches from last-painted to first, maintaining a uniform spatial grid of
//   already-swept (i.e. later-painted / "on top") splotches. A splotch's center is
//   "visible" iff no later splotch has a center within SPLOTCH_RADIUS of it (strict '<'
//   on the squared distance, cell size = SPLOTCH_DISPLAY_SIZE so only the 3x3 cell
//   neighborhood needs checking).
//
// coveragePct is ABSOLUTE canvas-area coverage (visibleCount * splotch-area / canvas-area,
// clamped to 100) — NOT a share of painted area — so a solo match doesn't trivially score
// 100%. Splotches whose ownerCxId doesn't match any current member (a not-yet-ported
// legacy sender, or a departed player) fall back to a colorIndex match against a live
// member; if that also fails, the splotch counts toward no one.
//
// Every current member gets a seeded zero-coverage entry, so painters-of-nothing still
// appear on the board, ranked last. Result is sorted best-first (coveragePct desc,
// visibleCount desc, cxId asc for determinism); ties share a rank and don't "beat" each
// other in CoverageEntry::beaten.
std::vector<CoverageEntry> computeCoverage(const std::vector<Splotch> &splotches,
                                            const std::vector<User> &members);
