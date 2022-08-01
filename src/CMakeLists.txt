set(PHOTO_VIEWER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(display)
add_subdirectory(filesystem)
add_subdirectory(platform)
add_subdirectory(storage)
add_subdirectory(tools)
add_subdirectory(transceiver)

add_baremetal_executable(PhotoViewer 
        "viewer.hpp"
        "viewer.cpp")

target_compile_definitions(PhotoViewer PRIVATE STM32F412xG)

target_compile_features(PhotoViewer PRIVATE cxx_std_17)

target_compile_options(PhotoViewer PRIVATE
        ${BASIC_COMPILE_OPTIONS}
        ${ISO_COMPILE_OPTIONS}
        ${CXX_COMPILE_OPTIONS}
        ${BUILD_TYPE_SPECIFIC_COMPILE_OPTIONS})

target_link_options(PhotoViewer PRIVATE
        -T ${LINKER_SCRIPT}
        ${BASIC_LINK_OPTIONS}
        -Wl,-Map=${PROJECT_NAME}.map,--cref
        -Wl,--gc-sections
        -Wl,--print-memory-usage)
        
target_include_directories(PhotoViewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(PhotoViewer PRIVATE
        display
        filesystem
        fs::fat
        platform
        stm32::f412zg
        storage
        tools
        transceiver)


