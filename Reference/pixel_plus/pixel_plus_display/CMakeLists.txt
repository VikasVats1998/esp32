# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# esp-sr movemodel.py invokes bare `python`; shim in tools/ forwards to the IDF venv (Windows).
get_filename_component(PROJECT_TOOLS "${CMAKE_CURRENT_LIST_DIR}/tools" ABSOLUTE)
set(ENV{PATH} "${PROJECT_TOOLS};$ENV{PATH}")
if(DEFINED ENV{IDF_PYTHON_ENV_PATH})
    set(ENV{PATH} "$ENV{IDF_PYTHON_ENV_PATH}/Scripts;$ENV{PATH}")
endif()

# Re-apply after component manager restores managed_components (Windows cp1252 + movemodel.py).
include(${CMAKE_CURRENT_LIST_DIR}/cmake/patch_esp_sr_movemodel.cmake)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pixel_plus_display)
