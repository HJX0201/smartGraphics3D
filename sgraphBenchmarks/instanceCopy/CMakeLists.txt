if(NOT WIN32)
    return()
endif()

add_executable(sgraphInstanceCopyBenchmark
    src/s_instance_copy_benchmark.cpp
)
set_target_properties(sgraphInstanceCopyBenchmark PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/benchmarks"
)
target_link_libraries(sgraphInstanceCopyBenchmark
    PRIVATE sgraphRender sgraphDocument sgraphKernel sgraphIo Qt5::Widgets Psapi
)
