cmake_minimum_required(VERSION 2.8)

project (rnnoise)

# Get source files
file(GLOB SOURCES "src/*.c" "src/*.h" "include/*.h")



# Compile the library
add_library(rnnoise STATIC ${SOURCES})

target_compile_options(rnnoise PRIVATE -fPIC)

# Include dirs
target_include_directories(rnnoise PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        PRIVATE src)
