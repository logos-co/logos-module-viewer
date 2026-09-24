cmake_minimum_required(VERSION 3.16)
project(LogosModuleViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets RemoteObjects)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets RemoteObjects)

# Find liblogos_core
if(DEFINED LOGOS_LIBLOGOS_ROOT)
    message(STATUS "Using LOGOS_LIBLOGOS_ROOT: ${LOGOS_LIBLOGOS_ROOT}")
    find_library(LOGOS_CORE_LIB logos_core PATHS ${LOGOS_LIBLOGOS_ROOT}/lib NO_DEFAULT_PATH)
    if(LOGOS_CORE_LIB)
        message(STATUS "Found liblogos_core: ${LOGOS_CORE_LIB}")
    else()
        message(FATAL_ERROR "liblogos_core not found in ${LOGOS_LIBLOGOS_ROOT}/lib")
    endif()
else()
    message(FATAL_ERROR "LOGOS_LIBLOGOS_ROOT not set")
endif()

# The Qt host runtime (LogosAPI, TokenManager), from its own packages:
# liblogos' Qt-free core does not ship it. logos_qt_host_shared brings
# logos_protocol_shared with it.
foreach(_root LOGOS_PROTOCOL_ROOT LOGOS_QT_HOST_ROOT)
    if(NOT DEFINED ${_root})
        message(FATAL_ERROR "${_root} not set")
    endif()
endforeach()
find_package(logos-protocol REQUIRED CONFIG
    PATHS ${LOGOS_PROTOCOL_ROOT}/lib/cmake/logos-protocol NO_DEFAULT_PATH)
find_package(logos-qt-host REQUIRED CONFIG
    PATHS ${LOGOS_QT_HOST_ROOT}/lib/cmake/logos-qt-host NO_DEFAULT_PATH)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(logos-module-viewer
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_include_directories(logos-module-viewer PRIVATE
    ${LOGOS_LIBLOGOS_ROOT}/include
)

target_link_libraries(logos-module-viewer PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    ${LOGOS_CORE_LIB}
    logos-qt-host::logos_qt_host_shared
)

if(APPLE)
    set_target_properties(logos-module-viewer PROPERTIES
        INSTALL_RPATH "@executable_path/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
elseif(UNIX)
    set_target_properties(logos-module-viewer PROPERTIES
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH TRUE
    )
endif()

install(TARGETS logos-module-viewer
    RUNTIME DESTINATION bin
)
