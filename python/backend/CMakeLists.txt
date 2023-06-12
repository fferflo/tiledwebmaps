set(Python3_FIND_STRATEGY LOCATION)
find_package(Python REQUIRED COMPONENTS NumPy)
find_package(pybind11 REQUIRED)
find_package(xtl REQUIRED)
find_package(xtensor REQUIRED)
find_package(xtensor-python REQUIRED)

pybind11_add_module(backend main.cpp)
target_link_libraries(backend PUBLIC tiledwebmaps xtensor-python Python::NumPy)

set_target_properties(backend PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python/tiledwebmaps")
