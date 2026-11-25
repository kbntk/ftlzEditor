cmake_minimum_required(VERSION 3.21)
project(ftlz-edytor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Platform-independent core (model, .ftlz format, label matching, generation, i18n).
add_library(ftlcore STATIC
    src/document.cpp
    src/document.h
    src/labels.cpp
    src/labels.h
    src/i18n.cpp
    src/i18n.h
    src/commands.h
    src/indent.cpp
    src/indent.h
)
target_include_directories(ftlcore PUBLIC src)
target_link_libraries(ftlcore PUBLIC Qt6::Core)

add_executable(ftlz-edytor
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/blockwidget.cpp
    src/blockwidget.h
    src/filepanewidget.cpp
    src/filepanewidget.h
    src/ftlhighlighter.cpp
    src/ftlhighlighter.h
)
target_link_libraries(ftlz-edytor PRIVATE ftlcore Qt6::Core Qt6::Gui Qt6::Widgets)
if(WIN32)
    set_target_properties(ftlz-edytor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

option(FTLZ_BUILD_TESTS "Build unit tests for the core library" ON)
if(FTLZ_BUILD_TESTS)
    find_package(Qt6 QUIET COMPONENTS Test)
    if(Qt6Test_FOUND)
        enable_testing()
        add_executable(ftlcore_tests tests/tests.cpp)
        target_link_libraries(ftlcore_tests PRIVATE ftlcore Qt6::Test)
        add_test(NAME ftlcore_tests COMMAND ftlcore_tests)
    endif()
endif()
