add_executable(smartGraphics3D WIN32
    s_main.cpp
)

target_link_libraries(smartGraphics3D PRIVATE sgraphGui)

set_target_properties(smartGraphics3D PROPERTIES OUTPUT_NAME "smartGraphics3D")
