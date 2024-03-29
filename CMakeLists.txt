cmake_minimum_required(VERSION 3.15)
project(PhotoViewer)
enable_language(C ASM)

set(CC_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_TOOLCHAIN_FILE "${CC_DIR}/tools/cmake/ArmNoneEabiGcc.cmake")
set(LINKER_SCRIPT "${CC_DIR}/tools/link/STM32F412ZG_FLASH.lds")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CC_DIR}/build")

set(ARCHITECTURE_SPECIFIC_OPTIONS
        -mcpu=cortex-m4
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard)

set(BASIC_COMPILE_OPTIONS
        ${ARCHITECTURE_SPECIFIC_OPTIONS}
        -fdata-sections -ffunction-sections
        -Wall -Wextra
        -Wduplicated-branches -Wduplicated-cond 
        -Wlogical-op
        -Wcast-qual)

set(ISO_COMPILE_OPTIONS 
        -Werror 
        -Wpedantic)

set(CXX_COMPILE_OPTIONS
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics)

set(BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O0 -g3>)

set(BASIC_LINK_OPTIONS
        ${ARCHITECTURE_SPECIFIC_OPTIONS}
        -specs=nano.specs
        -lc -lm -lnosys)

add_subdirectory(external)
add_subdirectory(src)



