cmake_minimum_required(VERSION 3.10)
#set(CMAKE_MAKE_PROGRAM gmake) 
project(rogue)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
#set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} /usr/local)
link_directories(/usr/local/lib)
# Set C/C++ standards and compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0 -Wall -Wextra -Wpedantic -Wno-nullability-extension -Wstrict-prototypes -Wold-style-definition -fdiagnostics-color=always")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0 -Wno-nullability-completeness")

# Find required packages
find_package(Vulkan REQUIRED)
find_package(Threads REQUIRED)
#find_package(X11 REQUIRED)
find_package(glfw3 REQUIRED)
#find_package(LuaJIT REQUIRED)

# Set include directories
include_directories(
    /usr/local/include
    /usr/X11R6/include
    #/usr/local/include/luajit-2.1
    ${Vulkan_INCLUDE_DIRS}
)

# Create custom commands for shader compilation
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv
    COMMAND glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    COMMENT "Compiling vertex shader"
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv
    COMMAND glslangValidator -V ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
    COMMENT "Compiling fragment shader"
)

# Create shader target
add_custom_target(shaders 
    DEPENDS 
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/vert.spv
        ${CMAKE_CURRENT_BINARY_DIR}/shaders/frag.spv
)

# Create directory for compiled shaders
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders)

# Add VMA as a library
add_library(vma OBJECT extern/vma.cpp)
target_include_directories(vma PRIVATE /usr/local/include)

# Collect all source files
file(GLOB SOURCES "src/*.c")

# Create executable
add_executable(rogue ${SOURCES} $<TARGET_OBJECTS:vma>)

# Add shader dependencies
add_dependencies(rogue shaders)

# Link libraries
target_link_libraries(rogue
    PRIVATE
        glfw
        Vulkan::Vulkan
        ${CMAKE_THREAD_LIBS_INIT}
        m
        #luajit-5.1
)

# Enable rdynamic
set_target_properties(rogue PROPERTIES
    ENABLE_EXPORTS ON
    POSITION_INDEPENDENT_CODE ON
)
