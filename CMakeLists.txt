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

# FTXUI: build the libraries only. Skip its examples, docs, tests and C++20
# module interface units, and do not emit install rules for it.
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_MODULES  OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(FTXUI_QUIET          ON  CACHE BOOL "" FORCE)

add_subdirectory(third_party/FTXUI EXCLUDE_FROM_ALL)

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

target_link_libraries(${PROJECT_NAME} PRIVATE ftxui::ftxui)

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
