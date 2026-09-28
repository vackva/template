# Install rules: headers, tpl_dsp, and a relocatable CMake package, so a consumer can
#   find_package(tpl CONFIG REQUIRED)
#   target_link_libraries(app PRIVATE tpl::dsp)
# The release packages built on a tag (on_tag.yml) are `cmake --install` of this.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(${PROJECT_SOURCE_DIR}/cmake/tanh/install-helpers.cmake)

set_target_properties(tpl_dsp PROPERTIES EXPORT_NAME dsp)
tanh_set_install_rpath(tpl_dsp)

install(TARGETS tpl_dsp
    EXPORT tplTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/tpl DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

set(_config_dir ${CMAKE_INSTALL_LIBDIR}/cmake/tpl)
install(EXPORT tplTargets NAMESPACE tpl:: DESTINATION ${_config_dir})
configure_package_config_file(${PROJECT_SOURCE_DIR}/cmake/tplConfig.cmake.in
    ${PROJECT_BINARY_DIR}/tplConfig.cmake
    INSTALL_DESTINATION ${_config_dir})
write_basic_package_version_file(${PROJECT_BINARY_DIR}/tplConfigVersion.cmake
    COMPATIBILITY SameMajorVersion)
install(FILES ${PROJECT_BINARY_DIR}/tplConfig.cmake ${PROJECT_BINARY_DIR}/tplConfigVersion.cmake
    DESTINATION ${_config_dir})
