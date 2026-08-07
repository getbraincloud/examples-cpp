//-----------------------------------------------------------------------------
// Copyright 2021 bitHeads inc.
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
// File: globals.cpp
// Desc: Definition for global application state, data and constants
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "globals.h"

// OpenGL
#if defined(_WIN32) && !defined(APIENTRY)
#define APIENTRY __stdcall // It is customary to use APIENTRY for OpenGL function pointer declarations on all platforms.  Additionally, the Windows OpenGL header needs APIENTRY.
#endif
#if defined(_WIN32) && !defined(WINGDIAPI)
#define WINGDIAPI __declspec(dllimport) // Some Windows OpenGL headers need this
#endif
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#elif defined(__ANDROID__)
#include <GLES/gl.h>
#else
#include <GL/gl.h>
#endif

// Stb image
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

// For resolving the running executable's own directory — see assetPath() below.
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <unistd.h>
#endif

// Runtime color palette (populated from braincloud "Colours" property after login)
std::vector<ImVec4> g_colors;

// Directory containing the running executable — cached after the first call.
// The CMake build copies assets/ next to the binary on every platform (Contents/
// MacOS/assets inside the .app bundle on mac, alongside the .exe on Windows), so
// resolving asset paths relative to THIS instead of the process's current working
// directory works both for `bccm run` (which sets a deliberate cwd) and for a
// distributed build launched by double-clicking it (Finder/Explorer set some
// unrelated cwd, which is what silently broke splotch/arrow textures there).
static const std::string &exeDir()
{
    static std::string dir;
    static bool resolved = false;
    if (!resolved)
    {
        resolved = true;
        char buf[4096];
#if defined(_WIN32)
        DWORD len = GetModuleFileNameA(nullptr, buf, sizeof(buf));
        std::string exePath(buf, len);
#elif defined(__APPLE__)
        uint32_t size = sizeof(buf);
        std::string exePath;
        if (_NSGetExecutablePath(buf, &size) == 0)
            exePath.assign(buf);
#else
        ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        std::string exePath(buf, len > 0 ? (size_t)len : 0);
#endif
        auto pos = exePath.find_last_of("/\\");
        if (pos != std::string::npos)
            dir = exePath.substr(0, pos);
    }
    return dir;
}

// Resolves relPath (e.g. "assets/PaintSplatter1.png") relative to the executable's
// own directory. Falls back to the plain relative path (old cwd-relative behavior)
// if the executable path couldn't be resolved for some reason.
static std::string assetPath(const std::string &relPath)
{
    const std::string &dir = exeDir();
    return dir.empty() ? relPath : (dir + "/" + relPath);
}

// Main application state instance
State state;

// Credentials
Settings settings;

// Main window's dimensions
int width = 1280;
int height = 720;

// Arrow textures
ImTextureID ARROWS[8];

// Splotch texture
ImTextureID SPLOTCH_TEX = nullptr;

// Load configuration file from disk.
// For multi-instance: tries configs_N.txt first, falls back to configs.txt.
// Returns true if a per-instance config (configs_N.txt) was successfully loaded.
bool loadConfigs()
{
    char key[256];
    char value[256];
    bool perInstanceLoaded = false;

    FILE *pFile = nullptr;
    if (settings.multiInstance || settings.instanceIndex > 0)
    {
        std::string instanceConfig = "configs_" + std::to_string(settings.instanceIndex) + ".txt";
        pFile = fopen(instanceConfig.c_str(), "r");
        if (pFile)
            perInstanceLoaded = true;
    }
    if (!pFile)
        pFile = fopen("configs.txt", "r");

    if (pFile)
    {
        while (fscanf(pFile, "%s = %s\n", key, value) == 2)
        {
            if (strcmp(key, "username") == 0)
            {
                strcpy(settings.username, value);
            }
            else if (strcmp(key, "password") == 0)
            {
                strcpy(settings.password, value);
            }
            else if (strcmp(key, "colorIndex") == 0)
            {
                settings.colorIndex = std::stoi(value);
            }
            else if (strcmp(key, "gameUIIScale") == 0)
            {
                settings.gameUIIScale = std::stoi(value);
            }
            else if (strcmp(key, "protocol") == 0)
            {
                settings.protocol = (BrainCloud::eRelayConnectionType)std::stoi(value);
            }
            else if (strcmp(key, "lobbyType") == 0)
            {
                settings.lobbyType = value;
            }
            else if (strcmp(key, "autoLogin") == 0)
            {
                settings.autoLogin = std::stoi(value) != 0;
            }
            else if (strcmp(key, "teamCode") == 0)
            {
                settings.teamCode = value;
            }
        }
        fclose(pFile);
    }

    // Helper: load a PNG from disk and upload as an OpenGL texture
    auto loadTexture = [](const std::string &path) -> ImTextureID {
        int w, h, bpp;
        auto pixels = stbi_load(path.c_str(), &w, &h, &bpp, 4);
        if (!pixels) return nullptr;
        GLuint tex;
        GLint prev;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        stbi_image_free(pixels);
        glBindTexture(GL_TEXTURE_2D, prev);
        return (ImTextureID)(intptr_t)tex;
    };

    // Load arrow textures
    for (int i = 0; i < 8; ++i)
        ARROWS[i] = loadTexture(assetPath("assets/arrow" + std::to_string(i) + ".png"));

    // Load splotch texture
    SPLOTCH_TEX = loadTexture(assetPath("assets/PaintSplatter1.png"));

    return perInstanceLoaded;
}

