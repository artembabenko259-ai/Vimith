add_executable(vimith_tests
    test_piece_table.cpp
    test_mode_manager.cpp
    test_highlight_engine.cpp
)

target_link_libraries(vimith_tests PRIVATE
    vimith_core
    vimith_input
    vimith_syntax
    Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(vimith_tests)
