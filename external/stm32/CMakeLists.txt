set(STM32_RUNTIME_SOURCES
        "include/core_cm4.h"
        "include/core_cmFunc.h"
        "include/core_cmInstr.h"
        "include/core_cmSimd.h"
        "include/stm32f4xx.h"
        "include/system_stm32f4xx.h"
        "src/init.c"
        "src/startup_stm32f412zx.c"
        "src/system_stm32f4xx.c")

add_library(stm32_f412zg STATIC ${STM32_RUNTIME_SOURCES})
        
target_sources(stm32_f412zg PUBLIC ${STM32_RUNTIME_SOURCES})

add_library(stm32::f412zg ALIAS stm32_f412zg)

target_compile_definitions(stm32_f412zg PUBLIC 
        STM32F412xG 
        $<$<CONFIG:Debug>:DEBUG>)

target_compile_features(stm32_f412zg PUBLIC c_std_99)

target_compile_options(stm32_f412zg PUBLIC
        ${BASIC_COMPILE_OPTIONS})

target_link_options(stm32_f412zg PUBLIC
        ${BASIC_LINK_OPTIONS})

target_include_directories(stm32_f412zg PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include")

