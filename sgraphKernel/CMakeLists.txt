add_library(sgraphKernel STATIC
    s_kernel_shape.h
    s_kernel_shape.cpp
    s_kernel_shape_access.h
    s_kernel_service.h
    s_kernel_service.cpp
    s_kernel_measurement_utils.h
    s_kernel_measurement_utils.cpp
)

target_include_directories(sgraphKernel
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(sgraphKernel
    PUBLIC sgraphCore
    PRIVATE
        TKernel
        TKMath
        TKG2d
        TKG3d
        TKGeomBase
        TKBRep
        TKGeomAlgo
        TKTopAlgo
        TKPrim
        TKBO
        TKBool
        TKFillet
        TKOffset
        TKShHealing
)
