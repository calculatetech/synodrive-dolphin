cmake_minimum_required(VERSION 3.20)
project(synodrive-dolphin
    VERSION 1.0.0
    DESCRIPTION "Synology Drive Dolphin Extension (Unofficial)"
    HOMEPAGE_URL "https://github.com/calculatetech/synodrive-dolphin"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)
set(CMAKE_AUTOMOC ON)
option(SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL
    "Apply the Synology Drive tray patch during source installation"
    OFF
)
if(SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL)
    set(SYNODRIVE_TRAY_PATCH_INSTALL_HOME "$ENV{HOME}")
    if(NOT SYNODRIVE_TRAY_PATCH_INSTALL_HOME
       OR NOT IS_ABSOLUTE "${SYNODRIVE_TRAY_PATCH_INSTALL_HOME}")
        message(FATAL_ERROR
            "SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL requires a nonempty absolute HOME"
        )
    endif()
    set(SYNODRIVE_TRAY_PATCH_HOME_DELIMITER "=")
    while(SYNODRIVE_TRAY_PATCH_INSTALL_HOME MATCHES "]${SYNODRIVE_TRAY_PATCH_HOME_DELIMITER}]")
        string(APPEND SYNODRIVE_TRAY_PATCH_HOME_DELIMITER "=")
    endwhile()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6KIO REQUIRED)

find_path(KF6_KIO_INCLUDE_DIR KOverlayIconPlugin PATH_SUFFIXES KF6/KIOCore)
find_library(KF6_KIOCORE_LIBRARY NAMES KF6KIOCore)
if(NOT KF6_KIO_INCLUDE_DIR OR NOT KF6_KIOCORE_LIBRARY)
    message(FATAL_ERROR "KF6 KIO development header and runtime library are required")
endif()
get_filename_component(KF6_INCLUDE_ROOT "${KF6_KIO_INCLUDE_DIR}" DIRECTORY)

add_executable(synodrive-status
    src/synodrive-status.cpp
    src/synodrive_compatibility.cpp
)
target_compile_features(synodrive-status PRIVATE cxx_std_17)
target_link_libraries(synodrive-status PRIVATE dl)

install(TARGETS synodrive-status RUNTIME DESTINATION bin)

add_executable(synodrive-tray-patch
    src/synodrive-tray-patch.cpp
    src/synodrive_compatibility.cpp
)
target_compile_features(synodrive-tray-patch PRIVATE cxx_std_17)
install(TARGETS synodrive-tray-patch RUNTIME DESTINATION bin)

add_executable(synodrive-action
    src/synodrive-action.cpp
    src/synodrive_compatibility.cpp
)
target_compile_features(synodrive-action PRIVATE cxx_std_17)
target_link_libraries(synodrive-action PRIVATE dl)
set_target_properties(synodrive-action PROPERTIES
    ENABLE_EXPORTS ON
)
install(TARGETS synodrive-action
    RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${PROJECT_NAME}
)

add_library(synodrive-fileitemaction MODULE
    src/synodrive_fileitemaction_plugin.cpp
)
target_compile_features(synodrive-fileitemaction PRIVATE cxx_std_17)
target_compile_definitions(synodrive-fileitemaction PRIVATE
    SYNODRIVE_ACTION_PROGRAM="${CMAKE_INSTALL_FULL_LIBEXECDIR}/${PROJECT_NAME}/synodrive-action"
)
target_link_libraries(synodrive-fileitemaction PRIVATE
    Qt6::Core
    Qt6::Widgets
    KF6::CoreAddons
    KF6::KIOWidgets
)
set_target_properties(synodrive-fileitemaction PROPERTIES PREFIX "")
install(TARGETS synodrive-fileitemaction
    LIBRARY DESTINATION ${QT6_INSTALL_PLUGINS}/kf6/kfileitemaction
)

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
install(FILES LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/${PROJECT_NAME}
    RENAME copyright
)

