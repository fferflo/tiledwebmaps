add_subdirectory(backend)

execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/tiledwebmaps/__init__.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps/__init__.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/tiledwebmaps/config.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps/config.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/tiledwebmaps/geo.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps/geo.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/tiledwebmaps/presets.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps/presets.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/tiledwebmaps/util.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps/util.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/test ${CMAKE_CURRENT_BINARY_DIR}/test)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/setup.py ${CMAKE_CURRENT_BINARY_DIR}/setup.py)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/setup.cfg ${CMAKE_CURRENT_BINARY_DIR}/setup.cfg)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../readme.md ${CMAKE_CURRENT_BINARY_DIR}/readme.md)
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_CURRENT_SOURCE_DIR}/../LICENSE ${CMAKE_CURRENT_BINARY_DIR}/LICENSE)

find_package(Python REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/copy_proj_data.py ${CMAKE_CURRENT_BINARY_DIR}/tiledwebmaps COMMAND_ERROR_IS_FATAL ANY)

add_test(NAME pytest WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/python" COMMAND pytest)
