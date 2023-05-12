# CMakeList.txt : CMake project for hellobc, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.2)

# Enable C++11 features
set(CMAKE_CXX_STANDARD 11)

project ("hellobc")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT hellobc)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY XCODE_STARTUP_PROJECT hellobc)

# Add source to this project's executable.
list(APPEND src_files
	main.cpp
	ids.h
	)

add_executable (hellobc ${src_files})

# brainCloud
#set(USE_BC_LIBRARY ON)
if( USE_BC_LIBRARY )
	add_library(brainCloud STATIC IMPORTED) # or STATIC instead of SHARED
	set_target_properties(brainCloud PROPERTIES
  		IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/../thirdparties/brainCloud/libs/libbrainCloud.a"
  		INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/../thirdparties/brainCloud/include")

	list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/)
	list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/braincloud/internal/apple/)
	list(APPEND includes PUBLIC ../thirdparties/brainCloud/thirdparty/jsoncpp-1.0.0/)
	list(APPEND includes PUBLIC ../thirdparties/brainCloud/thirdparty/sskeychain/)
else()
	add_subdirectory(../thirdparties/braincloud-cpp ./thirdparties/braincloud-cpp)
	list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/include/)
	list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/libs/jsoncpp-1.0.0/)
endif()

list(APPEND libs brainCloud)

# Setup includes and linked libraries
target_include_directories(hellobc ${includes})
target_link_libraries(hellobc ${libs})
