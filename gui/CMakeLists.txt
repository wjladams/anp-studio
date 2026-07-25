set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(cppanp_gui
  main.cpp
  mainwindow.cpp
  document.cpp
  canvas/network_canvas.cpp
  canvas/cluster_item.cpp
  canvas/node_item.cpp
  canvas/link_item.cpp
  panels/structure_panel.cpp
  panels/pairwise_panel.cpp
  panels/results_panel.cpp
  commands/network_commands.cpp
)

target_include_directories(cppanp_gui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cppanp_gui
  PRIVATE
    cppanp::core
    Qt6::Widgets
)

if(MSVC)
  target_compile_options(cppanp_gui PRIVATE /W4)
else()
  target_compile_options(cppanp_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()
