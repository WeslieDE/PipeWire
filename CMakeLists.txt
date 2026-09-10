cmake_minimum_required(VERSION 3.21)
project(MixPipe VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets WebChannel)

find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3)

add_executable(mixpipe
    src/main.cpp
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    src/backend/AudioGraph.cpp
    src/backend/AudioGraph.h
    src/backend/PipeWireEngine.cpp
    src/backend/PipeWireEngine.h
    src/backend/LinkController.cpp
    src/backend/LinkController.h
    src/backend/VolumeController.cpp
    src/backend/VolumeController.h
    src/backend/VirtualDeviceManager.cpp
    src/backend/VirtualDeviceManager.h
    resources.qrc
)

target_include_directories(mixpipe PRIVATE src)

target_link_libraries(mixpipe PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::WebChannel
    PkgConfig::PIPEWIRE
)

install(TARGETS mixpipe RUNTIME DESTINATION bin)
