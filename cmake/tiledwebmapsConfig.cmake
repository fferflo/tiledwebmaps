get_filename_component(tiledwebmaps_CMAKE_DIR "${CMAKE_CURRENT_LIST_FILE}" PATH)

if(NOT TARGET tiledwebmaps::tiledwebmaps)
  find_package(xtl REQUIRED)
  find_package(xtensor REQUIRED)
  find_package(xtensor-blas REQUIRED)
  find_package(xtensor-interfaces REQUIRED)
  find_package(OpenCV REQUIRED)
  find_package(PROJ REQUIRED)
  find_package(CURL REQUIRED)
  find_package(curlcpp REQUIRED)

  include("${tiledwebmaps_CMAKE_DIR}/tiledwebmapsTargets.cmake")
endif()
