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

// Main application state instance
State state;

// Credentials
char username[MAX_CREDENTIAL_CHAR] = { '\0' };
char password[MAX_CREDENTIAL_CHAR] = { '\0' };

// GUI theme. 0 = classic, 1 = dark, 2 = light
int theme = 0;

// Main window's dimensions
int width = 1280;
int height = 720;

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
                strcpy(username, value);
            }
            else if (strcmp(key, "password") == 0)
            {
                strcpy(password, value);
            }
            else if (strcmp(key, "theme") == 0)
            {
                theme = atoi(value);
            }
        }
        fclose(pFile);
    }
}

// Save configuration file to disk (./config.txt)
void saveConfigs()
{
    auto pFile = fopen("configs.txt", "w");
    if (pFile)
    {
        fprintf(pFile, "username = %s\n", username);
        fprintf(pFile, "password = %s\n", password);
        fprintf(pFile, "theme = %i\n", theme);
        fclose(pFile);
    }
}
