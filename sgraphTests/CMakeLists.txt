function(sgraph_add_test target source)
    add_executable(${target} ${source})
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
    target_link_libraries(${target}
        PRIVATE sgraphCore sgraphKernel sgraphDocument sgraphIo Qt5::Test Qt5::Core
    )
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endfunction()

sgraph_add_test(sgraphKernelTests s_kernel_test.cpp)
sgraph_add_test(sgraphDocumentTests s_document_test.cpp)
sgraph_add_test(sgraphProjectCodecTests s_project_codec_test.cpp)
sgraph_add_test(sgraphCadCodecTests s_cad_codec_test.cpp)
sgraph_add_test(sgraphTaskManagerTests s_task_manager_test.cpp)

add_executable(sgraphGuiTests s_gui_test.cpp)
set_target_properties(sgraphGuiTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
target_link_libraries(sgraphGuiTests
    PRIVATE sgraphGui sgraphRender Qt5::Test Qt5::Widgets
)
add_test(NAME sgraphGuiTests COMMAND sgraphGuiTests)
set_tests_properties(sgraphGuiTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows;SMARTGRAPHICS3D_DISABLE_RECOVERY=1"
)

add_executable(sgraphViewportTests s_viewport_test.cpp)
set_target_properties(sgraphViewportTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)
target_link_libraries(sgraphViewportTests
    PRIVATE sgraphRender sgraphDocument sgraphKernel Qt5::Test Qt5::Widgets
)
add_test(NAME sgraphViewportTests COMMAND sgraphViewportTests)
set_tests_properties(sgraphViewportTests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=windows"
)
