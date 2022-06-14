add_library(cmsis INTERFACE)	

target_compile_features(cmsis INTERFACE c_std_99)

target_include_directories(cmsis INTERFACE 
		${CMAKE_CURRENT_SOURCE_DIR})

target_sources(cmsis INTERFACE 
        "core_cm4.h"
        "core_cmFunc.h"
        "core_cmInstr.h"
        "core_cmSimd.h"
        "stm32f4xx.h"
        "system_stm32f4xx.h")