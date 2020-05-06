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
// File: BCCallback.h
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

// Thirdparty includes
#include <braincloud/IServerCallback.h>
#include <braincloud/ServiceName.h>
#include <braincloud/ServiceOperation.h>
#include <json/json.h>

// C/C++ includes
#include <functional>
#include <set>

class BCCallback final : public BrainCloud::IServerCallback
{
public:
    BCCallback(
        std::function<void(const Json::Value&)> success,
        std::function<void(const std::string&)> error);

    static void destroyAll();

private:
    ~BCCallback() {} // Make sure only can be deleted on callback

    void destroy();

    void serverCallback(BrainCloud::ServiceName serviceName, 
                        BrainCloud::ServiceOperation serviceOperation, 
                        const std::string& jsonData) override;
    void serverError(BrainCloud::ServiceName serviceName, 
                     BrainCloud::ServiceOperation serviceOperation, 
                     int statusCode, 
                     int reasonCode, 
                     const std::string& jsonError) override;
    void serverWarning(BrainCloud::ServiceName serviceName, 
                       BrainCloud::ServiceOperation serviceOperation, 
                       int statusCode,
                       int reasonCode,
                       int numRetries, 
                       const std::string& statusMessage) override;

    std::function<void(const Json::Value&)> m_success;
    std::function<void(const std::string&)> m_error;

    static std::set<BCCallback*> bcCallbacks; // Dangling references to be deleted if we reset communications
};
