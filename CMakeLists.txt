# WinDoom: Windows x64 build of Linux DOOM 1.10.
# Copyright (C) 2026 Nikolai Zhivotenko. Licensed under GPLv2 (LICENSE.TXT).
cmake_minimum_required(VERSION 3.20)
project(windoom C CXX)

if(NOT WIN32)
  message(FATAL_ERROR "windoom is a Windows x64 target")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DOOM_DIR ${CMAKE_SOURCE_DIR}/linuxdoom-1.10)

set(DOOM_SOURCES
  ${DOOM_DIR}/am_map.c
  ${DOOM_DIR}/d_items.c
  ${DOOM_DIR}/d_main.c
  ${DOOM_DIR}/d_net.c
  ${DOOM_DIR}/doomdef.c
  ${DOOM_DIR}/doomstat.c
  ${DOOM_DIR}/dstrings.c
  ${DOOM_DIR}/f_finale.c
  ${DOOM_DIR}/f_wipe.c
  ${DOOM_DIR}/g_game.c
  ${DOOM_DIR}/hu_lib.c
  ${DOOM_DIR}/hu_stuff.c
  ${DOOM_DIR}/info.c
  ${DOOM_DIR}/language.c
  ${DOOM_DIR}/m_argv.c
  ${DOOM_DIR}/m_bbox.c
  ${DOOM_DIR}/m_cheat.c
  ${DOOM_DIR}/m_fixed.c
  ${DOOM_DIR}/m_menu.c
  ${DOOM_DIR}/m_misc.c
  ${DOOM_DIR}/m_random.c
  ${DOOM_DIR}/m_swap.c
  ${DOOM_DIR}/p_ceilng.c
  ${DOOM_DIR}/p_doors.c
  ${DOOM_DIR}/p_enemy.c
  ${DOOM_DIR}/p_floor.c
  ${DOOM_DIR}/p_inter.c
  ${DOOM_DIR}/p_lights.c
  ${DOOM_DIR}/p_map.c
  ${DOOM_DIR}/p_maputl.c
  ${DOOM_DIR}/p_mobj.c
  ${DOOM_DIR}/p_plats.c
  ${DOOM_DIR}/p_pspr.c
  ${DOOM_DIR}/p_saveg.c
  ${DOOM_DIR}/p_setup.c
  ${DOOM_DIR}/p_sight.c
  ${DOOM_DIR}/p_spec.c
  ${DOOM_DIR}/p_switch.c
  ${DOOM_DIR}/p_telept.c
  ${DOOM_DIR}/p_tick.c
  ${DOOM_DIR}/p_user.c
  ${DOOM_DIR}/r_bsp.c
  ${DOOM_DIR}/r_data.c
  ${DOOM_DIR}/r_draw.c
  ${DOOM_DIR}/r_main.c
  ${DOOM_DIR}/r_plane.c
  ${DOOM_DIR}/r_segs.c
  ${DOOM_DIR}/r_sky.c
  ${DOOM_DIR}/r_things.c
  ${DOOM_DIR}/s_sound.c
  ${DOOM_DIR}/sounds.c
  ${DOOM_DIR}/st_lib.c
  ${DOOM_DIR}/st_stuff.c
  ${DOOM_DIR}/tables.c
  ${DOOM_DIR}/v_video.c
  ${DOOM_DIR}/w_wad.c
  ${DOOM_DIR}/wi_stuff.c
  ${DOOM_DIR}/z_zone.c
)

add_executable(windoom
  ${DOOM_SOURCES}
  win32/gbuffer.c
  win32/dxr_diagnostic.cpp
  win32/render_adapter.c
  win32/gpu_timing.c
  win32/i_main_win.c
  win32/i_net_win.c
  win32/i_sound_win.c
  win32/i_system_win.c
  win32/i_video_win.c
  win32/ngx_dlss.c
  win32/ngx_runtime.c
  win32/anime4k.c
  win32/fsr2.c
  win32/png_export.c
)

