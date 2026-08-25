cmake_minimum_required(VERSION 3.24)

project(best_exchange
    VERSION 0.1.0
    DESCRIPTION "Desktop application for finding the best RUB exchange offer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Network Widgets)

qt_standard_project_setup()

qt_add_executable(best_exchange
    src/main.cpp
    src/main_widget.cpp
    src/main_widget.h
)

target_link_libraries(best_exchange
    PRIVATE
        Qt6::Network
        Qt6::Widgets
)

# Conan currently builds Qt statically on macOS. Static platform plugins must
# be imported explicitly or the application cannot initialize the Cocoa UI.
if(APPLE AND TARGET Qt6::QCocoaIntegrationPlugin)
    find_library(IOSURFACE_FRAMEWORK IOSurface REQUIRED)
    target_link_libraries(best_exchange
        PRIVATE
            Qt6::QCocoaIntegrationPlugin
            ${IOSURFACE_FRAMEWORK}
    )
endif()

set_target_properties(best_exchange PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
