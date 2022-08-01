add_library(storage INTERFACE)

target_compile_features(storage INTERFACE cxx_std_17)

target_include_directories(storage INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