target_include_directories(windoom PRIVATE
  ${DOOM_DIR}
  ${CMAKE_SOURCE_DIR}/win32
  ${CMAKE_SOURCE_DIR}/win32/compat
)

target_compile_definitions(windoom PRIVATE
  NORMALUNIX
  LINUX
  _CRT_SECURE_NO_WARNINGS
  NOMINMAX
)

if(MSVC)
  target_compile_options(windoom PRIVATE
    /W3
    /wd4244
    /wd4267
    /wd4018
    /wd4305
    /wd4996
    /wd4101
    /FIstrings.h
    /FIalloca.h
  )
endif()

target_link_libraries(windoom PRIVATE
  d3d12
  dxgi
  dxguid
  ole32
  uuid
  windowscodecs
  bcrypt
  version
)

option(WINDOOM_NGX_DIAGNOSTICS "Enable explicitly labelled NGX failure injection for acceptance captures" OFF)
option(WINDOOM_NGX "Link NVIDIA NGX (needs a local SDK; off by default)" OFF)
option(WINDOOM_ANIME4K "Compile Anime4K Fast present path (no NVIDIA)" OFF)
option(WINDOOM_FSR2 "Compile AMD FSR 2.2 present path (needs fetched sources)" OFF)
set(NGX_SDK_DIR "" CACHE PATH "NVIDIA NGX SDK root (headers and lib)")

set(_wd_present 0)
if(WINDOOM_NGX)
  math(EXPR _wd_present "${_wd_present}+1")
endif()
if(WINDOOM_ANIME4K)
  math(EXPR _wd_present "${_wd_present}+1")
endif()
if(WINDOOM_FSR2)
  math(EXPR _wd_present "${_wd_present}+1")
endif()
if(_wd_present GREATER 1)
  message(FATAL_ERROR
    "WINDOOM_NGX, WINDOOM_ANIME4K, and WINDOOM_FSR2 cannot be combined. "
    "They each own present. Build separate folders via build.cmd.")
endif()

if(WINDOOM_ANIME4K)
  message(STATUS "Anime4K Fast enabled")
  target_compile_definitions(windoom PRIVATE WINDOOM_HAS_ANIME4K)
  target_link_libraries(windoom PRIVATE d3dcompiler)
endif()

