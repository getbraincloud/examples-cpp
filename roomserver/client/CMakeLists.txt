cmake_minimum_required(VERSION 3.10)

project(RoomServerExampleClient)

# we use C++ 14 for std::literals::chrono_literals
set(CMAKE_CXX_STANDARD 14)

add_executable(${PROJECT_NAME} main.cpp)

add_subdirectory(../../thirdparties/braincloud-cpp braincloud-cpp)
target_include_directories(${PROJECT_NAME} PUBLIC
    braincloud-cpp/include/
    braincloud-cpp/lib/jsoncpp/src/
)
target_link_libraries(${PROJECT_NAME} PUBLIC brainCloud)
