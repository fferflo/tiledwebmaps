find_package(Catch2 3 REQUIRED)

macro(twm_add_test TARGET TEST_NAME)
  add_executable(${TARGET} EXCLUDE_FROM_ALL ${TEST_NAME})
  target_link_libraries(${TARGET} tiledwebmaps Catch2::Catch2WithMain)
  add_test(NAME ${TARGET} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test" COMMAND "${CMAKE_BINARY_DIR}/bin/test/${TARGET}")
  set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/test")
  add_dependencies(tests ${TARGET})
endmacro()

twm_add_test(test_tiledwebmaps tiledwebmaps.cpp)
target_link_libraries(test_tiledwebmaps tiledwebmaps)
