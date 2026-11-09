cmake_minimum_required(VERSION 3.21)

project(
	Skhroma
	VERSION 1.0.1
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

# ---------------------------------------------------------------------------
# CommonLibSSE-NG (vendored in external/, not tracked by git)
# Nolvus runs 1.5.97 (SE), but we keep AE enabled so the DLL also works on 1.6.x.
# ---------------------------------------------------------------------------
set(COMMONLIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/CommonLibSSE-NG" CACHE PATH "CommonLibSSE-NG source dir")
if(NOT EXISTS "${COMMONLIB_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "CommonLibSSE-NG not found at ${COMMONLIB_DIR}. Run:\n"
		"  git clone --recurse-submodules https://github.com/CharmedBaryon/CommonLibSSE-NG.git external/CommonLibSSE-NG")
endif()

set(ENABLE_SKYRIM_SE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${COMMONLIB_DIR}" CommonLibSSE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
file(GLOB_RECURSE SKHROMA_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(${PROJECT_NAME} SHARED ${SKHROMA_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
target_precompile_headers(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")

target_compile_definitions(${PROJECT_NAME} PRIVATE
	PLUGIN_NAME="${PROJECT_NAME}"
	PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
	PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
	NOMINMAX
	WIN32_LEAN_AND_MEAN
)

target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8 /MP)
target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Release,RelWithDebInfo>:/OPT:REF;/OPT:ICF;/DEBUG:FULL>")

# ---------------------------------------------------------------------------
# chroma_test: standalone smoke test of the Chroma layer (run outside the game)
# ---------------------------------------------------------------------------
add_executable(chroma_test tools/chroma_test.cpp src/Chroma.cpp)
target_include_directories(chroma_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(chroma_test PRIVATE CommonLibSSE::CommonLibSSE)
target_precompile_headers(chroma_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/PCH.h")
target_compile_definitions(chroma_test PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(chroma_test PRIVATE /W4 /permissive- /Zc:preprocessor /utf-8)

# ---------------------------------------------------------------------------
# Optional deploy: -DSKHROMA_DEPLOY_DIR=".../MODS/mods/Skhroma" copies the mod
# (DLL, PDB, INI) into an MO2 mod folder after each build.
# ---------------------------------------------------------------------------
set(SKHROMA_DEPLOY_DIR "" CACHE PATH "MO2 mod folder to deploy into (optional)")
if(SKHROMA_DEPLOY_DIR)
	add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory "${SKHROMA_DEPLOY_DIR}/SKSE/Plugins"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${PROJECT_NAME}>" "${SKHROMA_DEPLOY_DIR}/SKSE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_PDB_FILE:${PROJECT_NAME}>" "${SKHROMA_DEPLOY_DIR}/SKSE/Plugins/"
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/dist/SKSE/Plugins/Skhroma.ini" "${SKHROMA_DEPLOY_DIR}/SKSE/Plugins/"
		COMMENT "Deploying Skhroma to ${SKHROMA_DEPLOY_DIR}"
	)
endif()
