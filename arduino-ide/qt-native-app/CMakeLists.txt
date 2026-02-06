cmake_minimum_required(VERSION 3.21)

project(rewritto-ide LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Test)

qt_standard_project_setup()

qt_add_executable(rewritto-ide MANUAL_FINALIZATION
  resources/resources.qrc
  src/main.cpp
  src/main_window.cpp
  src/main_window.h
  src/arduino_cli.cpp
  src/arduino_cli.h
  src/boards_manager_dialog.cpp
  src/boards_manager_dialog.h
  src/board_selector_dialog.cpp
  src/board_selector_dialog.h
	  src/build_output_parser.cpp
	  src/build_output_parser.h
	  src/code_editor.cpp
	  src/code_editor.h
	  src/code_snapshot_compare_dialog.cpp
	  src/code_snapshot_compare_dialog.h
	  src/code_snapshot_store.cpp
	  src/code_snapshot_store.h
	  src/code_snapshots_dialog.cpp
	  src/code_snapshots_dialog.h
	  src/cpp_highlighter.cpp
	  src/cpp_highlighter.h
	  src/editor_widget.cpp
	  src/editor_widget.h
  src/examples_dialog.cpp
  src/examples_dialog.h
  src/examples_scanner.cpp
  src/examples_scanner.h
  src/find_in_files_dialog.cpp
  src/find_in_files_dialog.h
  src/find_replace_dialog.cpp
  src/find_replace_dialog.h
  src/index_update_policy.cpp
  src/index_update_policy.h
  src/library_manager_dialog.cpp
  src/library_manager_dialog.h
  src/lsp_client.cpp
  src/lsp_client.h
  src/lsp_code_action_utils.cpp
  src/lsp_code_action_utils.h
  src/mi_parser.cpp
  src/mi_parser.h
  src/output_widget.cpp
  src/output_widget.h
  src/problems_widget.cpp
  src/problems_widget.h
  src/quick_pick_dialog.cpp
  src/quick_pick_dialog.h
  src/sketch_manager.cpp
  src/sketch_manager.h
  src/sketch_build_settings_store.cpp
  src/sketch_build_settings_store.h
  src/preferences_dialog.cpp
  src/preferences_dialog.h
  src/replace_in_files_dialog.cpp
  src/replace_in_files_dialog.h
  src/serial_plot_parser.cpp
  src/serial_plot_parser.h
  src/serial_plot_range.cpp
  src/serial_plot_range.h
  src/serial_plotter_widget.cpp
  src/serial_plotter_widget.h
  src/serial_monitor_widget.cpp
  src/serial_monitor_widget.h
  src/serial_port.cpp
  src/serial_port.h
  src/platform_filter_proxy_model.cpp
  src/platform_filter_proxy_model.h
  src/theme_manager.cpp
  src/theme_manager.h
  src/toast_widget.cpp
  src/toast_widget.h
  src/interface_scale_manager.cpp
  src/interface_scale_manager.h
  src/welcome_widget.cpp
  src/welcome_widget.h
)

target_link_libraries(rewritto-ide PRIVATE
  Qt6::Core
  Qt6::Widgets
  Qt6::Network
)

qt_finalize_executable(rewritto-ide)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(tests)
endif()
