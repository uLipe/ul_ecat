if(UL_ECAT_BUILD_MASTER)
    add_executable(ul_ecat_tests
        test_al.cpp
        test_frame.cpp
        test_frame_negative.cpp
        test_master_smoke.cpp
    )

    target_link_libraries(ul_ecat_tests PRIVATE
        ul_ecat
        GTest::gtest_main
    )

    target_include_directories(ul_ecat_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    add_test(NAME ul_ecat_gtest COMMAND ul_ecat_tests)
    set_tests_properties(ul_ecat_gtest PROPERTIES TIMEOUT 120)
endif()

if(UL_ECAT_BUILD_SLAVE)
    add_executable(ul_ecat_slave_tests
        test_slave.cpp
    )

    target_link_libraries(ul_ecat_slave_tests PRIVATE
        ul_ecat_slave
        GTest::gtest_main
    )

    target_include_directories(ul_ecat_slave_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
    )

    add_test(NAME ul_ecat_slave_gtest COMMAND ul_ecat_slave_tests)
    set_tests_properties(ul_ecat_slave_gtest PROPERTIES TIMEOUT 120)
endif()
