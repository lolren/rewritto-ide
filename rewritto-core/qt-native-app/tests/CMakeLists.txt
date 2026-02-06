add_executable(rewritto-ide-qt-native-fake-lsp-server
  fake_lsp_server.cpp
)
target_link_libraries(rewritto-ide-qt-native-fake-lsp-server PRIVATE
  Qt6::Core
)

add_test(NAME qt-native-app-smoke COMMAND rewritto-ide --smoke-test --smoke-test-ms 50 .)
set_tests_properties(qt-native-app-smoke PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen;USER=ctest-smoke;XDG_CONFIG_HOME=${CMAKE_BINARY_DIR}/ctest-xdg-config;XDG_DATA_HOME=${CMAKE_BINARY_DIR}/ctest-xdg-data;XDG_CACHE_HOME=${CMAKE_BINARY_DIR}/ctest-xdg-cache"
)

add_executable(rewritto-ide-qt-native-test-editor
  test_editor_widget.cpp
  ../src/code_editor.cpp
  ../src/cpp_highlighter.cpp
  ../src/editor_widget.cpp
)
target_include_directories(rewritto-ide-qt-native-test-editor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-editor PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-editor COMMAND rewritto-ide-qt-native-test-editor)
set_tests_properties(qt-native-editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-code-editor
  test_code_editor.cpp
  ../src/code_editor.cpp
)
target_include_directories(rewritto-ide-qt-native-test-code-editor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-code-editor PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-code-editor COMMAND rewritto-ide-qt-native-test-code-editor)
set_tests_properties(qt-native-code-editor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-arduino-cli
  test_arduino_cli.cpp
  ../src/arduino_cli.cpp
)
target_include_directories(rewritto-ide-qt-native-test-arduino-cli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-arduino-cli PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-arduino-cli COMMAND rewritto-ide-qt-native-test-arduino-cli)

add_executable(rewritto-ide-qt-native-test-arduino-cli-diagnostics
  test_arduino_cli_diagnostics.cpp
  ../src/arduino_cli.cpp
)
target_include_directories(rewritto-ide-qt-native-test-arduino-cli-diagnostics PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-arduino-cli-diagnostics PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-arduino-cli-diagnostics COMMAND rewritto-ide-qt-native-test-arduino-cli-diagnostics)

add_executable(rewritto-ide-qt-native-test-build-output-parser
  test_build_output_parser.cpp
  ../src/build_output_parser.cpp
)
target_include_directories(rewritto-ide-qt-native-test-build-output-parser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-build-output-parser PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-build-output-parser COMMAND rewritto-ide-qt-native-test-build-output-parser)

add_executable(rewritto-ide-qt-native-test-serial
  test_serial_port.cpp
  ../src/serial_port.cpp
)
target_include_directories(rewritto-ide-qt-native-test-serial PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-serial PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-serial COMMAND rewritto-ide-qt-native-test-serial)

add_executable(rewritto-ide-qt-native-test-find-in-files
  test_find_in_files_worker.cpp
  ../src/find_in_files_dialog.cpp
)
target_include_directories(rewritto-ide-qt-native-test-find-in-files PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-find-in-files PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-find-in-files COMMAND rewritto-ide-qt-native-test-find-in-files)

add_executable(rewritto-ide-qt-native-test-examples-scanner
  test_examples_scanner.cpp
  ../src/examples_scanner.cpp
)
target_include_directories(rewritto-ide-qt-native-test-examples-scanner PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-examples-scanner PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-examples-scanner COMMAND rewritto-ide-qt-native-test-examples-scanner)

add_executable(rewritto-ide-qt-native-test-sketch-build-settings-store
  test_sketch_build_settings_store.cpp
  ../src/sketch_build_settings_store.cpp
)
target_include_directories(rewritto-ide-qt-native-test-sketch-build-settings-store PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-sketch-build-settings-store PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-sketch-build-settings-store COMMAND rewritto-ide-qt-native-test-sketch-build-settings-store)

add_executable(rewritto-ide-qt-native-test-code-snapshot-store
  test_code_snapshot_store.cpp
  ../src/code_snapshot_store.cpp
)
target_include_directories(rewritto-ide-qt-native-test-code-snapshot-store PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-code-snapshot-store PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-code-snapshot-store COMMAND rewritto-ide-qt-native-test-code-snapshot-store)

add_executable(rewritto-ide-qt-native-test-sketch-manager
  test_sketch_manager.cpp
  ../src/sketch_manager.cpp
)
target_include_directories(rewritto-ide-qt-native-test-sketch-manager PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-sketch-manager PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-sketch-manager COMMAND rewritto-ide-qt-native-test-sketch-manager)

add_executable(rewritto-ide-qt-native-test-replace-in-files
  test_replace_in_files_worker.cpp
  ../src/replace_in_files_dialog.cpp
)
target_include_directories(rewritto-ide-qt-native-test-replace-in-files PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-replace-in-files PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-replace-in-files COMMAND rewritto-ide-qt-native-test-replace-in-files)
set_tests_properties(qt-native-replace-in-files PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-lsp
  test_lsp_client.cpp
  ../src/lsp_client.cpp
)
target_include_directories(rewritto-ide-qt-native-test-lsp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-lsp PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-lsp COMMAND rewritto-ide-qt-native-test-lsp)
set_tests_properties(qt-native-lsp PROPERTIES
  ENVIRONMENT "FAKE_LSP_SERVER=$<TARGET_FILE:rewritto-ide-qt-native-fake-lsp-server>"
)

