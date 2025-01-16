cmake_minimum_required(VERSION 3.16)

project(ul_ecat
    VERSION 0.2.0
    DESCRIPTION "Minimal EtherCAT master library (Linux raw sockets)"
    LANGUAGES C CXX
)

option(UL_ECAT_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
option(UL_ECAT_BUILD_TOOLS "Build legacy C ecatTool executable" OFF)
option(UL_ECAT_BUILD_SHARED "Build shared libul_ecat.so (Python FFI / ctypes)" ON)
option(UL_ECAT_ENABLE_COVERAGE "Compile with coverage flags (gcov)" OFF)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

if(UL_ECAT_ENABLE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
endif()

add_library(ul_ecat
    src/ul_ecat_al.c
    src/ul_ecat_frame.c
    src/ul_ecat_master.c
)
add_library(ul_ecat::ul_ecat ALIAS ul_ecat)

target_include_directories(ul_ecat
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ul_ecat PUBLIC pthread)

set_target_properties(ul_ecat PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

if(UL_ECAT_BUILD_SHARED)
    add_library(ul_ecat_shared SHARED
        src/ul_ecat_al.c
        src/ul_ecat_frame.c
        src/ul_ecat_master.c
    )
    add_library(ul_ecat::shared ALIAS ul_ecat_shared)
    target_include_directories(ul_ecat_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(ul_ecat_shared PUBLIC pthread)
    set_target_properties(ul_ecat_shared PROPERTIES
        OUTPUT_NAME ul_ecat
        POSITION_INDEPENDENT_CODE ON
    )
endif()

if(UL_ECAT_BUILD_TOOLS)
    add_executable(ecatTool tools/ul_ecat_tool.c)
    target_link_libraries(ecatTool PRIVATE ul_ecat)
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
        # Exclude integration test from default ctest (needs root + veth; can hang if env is wrong).
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

if(UL_ECAT_BUILD_SHARED)
    install(TARGETS ul_ecat ul_ecat_shared
        EXPORT ul_ecatTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
else()
    install(TARGETS ul_ecat
        EXPORT ul_ecatTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

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
