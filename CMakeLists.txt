cmake_minimum_required(VERSION 3.21)
project(differ LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable Qt auto processing
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6
find_package(Qt6 6.2 COMPONENTS Widgets Gui Core Sql Concurrent REQUIRED)

set(PROJECT_SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/ImageHash.cpp
    src/ImageHash.h
    src/SqliteStore.cpp
    src/SqliteStore.h
    src/ImageIndexer.cpp
    src/ImageIndexer.h
    src/ThumbnailModel.cpp
    src/ThumbnailModel.h
)

qt_add_executable(differ
    ${PROJECT_SOURCES}
)

## No qrc resources included

# Enable high DPI scaling
if (WIN32)
    target_compile_definitions(differ PRIVATE
        QT_USE_QSTRINGBUILDER
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX)
endif()

# Include directories
target_include_directories(differ PRIVATE src)

# Link Qt
target_link_libraries(differ PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Concurrent
)

# Set default output dir
set_target_properties(differ PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install step (optional)
install(TARGETS differ RUNTIME DESTINATION bin)
