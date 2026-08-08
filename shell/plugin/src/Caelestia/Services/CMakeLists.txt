find_package(KF6GlobalAccel REQUIRED)
find_package(KF6Config REQUIRED)
find_package(X11 REQUIRED)
# For QPlatformNativeInterface, the only way to reach a QWindow's wl_surface:
# Qt exposes no public native interface for it. Ships with qt6-base on Arch and
# with qt6-qtbase-private-devel on Fedora, both already installed by setup.
find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
qml_module(caelestia-services
    URI Caelestia.Services
    SOURCES
        service.cpp
        serviceref.cpp
        beattracker.cpp
        audiocollector.cpp
        audioprovider.cpp
        cavaprovider.cpp
        tickingservice.cpp
        sensorslib.cpp
        usagefmt.cpp
        cpu.cpp
        gpu.cpp
        memory.cpp
        diskinfo.cpp
        storage.cpp
        lyriccandidate.cpp
        lyrics.cpp
        sessionmanager.cpp
        sessionmanager.hpp
        discordipc.cpp
        discordipc.hpp
        hyprlandstate.cpp
        hyprlandstate.hpp
        keybindsmodel.cpp
        keybindsmodel.hpp
        emojidb.cpp
        emojidb.hpp
        brightnesswatcher.cpp
        brightnesswatcher.hpp
        clipboardmanager.cpp
        clipboardmanager.hpp
        schemeloader.cpp
        schemeloader.hpp
        palettemanager.cpp
        palettemanager.hpp
        dinogame.cpp
        dinogame.hpp
        kwinworkspacestate.cpp
        kwinworkspacestate.hpp
        kwinactivewindowbridge.cpp
        windowicon.cpp
        kwinactivewindowbridge.hpp
        nightcolorbridge.cpp
        nightcolorbridge.hpp
        globalshortcut.cpp
        globalshortcut.hpp
        krohnkiteconfig.cpp
        krohnkiteconfig.hpp
        windowscreencast.cpp
        windowscreencast.hpp
        windowicon.hpp
        minimizegeometry.cpp
        minimizegeometry.hpp
        plasmawindows.cpp
        plasmawindows.hpp
        plasmawindowicon.cpp
        plasmawindowicon.hpp
        screenedges.cpp
        screenedges.hpp
    LIBRARIES
        Qt::DBus
        Qt::Gui
        Qt::Quick
        Qt::GuiPrivate
        Qt::WaylandClient
        KF6::GlobalAccel
        KF6::ConfigCore
        X11::X11
        PkgConfig::Pipewire
        PkgConfig::Aubio
        PkgConfig::Cava
        Sensors::Sensors
        caelestia-core
        caelestia-config
        caelestia-internal
)



qt6_generate_wayland_protocol_client_sources(caelestia-services FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/zkde-screencast-unstable-v1.xml
    ${CMAKE_CURRENT_SOURCE_DIR}/../protocols/plasma-window-management.xml
)
set_source_files_properties(
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-zkde-screencast-unstable-v1-protocol.c
    ${CMAKE_CURRENT_BINARY_DIR}/wayland-plasma-window-management-protocol.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)
