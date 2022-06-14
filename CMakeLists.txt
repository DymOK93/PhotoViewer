cmake_minimum_required(VERSION 3.15)
project(PhotoViewer)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/elf")
set(LINKER_SCRIPT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/link")

set(BASIC_COMPILE_OPTIONS
        -mcpu=cortex-m4
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
        -fdata-sections
        -ffunction-sections
        -fno-exceptions
        -fno-rtti
        -fno-threadsafe-statics
        -Wall)

set(BASIC_LINK_OPTIONS
        -mcpu=cortex-m4
        -mthumb
        -mfpu=fpv4-sp-d16
        -mfloat-abi=hard
        -specs=nano.specs
        -lc
        -lm
        -lnosys)

set(LINKER_SCRIPT "${LINKER_SCRIPT_DIRECTORY}/STM32F412ZG_FLASH.lds")

add_subdirectory(fatfs)
add_subdirectory(stm32)

add_baremetal_executable(PhotoViewer 
                "attributes.hpp"
                "bmp.hpp"
                "bmp.cpp"
                "break_on.hpp"
                "circular.hpp"
                "command.hpp"
                "command.cpp"
                "diskio.cpp"
                "display.hpp"
                "display.cpp"
                "event.hpp"
                "event.cpp"
                "file.hpp"
                "file.cpp"
                "fsmc.hpp"
                "fsmc.cpp"
                "gpio.hpp"
                "lcd.hpp"
                "lcd.cpp"
                "main.hpp"
                "main.cpp"
                "receiver.hpp"
                "receiver.cpp"
                "request_parser.hpp"
                "sdio.hpp"
                "sdio.cpp"
                "singleton.hpp"
                "transmitter.hpp"
                "transmitter.cpp")

target_compile_definitions(PhotoViewer PUBLIC STM32F412xG)

target_compile_features(PhotoViewer PUBLIC cxx_std_17)

target_compile_options(PhotoViewer PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        $<$<CONFIG:Debug>:-O0 -g3>
        $<$<CONFIG:Release>:-O3 -flto>)

target_link_options(PhotoViewer PRIVATE
        -T ${LINKER_SCRIPT}
        ${BASIC_LINK_OPTIONS}
        -Wl,-Map=${PROJECT_NAME}.map,--cref
        -Wl,--gc-sections
        -Wl,--print-memory-usage)
        
target_include_directories(PhotoViewer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(PhotoViewer PUBLIC 
        fatfs
        stm32::f412zg)


