cmake_minimum_required(VERSION 3.20)

project(usbipd-tui
  LANGUAGES CXX
  VERSION 0.1.0
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Static libraries: no extra DLLs needed next to the executable.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Keep the executable in one predictable place: <build>/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
if(WIN32)
  foreach(config ${CMAKE_CONFIGURATION_TYPES})
    string(TOUPPER "${config}" config_upper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${CMAKE_BINARY_DIR}/bin")
  endforeach()
endif()

include(FetchContent)

# Prefer sources already present under third_party/<name>. When they are there,
# FetchContent skips the download; the binaries still go to
# build/_deps/<name>-build.
function(usbipd_use_local_dep name)
  string(TOUPPER "${name}" name_upper)
  set(local_source "${CMAKE_SOURCE_DIR}/third_party/${name}")
  if(EXISTS "${local_source}/CMakeLists.txt")
    message(STATUS "${name}: using local source third_party/${name}")
    set(FETCHCONTENT_SOURCE_DIR_${name_upper}
        "${local_source}" CACHE PATH "" FORCE)
  else()
    message(STATUS "${name}: no local source, FetchContent will download it")
    # Drop a stale cache entry left by an earlier configure.
    unset(FETCHCONTENT_SOURCE_DIR_${name_upper} CACHE)
  endif()
endfunction()

usbipd_use_local_dep(FTXUI)
usbipd_use_local_dep(spdlog)
usbipd_use_local_dep(nlohmann_json)

# FTXUI: build the libraries only. Skip its examples, docs, tests and C++20
# module interface units, and do not emit install rules for it.
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_MODULES  OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FTXUI_QUIET          ON  CACHE BOOL "" FORCE)

FetchContent_Declare(
  FTXUI
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
  GIT_TAG        v7.0.3
  GIT_SHALLOW    TRUE
)

# spdlog: build the library only, and reuse its bundled fmt instead of pulling
# in another dependency.
set(SPDLOG_BUILD_EXAMPLE  OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_BENCH    OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_WARNINGS OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL        OFF CACHE BOOL "" FORCE)
set(SPDLOG_FMT_EXTERNAL   OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.17.0
  GIT_SHALLOW    TRUE
)

# nlohmann/json: header-only JSON library used to parse `usbipd state` output.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.12.0
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(FTXUI spdlog nlohmann_json)

# Keep a copy of freshly downloaded sources in third_party, so the next
# configure finds them there and skips the download. The binaries stay in
# build/_deps/<name>-build either way.
function(usbipd_cache_dep_sources name source_dir)
  set(target_dir "${CMAKE_SOURCE_DIR}/third_party/${name}")
  if(EXISTS "${target_dir}/CMakeLists.txt")
    return()
  endif()

  string(TOLOWER "${name}" name_lower)
  if(NOT source_dir)
    set(source_dir "${FETCHCONTENT_BASE_DIR}/${name_lower}-src")
  endif()
  # Never copy from an unexpected location.
  if(NOT EXISTS "${source_dir}/CMakeLists.txt")
    return()
  endif()

  message(STATUS "${name}: saving a copy of the sources to third_party/${name}")
  # Skip VCS metadata and the dependency's own .gitignore: it would fight the
  # one in third_party (deeper .gitignore files take precedence).
  file(COPY "${source_dir}/" DESTINATION "${target_dir}"
       PATTERN ".git" EXCLUDE
       PATTERN ".gitignore" EXCLUDE)
endfunction()

usbipd_cache_dep_sources(FTXUI "${ftxui_SOURCE_DIR}")
usbipd_cache_dep_sources(spdlog "${spdlog_SOURCE_DIR}")
usbipd_cache_dep_sources(nlohmann_json "${nlohmann_json_SOURCE_DIR}")

# Every .cpp under src/ becomes part of the executable.
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

if(NOT APP_SOURCES)
  message(FATAL_ERROR "No .cpp sources found under ${CMAKE_CURRENT_SOURCE_DIR}/src")
endif()

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
  ftxui::ftxui
  spdlog::spdlog
  nlohmann_json::nlohmann_json
)

# MinGW defaults to dynamically linked runtime libraries. Static linking avoids
# requiring libwinpthread-1.dll, libgcc_s_*.dll or libstdc++-6.dll at runtime.
if(MINGW)
  target_link_options(${PROJECT_NAME} PRIVATE
    -static -static-libgcc -static-libstdc++
  )
endif()

if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4 /utf-8)
else()
  target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()
