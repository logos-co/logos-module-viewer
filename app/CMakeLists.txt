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

# Find logos_sdk
if(DEFINED LOGOS_CPP_SDK_ROOT)
    message(STATUS "Using LOGOS_CPP_SDK_ROOT: ${LOGOS_CPP_SDK_ROOT}")
    find_library(LOGOS_SDK_LIB logos_sdk PATHS ${LOGOS_CPP_SDK_ROOT}/lib NO_DEFAULT_PATH)
    if(LOGOS_SDK_LIB)
        message(STATUS "Found liblogos_sdk: ${LOGOS_SDK_LIB}")
    else()
        message(FATAL_ERROR "liblogos_sdk not found in ${LOGOS_CPP_SDK_ROOT}/lib")
    endif()
else()
    message(FATAL_ERROR "LOGOS_CPP_SDK_ROOT not set")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(logos-module-viewer
    main.cpp
    mainwindow.cpp
    mainwindow.h
)

target_include_directories(logos-module-viewer PRIVATE
    ${LOGOS_LIBLOGOS_ROOT}/include
    ${LOGOS_CPP_SDK_ROOT}/include/cpp
    ${LOGOS_CPP_SDK_ROOT}/include/core
)

target_link_libraries(logos-module-viewer PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::RemoteObjects
    ${LOGOS_CORE_LIB}
    ${LOGOS_SDK_LIB}
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
