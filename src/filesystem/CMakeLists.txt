add_library(filesystem)

target_sources(filesystem
        PUBLIC
            "bmp.hpp"
            "file.hpp"
            "sdio.hpp"
        PRIVATE
            "bmp.cpp"
            "diskio.cpp"
            "file.cpp"
            "sdio.cpp")

target_compile_definitions(filesystem PUBLIC STM32F412xG)

target_compile_features(filesystem PUBLIC cxx_std_17)

target_compile_options(filesystem PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${CXX_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(filesystem PUBLIC ${BASIC_LINK_OPTIONS})

target_include_directories(filesystem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(filesystem PUBLIC
        fs::fat
        stm32::f412zg)


