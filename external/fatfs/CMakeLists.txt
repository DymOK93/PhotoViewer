add_library(fatfs STATIC 
	"include/diskio.h"
        "include/ff.h"
        "src/ff.c"
        "include/ffconf.h"
        "src/ffsystem.c"
        "src/ffunicode.c")

target_compile_features(fatfs PUBLIC c_std_99)

target_compile_options(fatfs PUBLIC ${BASIC_COMPILE_OPTIONS})

target_link_options(fatfs PUBLIC ${BASIC_LINK_OPTIONS})

target_include_directories(fatfs PUBLIC 
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
