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

# Widgets lib — view + controller + the embeddable widget. Shared between the
# standalone exe and the main plotjuggler app, which embeds the timeline at
# the bottom of its layout in place of the legacy playback bar.
add_library(timeline_prototype_widgets STATIC
  controller/playback_controller.cpp
  view/playback_toolbar.cpp
  view/playhead_item.cpp
  view/time_ruler_item.cpp
  view/timeline_scene_widget.cpp
  view/topic_item.cpp
  view/work_range_handle_item.cpp
  embedded_timeline_widget.cpp
)

target_link_libraries(timeline_prototype_widgets
  PUBLIC
    timeline_prototype_model
    Qt5::Widgets
)

set_target_properties(timeline_prototype_widgets PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
)

# Standalone executable.
add_executable(timeline_prototype
  main.cpp
  timeline_window.cpp
)

target_link_libraries(timeline_prototype
  PRIVATE
    timeline_prototype_widgets
)

set_target_properties(timeline_prototype PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
