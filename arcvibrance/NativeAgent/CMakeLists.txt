cmake_minimum_required(VERSION 3.20)

project(ArcVibranceNative LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ArcVibranceAgent WIN32
    agent_main.cpp
    AgentTray.cpp
    AppState.cpp
    AgentClient.cpp
    Logger.cpp
    Settings.cpp
    ArcVibranceRuntime.cpp
    ColorBackendFactory.cpp
    IntelColorBackend.cpp
    IGCLManager.cpp
    GameMonitor.cpp
    ForegroundWatcher.cpp
    HueSaturation.cpp
    cApiWrapper.cpp
    ColorAlgorithms_App.cpp
    ArcVibrance.rc
)

set_target_properties(ArcVibranceAgent PROPERTIES OUTPUT_NAME "ArcVibrance.Agent")
target_compile_definitions(ArcVibranceAgent PRIVATE UNICODE _UNICODE)
target_link_libraries(ArcVibranceAgent PRIVATE
    Comctl32
    Shell32
    Advapi32
    Dwmapi
    UxTheme
    Version
)

include(CTest)
if(BUILD_TESTING)
    add_executable(ArcVibranceProfileMatchingTests
        tests/ProfileMatchingTests.cpp
        GameMonitor.cpp
    )
    target_compile_definitions(ArcVibranceProfileMatchingTests PRIVATE UNICODE _UNICODE)
    target_link_libraries(ArcVibranceProfileMatchingTests PRIVATE Shell32)
    add_test(NAME ProfileMatching COMMAND ArcVibranceProfileMatchingTests)
endif()
