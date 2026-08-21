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
    # The shared runtime, which liblogos_core no longer DEFINES.
    #
    # liblogos_core used to absorb liblogos_protocol.a and liblogos_qt_host.a
    # whole and re-export them, so linking it alone resolved LogosAPIClient,
    # TokenManager and LogosAPI. Since logos-liblogos#182 it imports them from
    # real shared libraries instead, and a consumer that links only
    # liblogos_core gets an undefined reference at link time:
    #
    #   Undefined symbols: LogosAPIClient::onEvent(LogosObject*, QString const&, ...)
    #
    # Linking the libraries that own the types is the whole point of that
    # migration -- it is what makes each of them exist exactly once in the
    # process. See logos-protocol/cpp/logos_shared_api.h.
    find_library(LOGOS_PROTOCOL_LIB logos_protocol PATHS ${LOGOS_LIBLOGOS_ROOT}/lib NO_DEFAULT_PATH)
    find_library(LOGOS_QT_HOST_LIB logos_qt_host PATHS ${LOGOS_LIBLOGOS_ROOT}/lib NO_DEFAULT_PATH)
    if(NOT LOGOS_PROTOCOL_LIB OR NOT LOGOS_QT_HOST_LIB)
        message(FATAL_ERROR
            "liblogos_protocol / liblogos_qt_host not found in ${LOGOS_LIBLOGOS_ROOT}/lib. "
            "They arrive with logos-liblogos#182; against an older liblogos this "
            "target would link only liblogos_core and fail on the runtime symbols "
            "it no longer defines.")
    endif()
    message(STATUS "Found shared runtime: ${LOGOS_PROTOCOL_LIB}, ${LOGOS_QT_HOST_LIB}")
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
    ${LOGOS_QT_HOST_LIB}
    ${LOGOS_PROTOCOL_LIB}
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
