cmake_minimum_required(VERSION 3.0)
project(wav_transceiver)

SET(sources test.cpp
            wav_transceiver.cpp
            )
SET(headers wav_transceiver.h)

add_executable(test ${sources} ${headers})

set_target_properties(test PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)