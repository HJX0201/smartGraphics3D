add_library(sgraphCore STATIC
    s_coordinate_system.h
    s_coordinate_system.cpp
    s_result.h
    s_types.h
    s_unit_system.h
    s_unit_system.cpp
    s_task_manager.h
    s_task_manager.cpp
    s_transform_utils.h
    s_transform_utils.cpp
)

target_include_directories(sgraphCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(sgraphCore PUBLIC "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(sgraphCore PUBLIC Qt5::Core Qt5::Gui Qt5::Concurrent)