add_executable(rewritto-ide-qt-native-test-mi-parser
  test_mi_parser.cpp
  ../src/mi_parser.cpp
)
target_include_directories(rewritto-ide-qt-native-test-mi-parser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-mi-parser PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-mi-parser COMMAND rewritto-ide-qt-native-test-mi-parser)

add_executable(rewritto-ide-qt-native-test-serial-plot-parser
  test_serial_plot_parser.cpp
  ../src/serial_plot_parser.cpp
)
target_include_directories(rewritto-ide-qt-native-test-serial-plot-parser PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-serial-plot-parser PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-serial-plot-parser COMMAND rewritto-ide-qt-native-test-serial-plot-parser)

add_executable(rewritto-ide-qt-native-test-serial-plot-range
  test_serial_plot_range.cpp
  ../src/serial_plot_range.cpp
)
target_include_directories(rewritto-ide-qt-native-test-serial-plot-range PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-serial-plot-range PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-serial-plot-range COMMAND rewritto-ide-qt-native-test-serial-plot-range)

add_executable(rewritto-ide-qt-native-test-output-widget
  test_output_widget.cpp
  ../src/output_widget.cpp
)
target_include_directories(rewritto-ide-qt-native-test-output-widget PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-output-widget PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-output-widget COMMAND rewritto-ide-qt-native-test-output-widget)
set_tests_properties(qt-native-output-widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-index-update-policy
  test_index_update_policy.cpp
  ../src/index_update_policy.cpp
)
target_include_directories(rewritto-ide-qt-native-test-index-update-policy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-index-update-policy PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-index-update-policy COMMAND rewritto-ide-qt-native-test-index-update-policy)

add_executable(rewritto-ide-qt-native-test-preferences-dialog
  test_preferences_dialog.cpp
  ../src/preferences_dialog.cpp
)
target_include_directories(rewritto-ide-qt-native-test-preferences-dialog PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-preferences-dialog PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
  Qt6::Test
)
add_test(NAME qt-native-preferences-dialog COMMAND rewritto-ide-qt-native-test-preferences-dialog)
set_tests_properties(qt-native-preferences-dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-quick-pick
  test_quick_pick_dialog.cpp
  ../src/quick_pick_dialog.cpp
)
target_include_directories(rewritto-ide-qt-native-test-quick-pick PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-quick-pick PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-quick-pick COMMAND rewritto-ide-qt-native-test-quick-pick)
set_tests_properties(qt-native-quick-pick PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-library-manager-dialog
  test_library_manager_dialog.cpp
  ../src/arduino_cli.cpp
  ../src/index_update_policy.cpp
  ../src/library_manager_dialog.cpp
  ../src/output_widget.cpp
)
target_include_directories(rewritto-ide-qt-native-test-library-manager-dialog PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-library-manager-dialog PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-library-manager-dialog COMMAND rewritto-ide-qt-native-test-library-manager-dialog)
set_tests_properties(qt-native-library-manager-dialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-serial-monitor
  test_serial_monitor_widget.cpp
  ../src/serial_monitor_widget.cpp
)
target_include_directories(rewritto-ide-qt-native-test-serial-monitor PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-serial-monitor PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-serial-monitor COMMAND rewritto-ide-qt-native-test-serial-monitor)
set_tests_properties(qt-native-serial-monitor PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-serial-plotter-widget
  test_serial_plotter_widget.cpp
  ../src/serial_plotter_widget.cpp
  ../src/serial_plot_parser.cpp
  ../src/serial_plot_range.cpp
)
target_include_directories(rewritto-ide-qt-native-test-serial-plotter-widget PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-serial-plotter-widget PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-serial-plotter-widget COMMAND rewritto-ide-qt-native-test-serial-plotter-widget)
set_tests_properties(qt-native-serial-plotter-widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-welcome-widget
  test_welcome_widget.cpp
  ../src/welcome_widget.cpp
)
target_include_directories(rewritto-ide-qt-native-test-welcome-widget PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-welcome-widget PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-welcome-widget COMMAND rewritto-ide-qt-native-test-welcome-widget)
set_tests_properties(qt-native-welcome-widget PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-platform-filter-proxy
  test_platform_filter_proxy_model.cpp
  ../src/platform_filter_proxy_model.cpp
)
target_include_directories(rewritto-ide-qt-native-test-platform-filter-proxy PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-platform-filter-proxy PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Test
)
add_test(NAME qt-native-platform-filter-proxy COMMAND rewritto-ide-qt-native-test-platform-filter-proxy)
set_tests_properties(qt-native-platform-filter-proxy PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(rewritto-ide-qt-native-test-lsp-code-action-utils
  test_lsp_code_action_utils.cpp
  ../src/lsp_code_action_utils.cpp
)
target_include_directories(rewritto-ide-qt-native-test-lsp-code-action-utils PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
)
target_link_libraries(rewritto-ide-qt-native-test-lsp-code-action-utils PRIVATE
  Qt6::Core
  Qt6::Test
)
add_test(NAME qt-native-lsp-code-action-utils COMMAND rewritto-ide-qt-native-test-lsp-code-action-utils)
