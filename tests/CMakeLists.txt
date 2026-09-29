add_executable(crt_tests
    main.cpp
    test_core.cpp
    test_optics.cpp
    test_rt.cpp
    test_term.cpp
)
target_link_libraries(crt_tests PRIVATE crt_engine crt_options)
add_test(NAME unit_tests COMMAND crt_tests)
