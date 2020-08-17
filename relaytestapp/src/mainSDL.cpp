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
// File: main.cpp
// Desc: App entry point, SDL framework calls
// Author: David St-Louis
//-----------------------------------------------------------------------------

// SDL implementations related includes
#if defined(WIN32)
#include <Windows.h>
#endif
#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#define SDL_MAIN_HANDLED
#if defined(USE_MAXOS_SDL_FRAMEWORK)
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#else
#include <SDL.h>
#include <SDL_opengl.h>
#endif

// App includes
#include "app.h"
#include "globals.h"
#include "login.h"

#include <cmath>

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
bool done = false;

int main(int argc, char *argv[])
{
    // Make command prompt faster
    static char stdOutBuffer[8192];
    setvbuf(stdout, stdOutBuffer, _IOFBF, sizeof(stdOutBuffer));

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }
    
    SDL_DisplayMode current;
    SDL_Rect usable_bounds;
    SDL_GetDesktopDisplayMode(0, &current);
    SDL_GetDisplayUsableBounds(0, &usable_bounds);
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (argc > 2)
    {
        settings.instanceIndex = std::atoi(argv[argc - 2]);
        auto count = std::atoi(argv[argc - 1]);
        if (count > 1) settings.autoJoin = true;
        auto count_per_col = (int)std::sqrt((double)count);
        auto count_per_row = (int)std::ceil((double)count / (double)count_per_col);
        auto col_index = settings.instanceIndex % count_per_row;
        auto row_index = settings.instanceIndex / count_per_row;

        width = usable_bounds.w / count_per_row;
        height = usable_bounds.h / count_per_col;
        x = col_index * width;
        y = row_index * height;

        flags |= SDL_WINDOW_BORDERLESS;
    }

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_Window* window = SDL_CreateWindow("brainCloud Relay Test App", x, y, width, height, flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Init imgui GL renderer
    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();
    ImGui::StyleColorsClassic();

    // Load app related stuff
    loadConfigs();
    if (settings.autoJoin)
    {
        if (settings.instanceIndex > 0)
        {
            std::string newName(settings.username);
            newName += std::to_string(settings.instanceIndex + 1);
            strcpy(settings.username, newName.c_str());
        }
        settings.colorIndex = settings.instanceIndex % 8;
    }

    // Main loop
    while (!done)
    {
        // Poll window events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
            {
                done = true;
            }
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            {
                width = (int)event.window.data1;
                height = (int)event.window.data2;
            }
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
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
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
