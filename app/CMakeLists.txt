cmake_minimum_required(VERSION 3.16)
project(LogosModuleViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets RemoteObjects)

# Header-only; pulled in transitively by logos_api_client.h.
find_package(nlohmann_json REQUIRED)

# ── liblogos_core ────────────────────────────────────────────────────────────
# Provides the C runtime API (logos_core_*) AND, post Qt-split, the aggregated
# LogosAPI / LogosAPIClient symbols. Headers (logos_core.h, logos_api.h,
# logos_api_client.h) ship under include/ and include/cpp/.
if(NOT DEFINED LOGOS_LIBLOGOS_ROOT)
    message(FATAL_ERROR "LOGOS_LIBLOGOS_ROOT not set")
endif()
message(STATUS "Using LOGOS_LIBLOGOS_ROOT: ${LOGOS_LIBLOGOS_ROOT}")

find_library(LOGOS_CORE_LIB logos_core
    PATHS ${LOGOS_LIBLOGOS_ROOT}/lib NO_DEFAULT_PATH)
if(NOT LOGOS_CORE_LIB)
    message(FATAL_ERROR "liblogos_core not found in ${LOGOS_LIBLOGOS_ROOT}/lib")
endif()
message(STATUS "Found liblogos_core: ${LOGOS_CORE_LIB}")

add_library(logos_core SHARED IMPORTED)
set_target_properties(logos_core PROPERTIES
    IMPORTED_LOCATION "${LOGOS_CORE_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${LOGOS_LIBLOGOS_ROOT}/include;${LOGOS_LIBLOGOS_ROOT}/include/cpp")

# ── logos-module (introspection library) ─────────────────────────────────────
# ModuleLib::LogosModule reads a plugin's metadata and methods, handling both
# legacy Q_INVOKABLE plugins and modern universal/cdylib provider plugins.
if(NOT DEFINED LOGOS_MODULE_ROOT)
    message(FATAL_ERROR "LOGOS_MODULE_ROOT not set")
endif()
message(STATUS "Using LOGOS_MODULE_ROOT: ${LOGOS_MODULE_ROOT}")

find_library(LOGOS_MODULE_LIB logos_module
    PATHS ${LOGOS_MODULE_ROOT}/lib NO_DEFAULT_PATH)
if(NOT LOGOS_MODULE_LIB)
    message(FATAL_ERROR "liblogos_module not found in ${LOGOS_MODULE_ROOT}/lib")
endif()
message(STATUS "Found liblogos_module: ${LOGOS_MODULE_LIB}")

add_library(logos_module STATIC IMPORTED)
set_target_properties(logos_module PROPERTIES
    IMPORTED_LOCATION "${LOGOS_MODULE_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${LOGOS_MODULE_ROOT}/include/module_lib")

# ── Optional: logos-qt-mcp QObject inspector (for headless UI doc-tests) ───────
# When enabled, the GUI embeds InspectorServer (a generic QObject-tree TCP
# inspector) so logos-doctest's ui_test can drive the running app and capture
# screenshots. Off by default; the nix build turns it on when logos-qt-mcp is
# available. Same mechanism logos-standalone-app uses.
if(NOT DEFINED ENABLE_QML_INSPECTOR)
    set(ENABLE_QML_INSPECTOR OFF)
endif()
if(ENABLE_QML_INSPECTOR)
    if(NOT DEFINED LOGOS_QT_MCP_ROOT OR NOT EXISTS "${LOGOS_QT_MCP_ROOT}/qt-plugin/CMakeLists.txt")
        message(FATAL_ERROR "ENABLE_QML_INSPECTOR=ON but logos-qt-mcp not found at "
                            "LOGOS_QT_MCP_ROOT='${LOGOS_QT_MCP_ROOT}'")
    endif()
    message(STATUS "QObject inspector enabled (LOGOS_QT_MCP_ROOT=${LOGOS_QT_MCP_ROOT})")
    add_subdirectory("${LOGOS_QT_MCP_ROOT}/qt-plugin" "${CMAKE_CURRENT_BINARY_DIR}/qml-inspector")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(logos-module-viewer
    main.cpp
    mainwindow.cpp
    mainwindow.h
    viewer_core.cpp
    viewer_core.h
)

if(ENABLE_QML_INSPECTOR)
    target_compile_definitions(logos-module-viewer PRIVATE ENABLE_QML_INSPECTOR)
endif()

target_link_libraries(logos-module-viewer PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    nlohmann_json::nlohmann_json
    logos_module
    logos_core
    $<$<BOOL:${ENABLE_QML_INSPECTOR}>:qml-inspector>
)

if(APPLE)
    set_target_properties(logos-module-viewer PROPERTIES
        INSTALL_RPATH "@executable_path/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE)
elseif(UNIX)
    set_target_properties(logos-module-viewer PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE)
endif()

install(TARGETS logos-module-viewer
    RUNTIME DESTINATION bin)
