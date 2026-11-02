cmake_minimum_required(VERSION 3.16)
project(linjefolger2 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CLion kan bygge/kjøre logikktestene som et vanlig CMake-prosjekt.
# Arduino-firmwaren bygges separat av PlatformIO med korrekte AVR-verktøy.
add_library(line_controller STATIC
    src/control/linefollow.cpp src/control/helper.cpp src/control/ramps.cpp)
target_include_directories(line_controller PUBLIC include include/control)
add_executable(runtime_tests tests/test_runtime.cpp)
target_link_libraries(runtime_tests PRIVATE line_controller)
add_executable(nano_runtime_tests tests/test_nano.cpp)
target_include_directories(nano_runtime_tests BEFORE PRIVATE tests/fakes)
target_link_libraries(nano_runtime_tests PRIVATE line_controller)
enable_testing()
add_test(NAME protocol_scheduler_controller COMMAND runtime_tests)
add_test(NAME actual_nano_runtime COMMAND nano_runtime_tests)
foreach(name master side_link)
    add_executable(${name}_tests tests/test_${name}.cpp)
    target_include_directories(${name}_tests BEFORE PRIVATE tests/fakes)
    target_link_libraries(${name}_tests PRIVATE line_controller)
    add_test(NAME ${name}_runtime COMMAND ${name}_tests)
endforeach()
add_executable(side_nano_tests tests/test_side_nano.cpp)
target_include_directories(side_nano_tests PRIVATE tests/side_config tests/fakes include)
add_test(NAME actual_side_nano_runtime COMMAND side_nano_tests)

find_program(PLATFORMIO_EXECUTABLE NAMES platformio pio
    HINTS "$ENV{USERPROFILE}/.platformio/penv/Scripts")
if(PLATFORMIO_EXECUTABLE)
    add_custom_target(firmware_nano
        COMMAND "${PLATFORMIO_EXECUTABLE}" run -d "${CMAKE_SOURCE_DIR}" -e nano VERBATIM)
    add_custom_target(firmware_uno
        COMMAND "${PLATFORMIO_EXECUTABLE}" run -d "${CMAKE_SOURCE_DIR}" -e uno VERBATIM)
    add_custom_target(firmware_master_nano
        COMMAND "${PLATFORMIO_EXECUTABLE}" run -d "${CMAKE_SOURCE_DIR}" -e master_nano VERBATIM)
    add_custom_target(firmware_side_nano
        COMMAND "${PLATFORMIO_EXECUTABLE}" run -d "${CMAKE_SOURCE_DIR}" -e side_nano VERBATIM)
endif()
