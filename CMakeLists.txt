cmake_minimum_required(VERSION 4.1)
project(String-Theory)

set(CMAKE_CXX_STANDARD 20)

add_executable(String-Theory main.cpp
        include/miniaudio.h
        include/id3.h
        include/tools.h
        include/music.h
        src/id3.cpp
        src/music.cpp
        src/tools.cpp
        resources.rc
        src/input.cpp
        include/input.h
)

target_include_directories(String-Theory PRIVATE include)