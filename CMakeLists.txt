
cmake_minimum_required(VERSION 3.16)

project(KPP
    VERSION 0.8.0
    LANGUAGES CXX
)

# ============================================================
# K++ — Kolibri C++
# ============================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------
# Directorios
# ------------------------------------------------------------

set(KPP_INCLUDE_DIR
    ${CMAKE_SOURCE_DIR}/include
)

set(KPP_SOURCE_DIR
    ${CMAKE_SOURCE_DIR}/src
)

# ------------------------------------------------------------
# Código fuente
# ------------------------------------------------------------

file(GLOB_RECURSE KPP_SOURCES
    CONFIGURE_DEPENDS

    ${KPP_SOURCE_DIR}/*.cpp
)

# ------------------------------------------------------------
# Ejecutable
# ------------------------------------------------------------

add_executable(kpp
    ${KPP_SOURCES}
)

# ------------------------------------------------------------
# Includes
# ------------------------------------------------------------

target_include_directories(kpp
    PRIVATE
        ${KPP_INCLUDE_DIR}
)

# ------------------------------------------------------------
# Warnings
# ------------------------------------------------------------

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(kpp
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
    )
endif()

# ------------------------------------------------------------
# Directorios de salida
# ------------------------------------------------------------

set_target_properties(kpp PROPERTIES

    RUNTIME_OUTPUT_DIRECTORY
        ${CMAKE_SOURCE_DIR}/bin

)

# ------------------------------------------------------------
# Información de compilación
# ------------------------------------------------------------

message(STATUS "")
message(STATUS "======================================")
message(STATUS " K++ - Kolibri C++")
message(STATUS "======================================")
message(STATUS "Version : ${PROJECT_VERSION}")
message(STATUS "Source  : ${KPP_SOURCE_DIR}")
message(STATUS "Include : ${KPP_INCLUDE_DIR}")
message(STATUS "Output  : ${CMAKE_SOURCE_DIR}/bin")
message(STATUS "")
message(STATUS "Sources:")

foreach(SOURCE ${KPP_SOURCES})
    message(STATUS "  ${SOURCE}")
endforeach()

message(STATUS "======================================")
message(STATUS "")