// Dark-navy, rounded-panel theme, applied once at startup so every screen (main
// menu, lobby, game HUD) reads as one consistent design rather than default ImGui
// gray. Values chosen to match the reference main-menu/HUD mockups.
void applyTheme()
{
    ImGuiStyle &style = ImGui::GetStyle();
    style.WindowRounding    = 10.0f;
    style.ChildRounding     = 8.0f;
    style.FrameRounding     = 6.0f;
    style.PopupRounding     = 8.0f;
    style.GrabRounding      = 6.0f;
    style.TabRounding       = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowPadding     = ImVec2(16.0f, 16.0f);
    style.ItemSpacing       = ImVec2(8.0f, 8.0f);

    ImVec4 *colors = style.Colors;
    colors[ImGuiCol_WindowBg]         = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_ChildBg]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_PopupBg]          = ImVec4(0.10f, 0.10f, 0.15f, 0.98f);
    colors[ImGuiCol_Border]           = ImVec4(0.30f, 0.32f, 0.42f, 0.55f);
    colors[ImGuiCol_FrameBg]          = ImVec4(0.16f, 0.17f, 0.23f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.20f, 0.22f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive]    = ImVec4(0.24f, 0.26f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg]          = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgActive]    = ImVec4(0.14f, 0.15f, 0.21f, 1.00f);
    colors[ImGuiCol_Button]           = ImVec4(0.20f, 0.21f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered]    = ImVec4(0.27f, 0.29f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive]     = ImVec4(0.30f, 0.45f, 0.90f, 1.00f);
    colors[ImGuiCol_Header]           = colors[ImGuiCol_Button];
    colors[ImGuiCol_HeaderHovered]    = colors[ImGuiCol_ButtonHovered];
    colors[ImGuiCol_HeaderActive]     = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_TableHeaderBg]    = ImVec4(0.14f, 0.15f, 0.21f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.26f, 0.28f, 0.36f, 0.60f);
    colors[ImGuiCol_TableBorderStrong]= ImVec4(0.30f, 0.32f, 0.42f, 0.80f);
    colors[ImGuiCol_TableRowBg]       = ImVec4(1.00f, 1.00f, 1.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]    = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
    colors[ImGuiCol_ScrollbarBg]      = ImVec4(0.08f, 0.08f, 0.12f, 1.00f);
    colors[ImGuiCol_Text]             = ImVec4(0.92f, 0.93f, 0.96f, 1.00f);
    colors[ImGuiCol_Separator]        = colors[ImGuiCol_Border];
}

// Save configuration file to disk.
// Instance 0 saves to configs.txt; instance N saves to configs_N.txt.
void saveConfigs()
{
    std::string configFile = (settings.multiInstance || settings.instanceIndex > 0)
                                 ? ("configs_" + std::to_string(settings.instanceIndex) + ".txt")
                                 : "configs.txt";

    auto pFile = fopen(configFile.c_str(), "w");
    if (pFile)
    {
        fprintf(pFile, "username = %s\n", settings.username);
        // only use this on multi-mode otherwise never save this. EVER.
        if (settings.multiInstance && strlen(settings.password) > 0)
        {
            fprintf(pFile, "password = %s\n", settings.password);
        }
        fprintf(pFile, "colorIndex = %i\n", settings.colorIndex);
        fprintf(pFile, "gameUIIScale = %i\n", settings.gameUIIScale);
        fprintf(pFile, "protocol = %i\n", (int)settings.protocol);
        fprintf(pFile, "lobbyType = %s\n", settings.lobbyType.c_str());
        fprintf(pFile, "teamCode = %s\n", settings.teamCode.c_str());
        fprintf(pFile, "autoLogin = %i\n", settings.autoLogin ? 1 : 0);
        fclose(pFile);
    }
}