if(SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL)
    install(CODE "
        set(synodrive_tray_patch_home
            [${SYNODRIVE_TRAY_PATCH_HOME_DELIMITER}[${SYNODRIVE_TRAY_PATCH_INSTALL_HOME}]${SYNODRIVE_TRAY_PATCH_HOME_DELIMITER}]
        )
        if(NOT \"\$ENV{DESTDIR}\" STREQUAL \"\")
            message(FATAL_ERROR
                \"SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL cannot be used with DESTDIR\"
            )
        endif()
        execute_process(
            COMMAND \"${CMAKE_COMMAND}\" -E env
                    \"HOME=\${synodrive_tray_patch_home}\"
                    \"$<TARGET_FILE:synodrive-tray-patch>\" apply
            RESULT_VARIABLE synodrive_tray_patch_result
        )
        if(NOT synodrive_tray_patch_result EQUAL 0)
            message(FATAL_ERROR \"Synology Drive tray patch failed\")
        endif()
    ")
endif()

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_executable(synodrive-status-test
        src/synodrive-status.cpp
        src/synodrive_compatibility.cpp
    )
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

    add_executable(synodrive-tray-patch-test
        src/synodrive-tray-patch.cpp
        src/synodrive_compatibility.cpp
    )
    target_compile_features(synodrive-tray-patch-test PRIVATE cxx_std_17)
    target_compile_definitions(synodrive-tray-patch-test PRIVATE SYNODRIVE_TRAY_PATCH_TESTING)
    add_test(
        NAME tray-patch
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_tray_patch.py
                $<TARGET_FILE:synodrive-tray-patch>
                $<TARGET_FILE:synodrive-tray-patch-test>
                ${CMAKE_SOURCE_DIR}
                ${CMAKE_BINARY_DIR}
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
    add_test(
        NAME package-command
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_package_command.py
                ${CMAKE_SOURCE_DIR}/ci/package
    )

    find_library(NAUTILUS_EXTENSION_LIBRARY NAMES nautilus-extension libnautilus-extension.so.4 REQUIRED)
    find_library(GOBJECT_LIBRARY NAMES gobject-2.0 libgobject-2.0.so.0 REQUIRED)
    find_library(GLIB_LIBRARY NAMES glib-2.0 libglib-2.0.so.0 REQUIRED)
    add_library(fake-context-menu MODULE tests/fake_context_menu.cpp)
    target_compile_features(fake-context-menu PRIVATE cxx_std_17)
    target_link_libraries(fake-context-menu PRIVATE
        ${NAUTILUS_EXTENSION_LIBRARY}
        ${GOBJECT_LIBRARY}
        ${GLIB_LIBRARY}
    )
    set_target_properties(fake-context-menu PROPERTIES
        PREFIX ""
        OUTPUT_NAME "plugin-cb-4"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/context-fixture"
    )

    add_executable(synodrive-action-test
        src/synodrive-action.cpp
        src/synodrive_compatibility.cpp
    )
    target_compile_features(synodrive-action-test PRIVATE cxx_std_17)
    target_compile_definitions(synodrive-action-test PRIVATE SYNODRIVE_ACTION_TESTING)
    target_link_libraries(synodrive-action-test PRIVATE dl)
    set_target_properties(synodrive-action-test PROPERTIES ENABLE_EXPORTS ON)
    set(CONTEXT_TEST_PLUGIN_ROOT "${CMAKE_BINARY_DIR}/context-test-plugins")
    add_library(synodrive-fileitemaction-test MODULE
        src/synodrive_fileitemaction_plugin.cpp
    )
    target_compile_features(synodrive-fileitemaction-test PRIVATE cxx_std_17)
    target_compile_definitions(synodrive-fileitemaction-test PRIVATE
        SYNODRIVE_ACTION_PROGRAM="$<TARGET_FILE:synodrive-action-test>"
    )
    target_link_libraries(synodrive-fileitemaction-test PRIVATE
        Qt6::Core Qt6::Widgets KF6::CoreAddons KF6::KIOWidgets
    )
    set_target_properties(synodrive-fileitemaction-test PROPERTIES
        PREFIX ""
        OUTPUT_NAME "synodrive-fileitemaction"
        LIBRARY_OUTPUT_DIRECTORY "${CONTEXT_TEST_PLUGIN_ROOT}/kf6/kfileitemaction"
    )

    add_executable(test-context-plugin
        tests/test_context_plugin.cpp
        src/synodrive_fileitemaction_plugin.cpp
    )
    target_compile_features(test-context-plugin PRIVATE cxx_std_17)
    target_compile_definitions(test-context-plugin PRIVATE
        SYNODRIVE_ACTION_PROGRAM="$<TARGET_FILE:synodrive-action-test>"
    )
    target_include_directories(test-context-plugin PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(test-context-plugin PRIVATE
        Qt6::Core Qt6::Widgets Qt6::Test KF6::CoreAddons KF6::KIOWidgets
    )
    add_dependencies(test-context-plugin synodrive-fileitemaction-test)
    add_test(
        NAME context-actions
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/test_context_action.py
                $<TARGET_FILE:synodrive-action-test>
                $<TARGET_FILE:synodrive-action>
                $<TARGET_FILE:fake-context-menu>
                $<TARGET_FILE:synodrive-status-test>
                $<TARGET_FILE:fake-synodrive-plugin>
                $<TARGET_FILE:test-context-plugin>
                ${CONTEXT_TEST_PLUGIN_ROOT}
                $<TARGET_FILE:fake-synodrive-missing-symbol>
    )
endif()

set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "calculatetech")
set(CPACK_PACKAGE_CONTACT "Michael Beutler <mikebeutler84@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_DESCRIPTION
    "Adds Synology Drive file-status overlays and file actions to KDE Dolphin. Synology Drive is installed separately."
)
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_DEPENDS "dolphin, libnautilus-extension4")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE "1")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_REQUIRES "dolphin, libnautilus-extension.so.4()(64bit)")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")
set(CPACK_RPM_SPEC_MORE_DEFINE "Packager: ${CPACK_PACKAGE_CONTACT}")

include(CPack)
