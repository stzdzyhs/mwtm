cmake_minimum_required(VERSION 3.10)

# set the project name and version
project(mwtm VERSION 1.0)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# configure a header file to pass some of the CMake settings
# to the source code
configure_file(config.h.in config.h)

# add the executable
add_executable(mwtm src/main.cpp)

# link with libs
target_link_libraries(mwtm X11 Xrandr )

# add the binary tree to the search path for include files
# so that we will find TutorialConfig.h
target_include_directories(mwtm PUBLIC "${PROJECT_BINARY_DIR}")
