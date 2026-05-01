# Model lib — separated so tests can link without pulling in main.cpp.
add_library(timeline_prototype_model STATIC
  model/fake_sequence_provider.cpp
  model/timeline_model.cpp
)

target_link_libraries(timeline_prototype_model
  PUBLIC
    Qt5::Core
    Qt5::Gui
)

target_include_directories(timeline_prototype_model
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(timeline_prototype_model PROPERTIES AUTOMOC ON)

# Executable.
add_executable(timeline_prototype
  main.cpp
  timeline_window.cpp
  controller/playback_controller.cpp
  view/playback_toolbar.cpp
  view/playhead_item.cpp
  view/sequence_table_widget.cpp
  view/time_ruler_item.cpp
  view/timeline_scene_widget.cpp
  view/topic_item.cpp
  view/work_range_handle_item.cpp
)

target_link_libraries(timeline_prototype
  PRIVATE
    timeline_prototype_model
    Qt5::Widgets
)

set_target_properties(timeline_prototype PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
