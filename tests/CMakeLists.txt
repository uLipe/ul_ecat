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
        test_al_fsm.cpp
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

    # Controller + LAN9252 HAL mock (memória): não liga libul_ecat_lan9252; valida poll/PDO/callbacks.
    add_executable(ul_ecat_slave_controller_tests
        test_slave_controller_mock.cpp
        lan9252_full_mock.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_esc.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_slave_al.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_slave_pdu.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_slave.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_slave_controller.c
        ${CMAKE_SOURCE_DIR}/src/slave/ul_ecat_slave_controller_lan9252.c
        ${CMAKE_SOURCE_DIR}/generated/ul_ecat_slave_tables.c
    )
    target_compile_definitions(ul_ecat_slave_controller_tests PRIVATE UL_ECAT_HAVE_LAN9252_CONTROLLER=1)
    target_link_libraries(ul_ecat_slave_controller_tests PRIVATE
        ul_ecat_wire
        GTest::gtest_main
    )
    target_include_directories(ul_ecat_slave_controller_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/generated
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/slave
        ${CMAKE_SOURCE_DIR}/controllers/lan9252/include
    )
    add_test(NAME ul_ecat_slave_controller_gtest COMMAND ul_ecat_slave_controller_tests)
    set_tests_properties(ul_ecat_slave_controller_gtest PROPERTIES TIMEOUT 120)
endif()
