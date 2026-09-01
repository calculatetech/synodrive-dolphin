cmake_minimum_required(VERSION 3.20)
project(synodrive-status VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

add_executable(synodrive-status src/synodrive-status.cpp)
target_compile_features(synodrive-status PRIVATE cxx_std_17)
target_link_libraries(synodrive-status PRIVATE dl)

install(TARGETS synodrive-status RUNTIME DESTINATION bin)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
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
                $<TARGET_FILE:fake-synodrive-plugin>
                $<TARGET_FILE:fake-synodrive-missing-symbol>
    )
endif()
