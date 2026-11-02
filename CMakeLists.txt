cmake_minimum_required(VERSION 3.16)
project(linjefolger_esp32 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(line_controller STATIC
    src/control/linefollow.cpp src/control/helper.cpp src/control/ramps.cpp)
target_include_directories(line_controller PUBLIC include include/control)

enable_testing()
foreach(name runtime control_runtime master board_pins hardware hardware_mux)
    if(name STREQUAL "hardware_mux")
        set(test_source tests/test_hardware.cpp)
    else()
        set(test_source tests/test_${name}.cpp)
    endif()
    add_executable(${name}_tests ${test_source})
    target_include_directories(${name}_tests BEFORE PRIVATE tests/fakes)
    target_link_libraries(${name}_tests PRIVATE line_controller)
    if(name MATCHES "^hardware")
        target_compile_definitions(${name}_tests PRIVATE LINEFOLLOW_BUILD_FIXTURE)
    endif()
    if(name STREQUAL "hardware_mux")
        target_compile_definitions(${name}_tests PRIVATE LINEFOLLOW_MUX_FIXTURE)
    endif()
    add_test(NAME ${name} COMMAND ${name}_tests)
endforeach()

find_program(PLATFORMIO_EXECUTABLE NAMES platformio pio
    HINTS "$ENV{USERPROFILE}/.platformio/penv/Scripts")
if(PLATFORMIO_EXECUTABLE)
    add_custom_target(firmware_esp32
        COMMAND "${PLATFORMIO_EXECUTABLE}" run -d "${CMAKE_SOURCE_DIR}" -e esp32 VERBATIM)
endif()
