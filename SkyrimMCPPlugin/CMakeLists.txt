cmake_minimum_required(VERSION 3.21)

project(
    SkyrimMCPPlugin
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)

# Disable CommonLibSSE tests and VR
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)

# CommonLibSSE-NG from local clone
add_subdirectory(extern/CommonLibSSE-NG)

# Plugin sources
set(SOURCES
    src/Main.cpp
    src/PipeServer.cpp
    src/Protocol.cpp
    src/GameInterface.cpp
    src/TaskQueue.cpp
)

set(HEADERS
    src/PipeServer.h
    src/Protocol.h
    src/GameInterface.h
    src/TaskQueue.h
)

# Create the SKSE plugin as a shared library (DLL)
add_library(${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    CommonLibSSE::CommonLibSSE
    nlohmann_json::nlohmann_json
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    SKSE_SUPPORT_XBYAK=0
)

# Set output name without "lib" prefix
set_target_properties(${PROJECT_NAME} PROPERTIES
    PREFIX ""
    OUTPUT_NAME "SkyrimMCPPlugin"
)

# Copy output to a convenient location
add_custom_command(
    TARGET ${PROJECT_NAME}
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:${PROJECT_NAME}>"
        "${CMAKE_SOURCE_DIR}/output/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
    COMMENT "Copying plugin DLL to output/"
)
