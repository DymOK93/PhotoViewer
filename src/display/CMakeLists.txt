add_library(display)

target_sources(display
        PUBLIC
            "display.hpp"
            "fsmc.hpp"
            "lcd.hpp"
        PRIVATE
            "display.cpp"
            "fsmc.cpp"
            "lcd.cpp")

target_compile_definitions(display PUBLIC STM32F412xG)

target_compile_features(display PUBLIC cxx_std_17)

target_compile_options(display PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${CXX_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(display PUBLIC ${BASIC_LINK_OPTIONS})

target_include_directories(display 
        PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE 
            ${PHOTO_VIEWER_SOURCE_DIR})

target_link_libraries(display PUBLIC
        filesystem
        stm32::f412zg)


