cmake_minimum_required(VERSION 3.20)

project(ECGArrhythmiaProject_ContextAware)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ECGArrhythmiaProject_ContextAware
        main.cpp
        ECGReader.cpp
        PeakDetector.cpp
        ArrhythmiaDetector.cpp
)
