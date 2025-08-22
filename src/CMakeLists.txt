# Copyright (c) 2025 acrion innovations GmbH
# Authors: Stefan Zipproth, s.zipproth@acrion.ch
#
# This file is part of nexuslua, see https://github.com/acrion/nexuslua_executable and https://nexuslua.org
#
# nexuslua is offered under a commercial and under the AGPL license.
# For commercial licensing, contact us at https://acrion.ch/sales. For AGPL licensing, see below.
#
# AGPL licensing:
#
# nexuslua is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# nexuslua is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with nexuslua. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.25.2)
cmake_policy(VERSION 3.25.2)

include(FetchContent)

FetchContent_Declare(
    acrion_cmake
    GIT_REPOSITORY https://github.com/acrion/cmake.git
    GIT_TAG main
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(acrion_cmake)
if (NOT acrion_cmake_POPULATED)
    FetchContent_MakeAvailable(acrion_cmake)
endif ()

include(${acrion_cmake_SOURCE_DIR}/set-cpp-version.cmake)

include(${acrion_cmake_SOURCE_DIR}/get-project-version-from-git-tag.cmake)
get_project_version_from_git_tag("origin/main")

# The following line sets variable PROJECT_VERSION, which is referenced in config.hpp.cmake
project(nexuslua_executable VERSION ${version_major}.${version_minor}.${version_patch} DESCRIPTION "nexuslua executable")

if (NOT TARGET nexuslua_library)
    FetchContent_Declare(
        nexuslua_library
        GIT_REPOSITORY https://github.com/acrion/nexuslua-library.git
        GIT_TAG v0.9.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(nexuslua_library)
endif ()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.hpp.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.hpp)

add_executable(${PROJECT_NAME}
    ${PlatformSpecificSources}
    config.hpp.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.hpp
    main.cpp
    test.cpp
)

include(${acrion_cmake_SOURCE_DIR}/set-compiler-flags.cmake)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# PDB_NAME: Avoid conflict with nexuslua library PDB name, as both projects share the same output directory and Visual Studio generates linker PDB files in the same directory as the executables and DLLs.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "nexuslua" PDB_NAME ${PROJECT_NAME})

include(${acrion_cmake_SOURCE_DIR}/do-logging.cmake)

# if (NOT WIN32)
#     # patch library paths to point to @loader_path/... so we use our own build.
#     add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD COMMAND bash "${acrion_cmake_SOURCE_DIR}/patch-rpaths.sh" $<TARGET_FILE:${PROJECT_NAME}>)
# endif ()

if (GENERATED_WITH_DBG_CONFIGURATION) # is set by set-compiler-flags.cmake
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )

    if (WIN32)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    endif ()

    FetchContent_MakeAvailable(googletest)
    enable_testing()

    set(debugging_libraries GTest::gtest)
    include(GoogleTest)
    #gtest_discover_tests(${PROJECT_NAME})

    include(${acrion_cmake_SOURCE_DIR}/run-tests.cmake)
endif ()

target_link_libraries(${PROJECT_NAME} PRIVATE nexuslua::nexuslua ${debugging_libraries})

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
