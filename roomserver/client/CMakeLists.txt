cmake_minimum_required(VERSION 3.10)

project(RoomServerExampleClient)

# we use C++ 14 for std::literals::chrono_literals
set(CMAKE_CXX_STANDARD 14)

# The SDK expects the parent project's CA bundle in resources/cacerts.pem.
# Generate it from the pinned SDK so the example does not maintain a second copy.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/resources")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/../../thirdparties/braincloud-cpp/resources/cacerts.pem"
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/cacerts.pem"
    COPYONLY
)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(../../thirdparties/braincloud-cpp braincloud-cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
    braincloud-cpp/include/
    braincloud-cpp/lib/jsoncpp-1.0.0/
)
target_link_libraries(${PROJECT_NAME} PUBLIC brainCloud)
