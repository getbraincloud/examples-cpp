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

#if defined(__ANDROID__)
#include <jni.h>
#include "braincloud/internal/android/AndroidGlobals.h" // to store java native interface env and context for app
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#else
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl2.h"
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#endif

#include <stdio.h>

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

    // a note for android, uses openGL3 and SDL2
    // imgui has build and runtime issues with SDL3
    // build issue can be fixed by setting focused_window var in imgui_impl_sdl3.cpp
    // runtime issue is libraries (.so) failing to load
#if defined(__ANDROID__)
    // retrieve the JNI environment.
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();

    // retrieve the Java instance of the SDLActivity
    jobject activity = (jobject)SDL_AndroidGetActivity();

    // these are passed in from MainActivity.java
    // we need them for SaveDataHelper to access SharedPreferences
    // context may change while running callbacks so set each time
    BrainCloud::appEnv = env;
    BrainCloud::appContext = activity;
#endif
    // using SDL3
    SDL_DisplayID displayID = SDL_GetPrimaryDisplay(); // in case of multiple displays
    const SDL_DisplayMode *current;
    current = SDL_GetDesktopDisplayMode(displayID);

    SDL_Rect usable_bounds;
    int retval = SDL_GetDisplayUsableBounds(displayID, &usable_bounds);
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if(retval==0){ // only set window size if usable bounds ok
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
        else{
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
    SDL_Window* window = SDL_CreateWindow("brainCloud Relay Test App", width, height, flags);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui binding
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Init imgui GL renderer
#if defined(__ANDROID__)
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init();
#else
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL2_Init();
#endif
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

        // Start the Dear ImGui frame
#if defined(__ANDROID__)
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
#else
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplSDL3_NewFrame();
#endif
        ImGui::NewFrame();

        // Draw the app
        app_update();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
#if defined(__ANDROID__)
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#else
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
#endif
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
#if defined(__ANDROID__)
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
#else
    ImGui_ImplOpenGL2_Shutdown();
    ImGui_ImplSDL3_Shutdown();
#endif
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
