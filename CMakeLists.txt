cmake_minimum_required(VERSION 3.17 FATAL_ERROR)
project(tiledwebmaps LANGUAGES CXX)

enable_testing()

include(GNUInstallDirs)

set(PACKAGE_NAME tiledwebmaps)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/${PACKAGE_NAME}/cmake)
set(INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR}/${PACKAGE_NAME})


# Base library
add_library(tiledwebmaps INTERFACE)
target_compile_features(tiledwebmaps INTERFACE cxx_std_17)

find_package(xtl REQUIRED)
find_package(xtensor REQUIRED)
find_package(xtensor-io REQUIRED)
find_package(xtensor-blas REQUIRED)
find_package(xtensor-interfaces REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(PROJ REQUIRED)
find_package(CURL REQUIRED)
find_package(curlcpp REQUIRED)

target_link_libraries(tiledwebmaps INTERFACE
  xtensor
  xtensor-io
  xtensor-blas
  xtensor-interfaces::base
  xtensor-interfaces::opencv
  PROJ::proj
  CURL::libcurl
  curlcpp::curlcpp
  ${OpenCV_LIBS}
)
target_include_directories(tiledwebmaps INTERFACE
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${OpenCV_INCLUDE_DIRS}
)

install(
  TARGETS tiledwebmaps
  EXPORT install_targets
)



######################## TESTS ########################

add_custom_target(tests)
add_subdirectory(test)



######################## PYTHON ########################

option(tiledwebmaps_BUILD_PYTHON_INTERFACE "Build python interface" ON)
if(tiledwebmaps_BUILD_PYTHON_INTERFACE)
  add_subdirectory(python)
endif()



######################## INSTALL ########################

# Install headers
set(INSTALL_HEADERS_AS_SYMLINK OFF CACHE BOOL "Installs headers as a symlink to the source directory instead of copying all files to install directory")
if(INSTALL_HEADERS_AS_SYMLINK)
  install(CODE "file(MAKE_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR})")
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/include/tiledwebmaps ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/tiledwebmaps)")
  install(CODE "message(\"-- Installed symlink: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/tiledwebmaps -> ${CMAKE_CURRENT_SOURCE_DIR}/include/tiledwebmaps\")")
else()
  install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()

# Install targets file
install(EXPORT install_targets
  FILE
    ${PACKAGE_NAME}Targets.cmake
  NAMESPACE
    ${PACKAGE_NAME}::
  DESTINATION
    ${INSTALL_CONFIGDIR}
)
# Install ${PACKAGE_NAME}Config.cmake
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PACKAGE_NAME}Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
  INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/${PACKAGE_NAME}Config.cmake
  DESTINATION ${INSTALL_CONFIGDIR}
)
