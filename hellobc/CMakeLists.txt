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

# note: compiler ids only available after project() call
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		# using Clang with clang-cl front end
		message("Building brainCloud using Clang with clang-cl front end")
	elseif (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "GNU")
		# using Clang with regular front end
		message("Building brainCloud using Clang with regular front end")
	else ()
		# using Clang or AppleClang
		message("Building brainCloud using ${CMAKE_CXX_COMPILER_ID}")
	endif()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# using GCC
	message("Building brainCloud using GCC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
	# using Intel C++
	message("Building brainCloud using Intel C++")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# using Visual Studio C++
	message("Building brainCloud using Visual Studio C++")
endif()

add_executable (hellobc ${src_files})

# brainCloud
set(USE_BC_LIBRARY ON)
if( USE_BC_LIBRARY )
	if (APPLE)
		add_library(brainCloud STATIC IMPORTED) # or STATIC instead of SHARED
		set_target_properties(brainCloud PROPERTIES
			IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/../thirdparties/brainCloud/libs/brainCloudClient_osx.a"
			INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/../thirdparties/brainCloud/include")

		list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/)
		list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/braincloud/internal/apple/)
		list(APPEND includes PUBLIC ../thirdparties/brainCloud/thirdparty/jsoncpp-1.0.0/)
	elseif (UNIX AND NOT APPLE AND NOT ANDROID) # LINUX
		find_package(CURL REQUIRED)
		list(APPEND includes PUBLIC ${CURL_INCLUDE_DIRS})

		find_package(UUID REQUIRED)
		list(APPEND includes PUBLIC ${UUID_INCLUDE_DIRS})

		list(APPEND libs
				${CURL_LIBRARIES}
				${UUID_LIBRARIES})

		add_library(brainCloud STATIC IMPORTED) # or STATIC instead of SHARED
		set_target_properties(brainCloud PROPERTIES
				IMPORTED_LOCATION "${PROJECT_SOURCE_DIR}/../thirdparties/brainCloud/libs/release/libbrainCloud.a"
				INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/../thirdparties/brainCloud/include")

		list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/)
		list(APPEND includes PUBLIC ../thirdparties/brainCloud/include/braincloud/internal/)
		list(APPEND includes PUBLIC ../thirdparties/brainCloud/thirdparty/jsoncpp-1.0.0/)
	endif()
else()
	add_subdirectory(../thirdparties/braincloud-cpp ./thirdparties/braincloud-cpp)
	list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/include/)
	list(APPEND includes PUBLIC ../thirdparties/braincloud-cpp/libs/jsoncpp-1.0.0)
endif()

list(APPEND libs brainCloud)

# Setup includes and linked libraries
target_include_directories(hellobc ${includes})
target_link_libraries(hellobc ${libs})
