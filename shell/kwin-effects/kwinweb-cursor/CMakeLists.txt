cmake_minimum_required(VERSION 3.19)

project(caelestia-webcursor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
add_compile_options(-fexceptions)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(PkgConfig REQUIRED)
pkg_check_modules(EPOXY REQUIRED IMPORTED_TARGET epoxy)

set(ULTRALIGHT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ThirdParty"
    CACHE PATH "Path to an extracted Ultralight SDK")
set(ULTRALIGHT_BIN "${ULTRALIGHT_ROOT}/bin")

if(NOT EXISTS "${ULTRALIGHT_ROOT}/include/AppCore/App.h")
    message(FATAL_ERROR "Ultralight SDK not found: ${ULTRALIGHT_ROOT}")
endif()

function(ultralight_import_library target_name lib_basename)
    add_library(${target_name} SHARED IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION "${ULTRALIGHT_BIN}/lib${lib_basename}.so"
        INTERFACE_INCLUDE_DIRECTORIES "${ULTRALIGHT_ROOT}/include"
    )
endfunction()

ultralight_import_library(Ultralight::Ultralight Ultralight)
ultralight_import_library(Ultralight::UltralightCore UltralightCore)
ultralight_import_library(Ultralight::AppCore AppCore)
ultralight_import_library(Ultralight::WebCore WebCore)

add_library(Ultralight::SDK INTERFACE IMPORTED)
target_link_libraries(Ultralight::SDK INTERFACE
    Ultralight::Ultralight
    Ultralight::UltralightCore
    Ultralight::AppCore
    Ultralight::WebCore
)

set(CMAKE_BUILD_RPATH "${ULTRALIGHT_BIN}")
set(CMAKE_INSTALL_RPATH "/usr/lib/webkde_core")

find_package(Qt6 6.6 REQUIRED COMPONENTS Core Gui DBus Widgets)

find_package(ECM 6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(KF6 REQUIRED COMPONENTS CoreAddons Config)
find_package(KWin REQUIRED)


add_library(webkde_core OBJECT
    config/CursorJSON.cpp
    config/UserConfig.cpp
    lib/Quick/PluginPath/PluginPath.cpp
)
set_target_properties(webkde_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(webkde_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/header
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/Quick/PluginPath
)
target_link_libraries(webkde_core PUBLIC Qt6::Core Qt6::DBus )

add_subdirectory(kwin-plasma)

install(DIRECTORY WebCursor/
    DESTINATION "${CMAKE_INSTALL_DATADIR}/kwin/effects/ultralightwebcursor")
install(DIRECTORY "${ULTRALIGHT_BIN}/"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/webkde_core"
    FILES_MATCHING PATTERN "*.so*")
install(DIRECTORY "${ULTRALIGHT_ROOT}/resources"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/kwin/effects/ultralightwebcursor")
