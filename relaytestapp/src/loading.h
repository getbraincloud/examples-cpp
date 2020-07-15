//-----------------------------------------------------------------------------
// Copyright 2018 bitHeads inc.
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
// File: loading.h
// Desc: Interface for displaying and updating a loading screen
// Author: David St-Louis
//-----------------------------------------------------------------------------

// C/C++ headers
#include <string>

// Draws a loading dialog
void loading_update();

// Point loading_text to the text that should be displayed in the dialog
extern std::string loading_text;
