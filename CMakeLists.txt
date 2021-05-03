cmake_minimum_required(VERSION 3.13)
project(hammer VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD          11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS        OFF)

# Try to compile release
set(DEFAULT_BUILD_TYPE "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
	set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE STRING "Choose the type of build." FORCE)
	# Set the possible values of build type for cmake-gui
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(cglm     REQUIRED)
find_package(deadlock REQUIRED)
find_package(delaunay REQUIRED)
find_package(GLEW     REQUIRED)
find_package(OpenGL   REQUIRED)
find_package(PNG      REQUIRED)
find_package(SDL2     REQUIRED COMPONENTS SDL2::SDL2 SDL2::main SDL2::image)
find_package(Threads  REQUIRED)

# Add warnings
# Note: add_compile_options propegates these options to e.g. examples.
set(HAMMER_MSVC_WARNFLAGS /W4 /WX)
set(HAMMER_OTHR_WARNFLAGS -Wall -Wextra -Wno-override-init -Wno-missing-field-initializers)
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	set(HAMMER_OTHR_WARNFLAGS "${set(HAMMER_OTHR_WARNFLAGS} -Wno-date-time -Wno-c++98-compat -Wno-implicit-fallthrough")
endif()
add_compile_options("$<IF:$<C_COMPILER_ID:MSVC>,${HAMMER_MSVC_WARNFLAGS},${HAMMER_OTHR_WARNFLAGS}>")

set(HAMMER_SOURCES ${PROJECT_SOURCE_DIR}/src/appstate/main_menu.c
                   ${PROJECT_SOURCE_DIR}/src/appstate/viz_climate.c
                   ${PROJECT_SOURCE_DIR}/src/appstate/viz_tectonic.c
                   ${PROJECT_SOURCE_DIR}/src/appstate/world_config.c
                   ${PROJECT_SOURCE_DIR}/src/gui/img.c
                   ${PROJECT_SOURCE_DIR}/src/gui/interaction.c
                   ${PROJECT_SOURCE_DIR}/src/gui/rect.c
                   ${PROJECT_SOURCE_DIR}/src/gui/text.c
                   ${PROJECT_SOURCE_DIR}/src/worldgen/climate.c
                   ${PROJECT_SOURCE_DIR}/src/worldgen/large_scale_world_gen.c
                   ${PROJECT_SOURCE_DIR}/src/worldgen/tectonic.c
                   ${PROJECT_SOURCE_DIR}/src/cli.c
                   ${PROJECT_SOURCE_DIR}/src/error.c
                   ${PROJECT_SOURCE_DIR}/src/file.c
                   ${PROJECT_SOURCE_DIR}/src/glsl.c
                   ${PROJECT_SOURCE_DIR}/src/image.c
                   ${PROJECT_SOURCE_DIR}/src/opensimplex.c
                   ${PROJECT_SOURCE_DIR}/src/salloc.c
                   ${PROJECT_SOURCE_DIR}/src/main.c
                   ${PROJECT_SOURCE_DIR}/src/poisson.c
                   ${PROJECT_SOURCE_DIR}/src/ring.c
                   ${PROJECT_SOURCE_DIR}/src/vector.c
                   ${PROJECT_SOURCE_DIR}/src/version.c
                   ${PROJECT_SOURCE_DIR}/src/well.c
                   ${PROJECT_SOURCE_DIR}/src/window.c)

# Assume MINGW uses POSIX threads
if(WIN32 AND NOT MINGW)
	set(HAMMER_SOURCES ${HAMMER_SOURCES}
	                   ${PROJECT_SOURCE_DIR}/src/win32/glthread.c)
else()
	set(HAMMER_SOURCES ${HAMMER_SOURCES}
	                   ${PROJECT_SOURCE_DIR}/src/posix/glthread.c)
endif()

add_executable(hammer ${HAMMER_SOURCES})
target_include_directories(hammer PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(hammer m
                             cglm
                             deadlock
                             delaunay
                             PNG::PNG
                             OpenGL::GL
                             GLEW::GLEW
                             SDL2
                             SDL2_image)
target_compile_definitions(hammer
                           PRIVATE "HAMMER_VERSION_MAJOR=${hammer_VERSION_MAJOR}"
                           PRIVATE "HAMMER_VERSION_MINOR=${hammer_VERSION_MINOR}"
                           PRIVATE "HAMMER_VERSION_PATCH=${hammer_VERSION_PATCH}")

if(WIN32)
	# Disable warnings about fopen in VS2019
	target_compile_definitions(hammer PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_custom_target(hammer_copy_resources
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                           ${PROJECT_SOURCE_DIR}/resources/
                           ${CMAKE_CURRENT_BINARY_DIR}/resources/)
add_custom_target(hammer_build_resources
                   COMMAND ${PROJECT_SOURCE_DIR}/scripts/generate-atlas
                           ${CMAKE_CURRENT_BINARY_DIR}/resources/)
add_dependencies(hammer_build_resources hammer_copy_resources)
add_dependencies(hammer hammer_build_resources)

option(HAMMER_BUILD_SAN "Build with sanitizers" OFF)
if(HAMMER_BUILD_SAN)
	target_compile_options(hammer PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
	target_link_options   (hammer PUBLIC -fsanitize=address,leak,undefined,pointer-compare,pointer-subtract -fstack-protector)
endif()

option(HAMMER_BUILD_TUNE "Build for this architecture" OFF)
if(HAMMER_BUILD_TUNE)
	target_compile_options(hammer PUBLIC -march=native)
endif()

option(HAMMER_DEBUG_OPENGL "Write OpenGL debug information to stderr" OFF)
if(HAMMER_DEBUG_OPENGL)
	target_compile_definitions(hammer PRIVATE HAMMER_DEBUG_OPENGL)
endif()
