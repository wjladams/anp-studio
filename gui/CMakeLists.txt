set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(anpstudio
  main.cpp
  mainwindow.cpp
  document.cpp
  canvas/network_canvas.cpp
  canvas/cluster_item.cpp
  canvas/node_item.cpp
  canvas/link_item.cpp
  panels/pairwise_panel.cpp
  panels/results_panel.cpp
  panels/inspector_panel.cpp
  panels/judgment_nav_panel.cpp
  panels/judgment_priorities_panel.cpp
  panels/ratings_panel.cpp
  panels/session_stub_panel.cpp
  panels/synthesis_summary_panel.cpp
  panels/analysis_panel.cpp
  panels/sensitivity_chart_widget.cpp
  commands/network_commands.cpp
  resources.qrc
)

target_include_directories(anpstudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(anpstudio
  PRIVATE
    anpcpp::anpcpp
    Qt6::Widgets
)

if(MSVC)
  target_compile_options(anpstudio PRIVATE /W4)
else()
  target_compile_options(anpstudio PRIVATE -Wall -Wextra -Wpedantic)
endif()
