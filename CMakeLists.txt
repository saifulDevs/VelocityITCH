cmake_minimum_required(VERSION 3.14)
project(VelocityITCH VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# HFT Performance Compiler Optimization Flags
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(
        -O3
        -march=native
        -ffast-math
        -Wall
        -Wextra
        -Wpedantic
        -pthread
    )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 1. Main Synthetic ITCH Data Generator Executable
add_executable(dse_itch_generator
    src/main.cpp
)
target_link_libraries(dse_itch_generator PRIVATE pthread)

# 2. Protocol & State Manager Unit Test Executable
add_executable(test_protocol
    tests/test_protocol.cpp
)
target_link_libraries(test_protocol PRIVATE pthread)

# 3. ITCH Receiver Utility Executable
add_executable(itch_receiver
    tests/receiver.cpp
)
target_link_libraries(itch_receiver PRIVATE pthread)

# Installation targets
install(TARGETS dse_itch_generator itch_receiver test_protocol DESTINATION bin)
