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
// File: BCCallback.cpp
// Desc: Helper class that allows for lambda style callbacks for
//       brainCloud API calls.
// Author: David St-Louis
//
// Usage example:
//
// bc->someBrainCloudCall(..., new BCCallback(
//     [](const Json::Value& result)
//     {
//         // Success!!
//     },
//     [](const std::string& error)
//     {
//         // Error ...
//     })
// );
//-----------------------------------------------------------------------------

// App includes
#include "BCCallback.h"

std::set<BCCallback*> BCCallback::bcCallbacks;

BCCallback::BCCallback(
    std::function<void(const Json::Value&)> success,
    std::function<void(const std::string&)> error)
    : m_success(success)
    , m_error(error)
{
    bcCallbacks.insert(this);
}

void BCCallback::destroyAll()
{
    for (auto pBCCallback : bcCallbacks)
    {
        delete pBCCallback;
    }
    bcCallbacks.clear();
}

void BCCallback::destroy()
{
    auto it = bcCallbacks.find(this);
    if (it != bcCallbacks.end())
    {
        bcCallbacks.erase(it);
    }
    delete this;
}

void BCCallback::serverCallback(BrainCloud::ServiceName serviceName, 
                    BrainCloud::ServiceOperation serviceOperation, 
                    const std::string& jsonData)
{
    if (m_success)
    {
        Json::Reader reader;
        Json::Value result;
        reader.parse(jsonData, result);
        m_success(result);
    }
    destroy();
}

void BCCallback::serverError(BrainCloud::ServiceName serviceName, 
                    BrainCloud::ServiceOperation serviceOperation, 
                    int statusCode, 
                    int reasonCode, 
                    const std::string& jsonError)
{
    if (m_error)
    {
        Json::Reader reader;
        Json::Value result;
        reader.parse(jsonError, result);
        m_error(result["status_message"].asString());
    }
    destroy();
}
