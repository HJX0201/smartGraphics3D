add_library(sgraphRender STATIC
    s_render_quality.h
    s_render_quality.cpp
    s_occ_viewport.h
    s_occ_viewport_p.h
    s_occ_viewport.cpp
    s_occ_viewport_events.cpp
    s_occ_viewport_scene.cpp
)

target_include_directories(sgraphRender
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${OpenCASCADE_INCLUDE_DIR}
)

target_link_libraries(sgraphRender
    PUBLIC sgraphCore sgraphKernel sgraphDocument Qt5::Core Qt5::Gui Qt5::Widgets
    PRIVATE TKernel TKService TKV3d TKOpenGl TKBRep TKTopAlgo
)
