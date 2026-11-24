cmake_minimum_required(VERSION 3.28)
project(quadtree LANGUAGES C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_SYSTEM_NAME STREQUAL "Linux"
    AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "9.4.0")
    include(${CMAKE_SOURCE_DIR}/cmake/ImportSFML.cmake)
else ()
    include(FetchContent)
    FetchContent_Declare(
        SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.1.0
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM
    )
    set(SFML_USE_SYSTEM_DEPS OFF)
    FetchContent_MakeAvailable(SFML)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(freetype PRIVATE harfbuzz)
    endif ()
endif ()

set(GLAD_DIR ${CMAKE_CURRENT_SOURCE_DIR}/extern/glad)
set(GLAD_INCLUDE_DIR ${GLAD_DIR}/include)
set(GLAD_SRC ${GLAD_DIR}/src/glad.c)

add_executable(quadtree_accel src/accel_main.cpp ${GLAD_SRC})
target_include_directories(quadtree_accel PRIVATE ${GLAD_INCLUDE_DIR})
target_include_directories(quadtree_accel PRIVATE ${GLAD_INCLUDE_DIR})
target_link_libraries(quadtree_accel PRIVATE
    SFML::Graphics
    SFML::Window
)

add_executable(quadtree_no_accel src/no_accel_main.cpp ${GLAD_SRC})
target_include_directories(quadtree_no_accel PRIVATE ${GLAD_INCLUDE_DIR})
target_include_directories(quadtree_no_accel PRIVATE ${GLAD_INCLUDE_DIR})
target_link_libraries(quadtree_no_accel PRIVATE
    SFML::Graphics
    SFML::Window
)
