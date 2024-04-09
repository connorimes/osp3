cmake_minimum_required(VERSION 3.12...3.28)

project(osp3 VERSION 0.0.1
             LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
add_compile_options(-Wall -Wextra -Wpedantic)

include(GNUInstallDirs)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
  @ONLY
)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

enable_testing()


# Libraries

add_library(osp3 src/osp3.c
                 src/osp3i-common.c
                 $<IF:$<PLATFORM_ID:Darwin>,src/osp3i-serial-darwin.c,src/osp3i-serial-posix.c>)
target_include_directories(osp3 PRIVATE ${PROJECT_SOURCE_DIR}/src
                                PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/inc>
                                       $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/osp3>)
set_target_properties(osp3 PROPERTIES PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/inc/osp3.h"
                                      VERSION ${PROJECT_VERSION}
                                      SOVERSION ${PROJECT_VERSION_MAJOR})
install(TARGETS osp3
        EXPORT OSP3Targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OSP3_Runtime
                NAMELINK_COMPONENT OSP3_Development
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                COMPONENT OSP3_Development
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/osp3
                      COMPONENT OSP3_Development)


# CMake package helper

include(CMakePackageConfigHelpers)
set(OSP3_CMAKE_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/OSP3)
set(CONFIG_TARGETS_FILE OSP3Targets.cmake)
set(CONFIG_REQUIRED_COMPONENTS osp3)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/OSP3Config.cmake
  INSTALL_DESTINATION ${OSP3_CMAKE_CONFIG_INSTALL_DIR}
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/OSP3ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMinorVersion
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/OSP3Config.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/OSP3ConfigVersion.cmake
        DESTINATION ${OSP3_CMAKE_CONFIG_INSTALL_DIR}
        COMPONENT OSP3_Development)
install(EXPORT OSP3Targets
        DESTINATION ${OSP3_CMAKE_CONFIG_INSTALL_DIR}
        NAMESPACE OSP3::
        COMPONENT OSP3_Development)


# pkg-config

set(PKG_CONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(PKG_CONFIG_EXEC_PREFIX "\${prefix}")
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
  set(PKG_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}/osp3")
else()
  set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}/osp3")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
  set(PKG_CONFIG_LIBDIR "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
set(PKG_CONFIG_NAME "osp3")
set(PKG_CONFIG_DESCRIPTION "Library for using an ODROID Smart Power 3")
set(PKG_CONFIG_REQUIRES "")
set(PKG_CONFIG_REQUIRES_PRIVATE "")
set(PKG_CONFIG_CFLAGS "-I\${includedir}")
set(PKG_CONFIG_LIBS "-L\${libdir} -losp3")
set(PKG_CONFIG_LIBS_PRIVATE "")
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/pkgconfig.in
  ${CMAKE_CURRENT_BINARY_DIR}/osp3.pc
  @ONLY
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/osp3.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        COMPONENT OSP3_Development)


# Subdirectories

add_subdirectory(test)
add_subdirectory(utils)
