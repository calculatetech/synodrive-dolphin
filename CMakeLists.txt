cmake_minimum_required(VERSION 3.20)
project(synodrive-dolphin VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core)

find_path(KF6_KIO_INCLUDE_DIR KOverlayIconPlugin PATH_SUFFIXES KF6/KIOCore)
find_library(KF6_KIOCORE_LIBRARY NAMES KF6KIOCore)
if(NOT KF6_KIO_INCLUDE_DIR OR NOT KF6_KIOCORE_LIBRARY)
    message(FATAL_ERROR "KF6 KIO development header and runtime library are required")
endif()
get_filename_component(KF6_INCLUDE_ROOT "${KF6_KIO_INCLUDE_DIR}" DIRECTORY)

add_executable(synodrive-status src/synodrive-status.cpp)
target_compile_features(synodrive-status PRIVATE cxx_std_17)
target_link_libraries(synodrive-status PRIVATE dl)

install(TARGETS synodrive-status RUNTIME DESTINATION bin)

add_library(synodrive-overlay MODULE
    src/status_provider.cpp
    src/synodrive_overlay_plugin.cpp
)
target_compile_features(synodrive-overlay PRIVATE cxx_std_17)
target_include_directories(synodrive-overlay PRIVATE
    ${KF6_KIO_INCLUDE_DIR}
    ${KF6_INCLUDE_ROOT}/KIO
)
target_link_libraries(synodrive-overlay PRIVATE Qt6::Core ${KF6_KIOCORE_LIBRARY})
set_target_properties(synodrive-overlay PROPERTIES PREFIX "")
install(TARGETS synodrive-overlay
    LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kf6/overlayicon
)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(synodrive-status-test src/synodrive-status.cpp)
    target_compile_features(synodrive-status-test PRIVATE cxx_std_17)
    target_compile_definitions(synodrive-status-test PRIVATE SYNODRIVE_STATUS_TESTING)
    target_link_libraries(synodrive-status-test PRIVATE dl)
    add_library(fake-synodrive-plugin MODULE tests/fake_plugin.cpp)
    target_compile_features(fake-synodrive-plugin PRIVATE cxx_std_17)
    set_target_properties(fake-synodrive-plugin PROPERTIES
        PREFIX ""
        OUTPUT_NAME "plugin-cb-4"
    )
    add_library(fake-synodrive-missing-symbol MODULE tests/fake_missing_symbol.cpp)
    set_target_properties(fake-synodrive-missing-symbol PROPERTIES
        PREFIX ""
        OUTPUT_NAME "plugin-cb-4-missing-symbol"
    )
    add_test(
        NAME cli
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_cli.py
                $<TARGET_FILE:synodrive-status-test>
                $<TARGET_FILE:synodrive-status>
                $<TARGET_FILE:fake-synodrive-plugin>
                $<TARGET_FILE:fake-synodrive-missing-symbol>
    )

    add_executable(fake-status-helper tests/fake_status_helper.cpp)
    target_compile_features(fake-status-helper PRIVATE cxx_std_17)

    add_executable(test-plugin
        tests/test_plugin.cpp
        src/status_provider.cpp
        src/synodrive_overlay_plugin.cpp
    )
    target_compile_features(test-plugin PRIVATE cxx_std_17)
    target_compile_definitions(test-plugin PRIVATE SYNODRIVE_PROVIDER_TESTING)
    target_include_directories(test-plugin PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${KF6_KIO_INCLUDE_DIR}
        ${KF6_INCLUDE_ROOT}/KIO
    )
    target_link_libraries(test-plugin PRIVATE Qt6::Core Qt6::Test ${KF6_KIOCORE_LIBRARY})
    add_test(
        NAME plugin
        COMMAND ${CMAKE_COMMAND} -E env
                SYNODRIVE_TEST_HELPER=$<TARGET_FILE:fake-status-helper>
                SYNODRIVE_TEST_PLUGIN=$<TARGET_FILE:synodrive-overlay>
                SYNODRIVE_TEST_CLI=$<TARGET_FILE:synodrive-status-test>
                SYNODRIVE_TEST_SYNOLOGY_PLUGIN=$<TARGET_FILE:fake-synodrive-plugin>
                $<TARGET_FILE:test-plugin>
    )
endif()
