cmake_minimum_required(VERSION 3.10)

project(MyRoomServer)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/brainclouds2s-cpp/cmake/")
find_package(JsonCpp REQUIRED)
add_subdirectory(brainclouds2s-cpp)

add_executable(${PROJECT_NAME} main.cpp RoomServer.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE brainclouds2s-cpp/include ${JSONCPP_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE brainclouds2s ${JSONCPP_LIBRARY})
