include(CMakePrintHelpers)
cmake_print_variables(CPACK_TEMPORARY_DIRECTORY)
cmake_print_variables(CPACK_TOPLEVEL_DIRECTORY)
cmake_print_variables(CPACK_PACKAGE_DIRECTORY)
cmake_print_variables(CPACK_PACKAGE_FILE_NAME)
cmake_print_variables(CMAKE_SYSTEM_PROCESSOR)
cmake_print_variables(CPACK_INSTALL_CMAKE_PROJECTS)

set(TWM_VERSION "@TWM_VERSION@")
list(GET CPACK_INSTALL_CMAKE_PROJECTS 0 TWM_BUILD_DIR)

file(SHA256 "${CPACK_PACKAGE_FILES}" TWM_INSTALLER_SHA256)
configure_file("${TWM_BUILD_DIR}/resources/winget.yaml" "${TWM_BUILD_DIR}/twm.yaml")
