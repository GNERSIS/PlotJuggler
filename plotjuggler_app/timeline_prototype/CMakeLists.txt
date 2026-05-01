# Timeline widget prototype — standalone Qt5 executable, no plotjuggler_base linkage.

set(TIMELINE_PROTO_SRC
  main.cpp
  model/fake_sequence_provider.cpp
)

add_executable(timeline_prototype ${TIMELINE_PROTO_SRC})

target_link_libraries(timeline_prototype
  PRIVATE
    Qt5::Core
    Qt5::Widgets
)

target_include_directories(timeline_prototype
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(timeline_prototype PROPERTIES
  AUTOMOC ON
  AUTOUIC ON
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
