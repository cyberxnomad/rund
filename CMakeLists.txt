cmake_minimum_required(VERSION 3.15.0)

# Optional: print out extra messages to see what is going on. Comment it to have less verbose messages
# set(CMAKE_VERBOSE_MAKEFILE ON)

# Set release with O2 optimization
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

# Setup project
project(rund)

set(EXECUTABLE ${PROJECT_NAME})

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(COMPILE_OPTS
    -ffunction-sections
    -fdata-sections

    -Wall
    -Wfloat-equal
    -Wshadow
    -Wunused
    -Wdeprecated
)

set(LINK_OPTS
    -Wl,--gc-sections
)

file(GLOB SRC CONFIGURE_DEPENDS
    src/*.c
)

# Build the executable based on the source files
add_executable(${EXECUTABLE} ${SRC})

# Compiler options
target_compile_options(${EXECUTABLE} PRIVATE
    ${COMPILE_OPTS}
)

# Link options
target_link_options(${EXECUTABLE} PRIVATE
    ${LINK_OPTS}
)

install(TARGETS ${EXECUTABLE}
    RUNTIME DESTINATION bin
)
