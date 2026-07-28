add_library(sgraphIo STATIC
    s_i_file_codec.h
    s_standard_cad_codec.h
    s_standard_cad_codec.cpp
    s_project_codec.h
    s_project_codec.cpp
    s_project_validation.h
    s_project_validation.cpp
    s_project_archive.cpp
)

target_include_directories(sgraphIo
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(sgraphIo
    PUBLIC sgraphCore sgraphKernel sgraphDocument Qt5::Core
    PRIVATE
        TKernel
        TKBRep
        TKMesh
        TKSTL
        TKRWMesh
        TKXSBase
        TKSTEP
        TKSTEPBase
        TKIGES
        TKXCAF
        TKXDESTEP
        TKXDEIGES
        TKCAF
        TKLCAF
        TKCDF
)
