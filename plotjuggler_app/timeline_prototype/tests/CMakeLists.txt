find_package(GTest QUIET)
if(NOT GTest_FOUND)
  message(STATUS "timeline_prototype: GTest not found, skipping tests")
  return()
endif()

add_executable(test_timeline_model test_timeline_model.cpp)

target_link_libraries(test_timeline_model
  PRIVATE
    timeline_prototype_model
    Qt5::Core
    Qt5::Gui
    GTest::gtest_main
)

target_include_directories(test_timeline_model
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

set_target_properties(test_timeline_model PROPERTIES AUTOMOC ON)

include(GoogleTest)
gtest_discover_tests(test_timeline_model)
