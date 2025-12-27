cmake_minimum_required(VERSION 3.21)

# Integrate vcpkg if available
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
    message(STATUS "Using vcpkg toolchain from VCPKG_ROOT: $ENV{VCPKG_ROOT}")
elseif(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    # Try common vcpkg install locations
    set(VCPKG_COMMON_PATHS
        "C:/vcpkg"
        "D:/vcpkg"
        "${CMAKE_SOURCE_DIR}/vcpkg"
        "$ENV{USERPROFILE}/vcpkg"
    )
    foreach(VCPKG_PATH ${VCPKG_COMMON_PATHS})
        if(EXISTS "${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake")
            set(CMAKE_TOOLCHAIN_FILE "${VCPKG_PATH}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
            message(STATUS "Found vcpkg at: ${VCPKG_PATH}")
            break()
        endif()
    endforeach()
endif()

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
    src/ThumbnailDelegate.cpp
    src/ThumbnailDelegate.h
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

# Require OpenCV for similarity search (ORB + Histogram)
find_package(OpenCV REQUIRED)
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV libraries: ${OpenCV_LIBS}")
target_compile_definitions(differ PRIVATE HAVE_OPENCV=1)
target_include_directories(differ PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(differ PRIVATE ${OpenCV_LIBS})

# MSVC-specific compiler flags for Qt compatibility
if(MSVC)
    target_compile_options(differ PRIVATE /Zc:__cplusplus /utf-8)
endif()

# Set default output dir
set_target_properties(differ PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Deploy Qt runtime dependencies (DLLs, plugins, etc.)
if(WIN32)
    # Get Qt6 installation directory from the actual Qt6 Core component
    get_target_property(_qt6_core_location Qt6::Core LOCATION)
    get_filename_component(_qt_bin_dir "${_qt6_core_location}" DIRECTORY)
    
    # Use windeployqt to copy all Qt dependencies
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
    
    if(WINDEPLOYQT_EXECUTABLE)
        # Determine output directory based on generator type
        if(CMAKE_CONFIGURATION_TYPES)
            set(_output_dir "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
        else()
            set(_output_dir "${CMAKE_BINARY_DIR}/bin")
        endif()
        
        add_custom_command(TARGET differ POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E echo "Deploying Qt dependencies to ${_output_dir}..."
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --dir "${_output_dir}"
                --no-compiler-runtime
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:differ>"
            COMMENT "Running windeployqt"
        )
        message(STATUS "Will use windeployqt: ${WINDEPLOYQT_EXECUTABLE}")
    else()
        message(WARNING "windeployqt not found. You may need to manually copy Qt DLLs.")
    endif()
endif()

# Install step (optional)
install(TARGETS differ RUNTIME DESTINATION bin)