if(WINDOOM_NGX)
  if(NOT NGX_SDK_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/ngx/include/nvsdk_ngx.h")
    set(NGX_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/ngx")
  endif()
  find_path(NGX_INCLUDE_DIR nvsdk_ngx.h
    HINTS "${NGX_SDK_DIR}"
    PATH_SUFFIXES include)
  find_library(NGX_LIBRARY NAMES nvsdk_ngx_d nvsdk_ngx_s nvsdk_ngx
    HINTS "${NGX_SDK_DIR}"
    PATH_SUFFIXES lib/Windows_x86_64/x64 lib/x64 lib)
  if(NGX_INCLUDE_DIR AND NGX_LIBRARY)
    message(STATUS "NGX enabled: ${NGX_INCLUDE_DIR}")
    set(_ngx_build_tag "unrecorded")
    set(_ngx_build_commit "unrecorded")
    if(EXISTS "${NGX_INCLUDE_DIR}/../ngx-install.json")
      file(STRINGS "${NGX_INCLUDE_DIR}/../ngx-install.json" _ngx_sdk_lines REGEX "\"(sdkTag|commit)\"")
      foreach(_ngx_line IN LISTS _ngx_sdk_lines)
        if(_ngx_line MATCHES "\"sdkTag\"[ ]*:[ ]*\"(v[0-9.]+)\"")
          set(_ngx_build_tag "${CMAKE_MATCH_1}")
        elseif(_ngx_line MATCHES "\"commit\"[ ]*:[ ]*\"([a-f0-9]+)\"")
          set(_ngx_build_commit "${CMAKE_MATCH_1}")
        endif()
      endforeach()
    endif()
    target_compile_definitions(windoom PRIVATE WINDOOM_HAS_NGX
      WINDOOM_NGX_BUILD_SDK_TAG="${_ngx_build_tag}"
      WINDOOM_NGX_BUILD_SDK_COMMIT="${_ngx_build_commit}")
    if(WINDOOM_NGX_DIAGNOSTICS)
      target_compile_definitions(windoom PRIVATE WINDOOM_NGX_DIAGNOSTICS)
    endif()
    target_include_directories(windoom PRIVATE ${NGX_INCLUDE_DIR})
    target_link_libraries(windoom PRIVATE ${NGX_LIBRARY})
  else()
    message(FATAL_ERROR
      "WINDOOM_NGX=ON but the SDK was not found. Run fetch-ngx.cmd first "
      "or pass -DNGX_SDK_DIR=...")
  endif()
else()
  message(STATUS "NGX disabled. Use build.cmd --ngx-dlss3.5 / --ngx-dlss4 / --ngx-dlss4.5 / --ngx-dlss5 / --anime4k / --fsr2.")
endif()

if(WINDOOM_FSR2)
  enable_language(CXX)
  include("${CMAKE_SOURCE_DIR}/cmake/FSR2.cmake")
  message(STATUS "FSR2 2.2.1 enabled")
  target_compile_definitions(windoom PRIVATE WINDOOM_HAS_FSR2)
  set_source_files_properties(win32/fsr2.c PROPERTIES LANGUAGE CXX)
  set_target_properties(windoom PROPERTIES LINKER_LANGUAGE CXX)
  if(MSVC)
    target_compile_options(windoom PRIVATE /EHsc)
  endif()
  target_link_libraries(windoom PRIVATE ffx_fsr2_api ffx_fsr2_dx12)
endif()

# Resource lookup is relative to the executable, including direct CMake builds.
add_custom_command(TARGET windoom POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/languages" "$<TARGET_FILE_DIR:windoom>/languages"
)

# Inline ray queries require DXIL/Shader Model 6.5. Missing DXC leaves normal
# rendering available; runtime diagnostic reports the absent shader.
option(WINDOOM_DXR "Build the DXR inline-ray diagnostic shader when DXC is available" ON)
if(WINDOOM_DXR)
  file(GLOB _wd_dxc_paths "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
  list(SORT _wd_dxc_paths ORDER DESCENDING)
  find_program(DXC_EXECUTABLE NAMES dxc.exe dxc HINTS ${_wd_dxc_paths})
  if(DXC_EXECUTABLE)
    # Generate directly beside the executable: shader-only rebuilds must also
    # refresh the runtime asset even when windoom does not relink.
    get_target_property(_wd_runtime_dir windoom RUNTIME_OUTPUT_DIRECTORY)
    if(NOT _wd_runtime_dir)
      set(_wd_runtime_dir "${CMAKE_CURRENT_BINARY_DIR}")
    endif()
    if(CMAKE_CONFIGURATION_TYPES AND NOT _wd_runtime_dir MATCHES "\\$<")
      string(APPEND _wd_runtime_dir "/$<CONFIG>")
    endif()
    set(_wd_dxr_shader "${_wd_runtime_dir}/shaders/dxr_diagnostic.cso")
    add_custom_command(OUTPUT "${_wd_dxr_shader}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_wd_runtime_dir}/shaders"
      COMMAND "${DXC_EXECUTABLE}" -T cs_6_5 -E main -O3
        -Fo "${_wd_dxr_shader}" "${CMAKE_CURRENT_SOURCE_DIR}/win32/shaders/dxr/diagnostic.hlsl"
      DEPENDS win32/shaders/dxr/diagnostic.hlsl VERBATIM)
    add_custom_target(windoom_dxr_shader DEPENDS "${_wd_dxr_shader}")
    add_dependencies(windoom windoom_dxr_shader)
    message(STATUS "DXR diagnostic DXC: ${DXC_EXECUTABLE}")
  else()
    message(STATUS "DXC absent; normal rendering works, DXR diagnostic shader unavailable")
  endif()
endif()
