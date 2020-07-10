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
// File: globals.cpp
// Desc: Definition for global application state, data and constants
// Author: David St-Louis
//-----------------------------------------------------------------------------

// App includes
#include "globals.h"

// OpenGL
#if defined(_WIN32) && !defined(APIENTRY)
#define APIENTRY __stdcall                  // It is customary to use APIENTRY for OpenGL function pointer declarations on all platforms.  Additionally, the Windows OpenGL header needs APIENTRY.
#endif
#if defined(_WIN32) && !defined(WINGDIAPI)
#define WINGDIAPI __declspec(dllimport)     // Some Windows OpenGL headers need this
#endif
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

// Stb image
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

// Main application state instance
State state;

// Credentials
Settings settings;

// Main window's dimensions
int width = 1280;
int height = 720;

// Arrow textures
ImTextureID ARROWS[8];

// Load configuration file from disk (./config.txt)
void loadConfigs()
{
    char key[256];
    char value[256];

    auto pFile = fopen("configs.txt", "r");
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
        }
        fclose(pFile);
    }

    // Load arrow textures here too
    for (int i = 0; i < 8; ++i)
    {
        auto filename = "assets/arrow" + std::to_string(i) + ".png";
        int w, h, bpp;
        auto pixels = stbi_load(filename.c_str(), &w, &h, &bpp, 4);

        // Upload texture to graphics system
        GLuint texture;
        GLint last_texture;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

        // Cleanup
        stbi_image_free(pixels);

        // Store our identifier
        ARROWS[i] = (ImTextureID)(intptr_t)texture;

        // Restore state
        glBindTexture(GL_TEXTURE_2D, last_texture);
    }
}

// Save configuration file to disk (./config.txt)
void saveConfigs()
{
    if (settings.instanceIndex != 0) return; // Only first instance will save

    auto pFile = fopen("configs.txt", "w");
    if (pFile)
    {
        fprintf(pFile, "username = %s\n", settings.username);
        fprintf(pFile, "password = %s\n", settings.password);
        fprintf(pFile, "colorIndex = %i\n", settings.colorIndex);
        fprintf(pFile, "gameUIIScale = %i\n", settings.gameUIIScale);
        fprintf(pFile, "protocol = %i\n", (int)settings.protocol);
        fclose(pFile);
    }
}
