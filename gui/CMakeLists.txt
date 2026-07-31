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
  ratings/rating_preset.cpp
  ratings/rating_preset_store.cpp
  ratings/rating_preset_dialogs.cpp
  resources.qrc
)

target_include_directories(anpstudio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(anpstudio
  PRIVATE
    anpcpp::anpcpp
    Qt6::Widgets
)

set_target_properties(anpstudio PROPERTIES
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_BUNDLE_NAME "ANP Studio"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.bamath.anpstudio"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  WIN32_EXECUTABLE TRUE
)

if(MSVC)
  target_compile_options(anpstudio PRIVATE /W4)
else()
  target_compile_options(anpstudio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Ship sample models next to the binary (and inside the macOS app bundle).
set(ANPSTUDIO_SAMPLES_SRC "${CMAKE_SOURCE_DIR}/samples")
add_custom_command(TARGET anpstudio POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${ANPSTUDIO_SAMPLES_SRC}"
    "$<TARGET_FILE_DIR:anpstudio>/samples"
  COMMENT "Copying samples next to anpstudio"
)
if(APPLE)
  add_custom_command(TARGET anpstudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_BUNDLE_CONTENT_DIR:anpstudio>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
      "${ANPSTUDIO_SAMPLES_SRC}"
      "$<TARGET_BUNDLE_CONTENT_DIR:anpstudio>/Resources/samples"
    COMMENT "Copying samples into macOS app Resources"
  )
endif()
