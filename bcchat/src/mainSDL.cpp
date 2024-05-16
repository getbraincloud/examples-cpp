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
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl2.h"
#include <stdio.h>
#define SDL_MAIN_HANDLED
#if defined(USE_MAXOS_SDL_FRAMEWORK)
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#else
#include <SDL.h>
#include <SDL_opengl.h>
#endif

// App includes
#include "app.h"
#include "globals.h"
#include "login.h"

ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

int main()
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

    // Setup window
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // using SDL3
    SDL_DisplayID displayID = SDL_GetPrimaryDisplay(); // in case of multiple displays
    const SDL_DisplayMode *current;
    current = SDL_GetDesktopDisplayMode(displayID);

    // using SDL3
    SDL_Window* window = SDL_CreateWindow(
        "BC Chat",
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Init imgui GL renderer
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();

    // Load app related stuff
    //loadConfigs();    

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll window events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
            {
                done = true;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                width = (int)event.window.data1;
                height = (int)event.window.data2;
            }
        }

        // Setup style
        switch (theme)
        {
            case 0: ImGui::StyleColorsClassic(); break;
            case 1: ImGui::StyleColorsDark(); break;
            case 2: ImGui::StyleColorsLight(); break;
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
