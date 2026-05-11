cmake_minimum_required(VERSION 3.22)
project(qtmonitor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network WebSockets)
find_package(Qt6 COMPONENTS Mqtt QUIET)
if(NOT Qt6Mqtt_FOUND)
    message(STATUS "Qt6Mqtt not found, building from source (requires Qt6WebSockets)")
    include(FetchContent)
    FetchContent_Declare(qtmqtt
        GIT_REPOSITORY https://github.com/qt/qtmqtt.git
        GIT_TAG v${Qt6_VERSION}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(qtmqtt)
endif()

add_executable(qtmonitor WIN32
    resources.qrc
    main.cpp
    mainwindow.cpp
    mqttclient.cpp
    logwidget.cpp
    nodewidget.cpp
    clientwidget.cpp
    statswidget.cpp
    loginattemptswidget.cpp
    maxconcurrentwidget.cpp
    actionwidget.cpp
    settingsdialog.cpp
)

target_link_libraries(qtmonitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Mqtt
)

qt_finalize_target(qtmonitor)
install(TARGETS qtmonitor DESTINATION bin)
