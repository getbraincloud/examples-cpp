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
// File: main.cpp
// Desc: App entry point, SDL framework calls
// Author: David St-Louis
//-----------------------------------------------------------------------------

// SDL implementations related includes
#if defined(WIN32)
#include <Windows.h>
#endif
#include "imgui.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl2.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <stdio.h>
#include <string>

// App includes
#include "app.h"
#include "globals.h"
#include "login.h"

#include <cmath>

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
bool done = false;
bool startfullscreen = false;

int main(int argc, char *argv[])
{
    // In multi-instance mode, redirect stdout/stderr to a per-instance log file
    // so debug output from all instances is captured separately (log_N.txt).
    if (argc > 2)
    {
        int instanceIdx = std::atoi(argv[argc - 2]);
        std::string logPath = "log_" + std::to_string(instanceIdx) + ".txt";
        freopen(logPath.c_str(), "w", stdout);
        freopen(logPath.c_str(), "a", stderr);
        // Line-buffered so writes appear immediately even through the redirect
        setvbuf(stdout, nullptr, _IOLBF, 0);
    }
    else
    {
        // Make command prompt faster for single-instance mode
        static char stdOutBuffer[8192];
        setvbuf(stdout, stdOutBuffer, _IOFBF, sizeof(stdOutBuffer));
    }

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // using SDL3
    SDL_DisplayID displayID = SDL_GetPrimaryDisplay(); // in case of multiple displays
    const SDL_DisplayMode *current;
    current = SDL_GetDesktopDisplayMode(displayID);

    SDL_Rect usable_bounds;
    int retval = SDL_GetDisplayUsableBounds(displayID, &usable_bounds);
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (retval == 0)
    { // only set window size if usable bounds ok
        if (argc > 2)
        {
            settings.instanceIndex = std::atoi(argv[argc - 2]);
            auto count = std::atoi(argv[argc - 1]);
            settings.multiInstance = true;
            if (count > 1)
                settings.autoJoin = true;

            // Grid layout: tile instances across the screen without overlapping.
            // Window size capped at 1280×720; grid cell determines position.
            int cols = (int)std::ceil(std::sqrt((double)count));
            int rows = (int)std::ceil((double)count / (double)cols);
            width = std::min(1280, usable_bounds.w / cols);
            height = std::min(720, usable_bounds.h / rows);
            int col_index = settings.instanceIndex % cols;
            int row_index = settings.instanceIndex / cols;
            x = usable_bounds.x + col_index * width;
            y = usable_bounds.y + row_index * height;
        }
        else if (startfullscreen)
        {
            // when possible, use entire space
            width = usable_bounds.w;
            height = usable_bounds.h;
        }
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // using SDL3
    std::string windowTitle = "brainCloud Relay Test App " + appVersion;
    if (settings.multiInstance)
        windowTitle += " [" + std::to_string(settings.instanceIndex + 1) + "]";
    SDL_Window *window = SDL_CreateWindow(windowTitle.c_str(), width, height, flags);

    // SDL3 CreateWindow doesn't take position; apply it now if set by grid layout
    if (settings.multiInstance)
        SDL_SetWindowPosition(window, x, y);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Init imgui GL renderer
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    ImGui::StyleColorsClassic();

    // Load app related stuff
    auto instanceConfigLoaded = loadConfigs();
    if (settings.multiInstance)
    {
        // In multi-instance mode every instance owns its own configs_N.txt.
        // If none was found, auto-generate one with a generic username and
        // matching password so the login form is pre-filled and autoLogin works.
        if (!instanceConfigLoaded)
        {
            std::string newName = "smrj_" + std::to_string(settings.instanceIndex + 1);
            strncpy(settings.username, newName.c_str(), MAX_CREDENTIAL_CHAR - 1);
            settings.username[MAX_CREDENTIAL_CHAR - 1] = '\0';
            strncpy(settings.password, newName.c_str(), MAX_CREDENTIAL_CHAR - 1);
            settings.password[MAX_CREDENTIAL_CHAR - 1] = '\0';
            settings.autoLogin = true;
            settings.lobbyType = DEFAULT_LOBBY_TYPE;
            saveConfigs();
        }
        // Multi-instance always uses CursorPartyGameLift regardless of saved config
        settings.lobbyType = DEFAULT_LOBBY_TYPE;
        settings.colorIndex = settings.instanceIndex % NUM_COLORS;
    }

    // Main loop
    while (!done)
    {
        // Poll window events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                app_exit();
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                width = (int)event.window.data1;
                height = (int)event.window.data2;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL3_NewFrame();

        ImGui::NewFrame();

        // Draw the app
        app_update();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
    }

    // Cleanup

    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL3_Shutdown();

    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
