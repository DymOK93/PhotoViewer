add_library(stm32_f412zg STATIC)

target_sources(stm32_f412zg
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE
            "src/init.c"
            "src/startup_stm32f412zx.c"
            "src/system_stm32f4xx.c")

add_library(stm32::f412zg ALIAS stm32_f412zg)
        
target_compile_definitions(stm32_f412zg PUBLIC 
        STM32F412xG 
        $<$<CONFIG:Debug>:DEBUG>)

target_compile_features(stm32_f412zg PUBLIC c_std_99)

target_compile_options(stm32_f412zg PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(stm32_f412zg PRIVATE
        ${BASIC_LINK_OPTIONS})

target_include_directories(stm32_f412zg PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include")

