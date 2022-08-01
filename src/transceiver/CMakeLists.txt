add_library(transceiver STATIC)

target_sources(transceiver
        PUBLIC
            "command.hpp"
            "io.hpp"
            "receiver.hpp"
            "request_parser.hpp"
            "transmitter.hpp"
        PRIVATE
            "command.cpp"
            "receiver.cpp"
            "transmitter.cpp")

target_compile_definitions(transceiver PUBLIC STM32F412xG)

target_compile_features(transceiver PUBLIC cxx_std_17)

target_compile_options(transceiver PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${CXX_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(transceiver PUBLIC ${BASIC_LINK_OPTIONS})

target_include_directories(transceiver 
        PUBLIC 
            ${CMAKE_CURRENT_SOURCE_DIR}
        PRIVATE 
            ${PHOTO_VIEWER_SOURCE_DIR})

target_link_libraries(transceiver PUBLIC
        display
        filesystem
        platform
        stm32::f412zg
        storage
        tools)


