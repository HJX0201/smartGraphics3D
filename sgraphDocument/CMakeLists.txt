add_library(sgraphDocument STATIC
    s_document_transaction.h
    s_document_transaction.cpp
    s_scene_object.h
    s_3d_document.h
    s_3d_document.cpp
    s_3d_document_coordinates.cpp
    s_3d_document_batch.cpp
    s_3d_document_copy.cpp
    s_3d_document_appearance.cpp
    s_3d_document_state.cpp
)

target_include_directories(sgraphDocument PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sgraphDocument PUBLIC sgraphCore sgraphKernel Qt5::Core Qt5::Gui)
