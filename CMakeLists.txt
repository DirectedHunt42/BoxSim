cmake_minimum_required(VERSION 3.16)
project(BoxSim)

set(CMAKE_CXX_STANDARD 20)

# Find SFML
find_package(SFML 2.6 COMPONENTS graphics window system REQUIRED)

# Create executable
add_executable(boxsim boxsim.cpp)

# Link SFML
target_link_libraries(boxsim PRIVATE sfml-graphics sfml-window sfml-system)
