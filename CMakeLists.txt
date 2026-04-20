cmake_minimum_required(VERSION 3.16)

project(ul_ecat
    VERSION 0.3.0
    DESCRIPTION "Minimal EtherCAT master/slave libraries (ETG wire format)"
    LANGUAGES C CXX
)

option(UL_ECAT_BUILD_MASTER "Build EtherCAT master library (libul_ecat)" ON)
option(UL_ECAT_BUILD_SLAVE "Build EtherCAT slave library (libul_ecat_slave)" ON)
option(UL_ECAT_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
option(UL_ECAT_BUILD_TOOLS "Build legacy C ecatTool executable" OFF)
option(UL_ECAT_BUILD_SHARED "Build shared libul_ecat.so (Python FFI / ctypes)" ON)
option(UL_ECAT_ENABLE_COVERAGE "Compile with coverage flags (gcov)" OFF)
option(UL_ECAT_BUILD_LAN9252 "Build LAN9252 SPI controller helper library (controllers/lan9252)" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

if(UL_ECAT_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

# Shared wire format + AL helpers (only when master and/or slave are built)
if(UL_ECAT_BUILD_MASTER OR UL_ECAT_BUILD_SLAVE)
    add_library(ul_ecat_wire STATIC
        src/common/ul_ecat_frame.c
        src/common/ul_ecat_al.c
    )
    target_include_directories(ul_ecat_wire PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    set_target_properties(ul_ecat_wire PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

if(UL_ECAT_BUILD_LAN9252)
    add_subdirectory(controllers/lan9252)
endif()

if(NOT UL_ECAT_BUILD_MASTER AND NOT UL_ECAT_BUILD_SLAVE AND NOT UL_ECAT_BUILD_LAN9252)
    message(FATAL_ERROR "Enable at least one of UL_ECAT_BUILD_MASTER, UL_ECAT_BUILD_SLAVE, or UL_ECAT_BUILD_LAN9252")
endif()

if(UL_ECAT_BUILD_MASTER)
    add_library(ul_ecat STATIC
        src/master/ul_ecat_master.c
        src/osal/osal_linux.c
        src/transport/transport_linux.c
    )
    add_library(ul_ecat::ul_ecat ALIAS ul_ecat)

    target_include_directories(ul_ecat
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(ul_ecat PUBLIC ul_ecat_wire pthread)

    set_target_properties(ul_ecat PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    if(UL_ECAT_BUILD_SHARED)
        add_library(ul_ecat_shared SHARED
            src/master/ul_ecat_master.c
            src/osal/osal_linux.c
            src/transport/transport_linux.c
        )
        add_library(ul_ecat::shared ALIAS ul_ecat_shared)
        target_include_directories(ul_ecat_shared
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
            PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/src
        )
        target_link_libraries(ul_ecat_shared PUBLIC ul_ecat_wire pthread)
        set_target_properties(ul_ecat_shared PROPERTIES
            OUTPUT_NAME ul_ecat
            POSITION_INDEPENDENT_CODE ON
        )
    endif()

    if(UL_ECAT_BUILD_TOOLS)
        add_executable(ecatTool tools/ul_ecat_tool.c)
        target_link_libraries(ecatTool PRIVATE ul_ecat)
    endif()
endif()

if(UL_ECAT_BUILD_SLAVE)
    find_program(UL_ECAT_PYTHON3 NAMES python3 python)
    if(UL_ECAT_PYTHON3)
        add_custom_target(ul_ecat_regen_slave_tables
            COMMAND ${UL_ECAT_PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_slave_data.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Regenerate generated/ul_ecat_slave_tables.{c,h} from scripts/gen_slave_data.py"
            VERBATIM
        )
    endif()

    set(_ul_ecat_slave_src
        src/slave/ul_ecat_esc.c
        src/slave/ul_ecat_slave_al.c
        src/slave/ul_ecat_slave_mailbox.c
        src/slave/ul_ecat_slave_pdu.c
        src/slave/ul_ecat_slave.c
        src/slave/ul_ecat_slave_controller.c
        generated/ul_ecat_slave_tables.c
    )
    if(UL_ECAT_BUILD_LAN9252)
        list(APPEND _ul_ecat_slave_src src/slave/ul_ecat_slave_controller_lan9252.c)
    endif()

    add_library(ul_ecat_slave STATIC ${_ul_ecat_slave_src})
    add_library(ul_ecat::slave ALIAS ul_ecat_slave)
    target_include_directories(ul_ecat_slave
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/generated>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(ul_ecat_slave PUBLIC ul_ecat_wire)
    if(UL_ECAT_BUILD_LAN9252)
        target_link_libraries(ul_ecat_slave PUBLIC ul_ecat_lan9252)
        target_compile_definitions(ul_ecat_slave PRIVATE UL_ECAT_HAVE_LAN9252_CONTROLLER=1)
    endif()
    set_target_properties(ul_ecat_slave PROPERTIES
        POSITION_INDEPENDENT_CODE ON
    )

    add_executable(ul_ecat_slave_harness tools/ul_ecat_slave_harness.c)
    target_link_libraries(ul_ecat_slave_harness PRIVATE ul_ecat_slave)
    target_include_directories(ul_ecat_slave_harness PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

if(UL_ECAT_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    add_subdirectory(tests)
    find_program(UL_ECAT_PYTEST NAMES pytest pytest3)
    if(UL_ECAT_PYTEST)
        add_test(
            NAME ul_ecat_pytest
            COMMAND ${UL_ECAT_PYTEST} ${CMAKE_CURRENT_SOURCE_DIR}/tests -v
                --ignore=${CMAKE_CURRENT_SOURCE_DIR}/tests/test_integration_sim.py
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )
        set_tests_properties(ul_ecat_pytest PROPERTIES
            ENVIRONMENT "PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}"
            TIMEOUT 120
        )
    endif()
endif()

find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)
if(UL_ECAT_ENABLE_COVERAGE AND LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    add_custom_target(ul_ecat_coverage
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        COMMAND ${LCOV_EXECUTABLE} --capture --directory ${CMAKE_BINARY_DIR} --output-file ${CMAKE_BINARY_DIR}/coverage.info
        COMMAND ${LCOV_EXECUTABLE} --remove ${CMAKE_BINARY_DIR}/coverage.info '/usr/*' '*/_deps/*' --output-file ${CMAKE_BINARY_DIR}/coverage.filtered.info
        COMMAND ${GENHTML_EXECUTABLE} ${CMAKE_BINARY_DIR}/coverage.filtered.info --output-directory ${CMAKE_BINARY_DIR}/coverage_html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Run tests and generate lcov HTML report in build/coverage_html"
        USES_TERMINAL
    )
endif()

set(_ul_ecat_install_targets "")
if(UL_ECAT_BUILD_MASTER OR UL_ECAT_BUILD_SLAVE)
    list(APPEND _ul_ecat_install_targets ul_ecat_wire)
endif()
if(UL_ECAT_BUILD_MASTER)
    list(APPEND _ul_ecat_install_targets ul_ecat)
    if(UL_ECAT_BUILD_SHARED)
        list(APPEND _ul_ecat_install_targets ul_ecat_shared)
    endif()
endif()
if(UL_ECAT_BUILD_SLAVE)
    list(APPEND _ul_ecat_install_targets ul_ecat_slave ul_ecat_slave_harness)
endif()
if(UL_ECAT_BUILD_LAN9252)
    list(APPEND _ul_ecat_install_targets ul_ecat_lan9252)
endif()

if(_ul_ecat_install_targets)
    install(TARGETS ${_ul_ecat_install_targets}
        EXPORT ul_ecatTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(UL_ECAT_BUILD_LAN9252)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/controllers/lan9252/include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
if(UL_ECAT_BUILD_SLAVE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/generated/ul_ecat_slave_tables.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ul_ecatConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ul_ecatConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ul_ecatConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ul_ecat
)

install(EXPORT ul_ecatTargets
    FILE ul_ecatTargets.cmake
    NAMESPACE ul_ecat::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ul_ecat
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/ul_ecatConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ul_ecatConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ul_ecat
)
