cmake_minimum_required(VERSION 3.10)

# We want to use C++11 flag on the compiler
set(CMAKE_CXX_STANDARD 11)

project(RoomServerExampleServer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/brainclouds2s-cpp/cmake/")

add_subdirectory(brainclouds2s-cpp)

add_executable(${PROJECT_NAME} main.cpp RoomServer.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE brainclouds2s-cpp/include)
target_link_libraries(${PROJECT_NAME} PRIVATE brainclouds2s)
