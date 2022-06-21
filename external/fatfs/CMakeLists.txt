add_library(fatfs STATIC)

target_sources(fatfs
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PRIVATE
            "src/ff.c"
            "src/ffsystem.c"
            "src/ffunicode.c")

add_library(fs::fat ALIAS fatfs)
        
target_compile_features(fatfs PUBLIC c_std_99)

target_compile_options(fatfs PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(fatfs PRIVATE
        ${BASIC_LINK_OPTIONS})

target_include_directories(fatfs PUBLIC
		"${CMAKE_CURRENT_SOURCE_DIR}/include")


