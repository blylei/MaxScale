# Run npm install
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/node_modules
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/package.json
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND npm install)
add_custom_target(install_node_modules ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/node_modules)

#
# Tests
#
add_test_script(mongodbclient_insert mongodbclient_insert.js mongodbclient LABELS mongodbclient REPL_BACKEND)
