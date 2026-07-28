add_library(sgraphCommands STATIC
    s_i_command.h
    s_callback_command.h
    s_callback_command.cpp
    s_command_registry.h
    s_command_registry.cpp
)

target_include_directories(sgraphCommands PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(sgraphCommands PUBLIC sgraphCore sgraphDocument Qt5::Core)
