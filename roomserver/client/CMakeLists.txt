cmake_minimum_required(VERSION 3.10)

project(RoomServerExampleClient)
set(CMAKE_CXX_STANDARD 11)
add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(../../thirdparties/braincloud-cpp braincloud-cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
    braincloud-cpp/include/
    braincloud-cpp/lib/jsoncpp-1.0.0/
)
target_link_libraries(${PROJECT_NAME} PUBLIC brainCloud)
