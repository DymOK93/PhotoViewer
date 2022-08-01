add_library(platform STATIC)

target_sources(platform
        PUBLIC
            "event.hpp"
            "gpio.hpp"
        PRIVATE
            "event.cpp")

target_compile_definitions(platform PUBLIC STM32F412xG)

target_compile_features(platform PUBLIC cxx_std_17)

target_compile_options(platform PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${CXX_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(platform PUBLIC ${BASIC_LINK_OPTIONS})

target_include_directories(platform 
        PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE 
            ${PHOTO_VIEWER_SOURCE_DIR})

target_link_libraries(platform PUBLIC
        stm32::f412zg
        tools)


