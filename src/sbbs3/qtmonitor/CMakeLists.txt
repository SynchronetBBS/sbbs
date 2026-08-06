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

set(QTMONITOR_SOURCES
    resources.qrc
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

add_executable(qtmonitor WIN32
    main.cpp
    ${QTMONITOR_SOURCES}
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

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 COMPONENTS Test QUIET)
    if(Qt6Test_FOUND)
        add_executable(qtmonitor_dock_test
            tests/dock_behavior_test.cpp
            ${QTMONITOR_SOURCES}
        )
        target_include_directories(qtmonitor_dock_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(qtmonitor_dock_test PRIVATE
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
            Qt6::Network
            Qt6::Mqtt
            Qt6::Test
        )
        add_test(NAME qtmonitor_dock_behavior COMMAND qtmonitor_dock_test)
        # The vcpkg Qt builds contain only their native platform plugin on
        # Windows and macOS. Distro Qt builds normally provide offscreen for
        # headless Unix CI runners.
        if(NOT WIN32 AND NOT APPLE)
            set_tests_properties(qtmonitor_dock_behavior PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
            )
        endif()
        qt_finalize_target(qtmonitor_dock_test)
    endif()
endif()
