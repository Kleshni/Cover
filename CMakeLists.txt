cmake_minimum_required(VERSION 3.6.2)

project(Cover VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

include_directories(include)

add_library(
	library SHARED
	library/container.c
	library/eph5-tables.c
	library/eph5.c
	library/rang-image.c
	library/rang-hash.c
	library/rang-jpeg.c
)

set_target_properties(
	library PROPERTIES
	OUTPUT_NAME cover
	VERSION ${PROJECT_VERSION}
	SOVERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
	LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/library/main.map
	LINK_FLAGS -Wl,-version-script=${CMAKE_CURRENT_SOURCE_DIR}/library/main.map
)

target_link_libraries(library jpeg nettle)

add_executable(
	tool
	tool/file.c
	tool/container-file.c
	tool/container.c
	tool/eph5.c
	tool/rang.c
	tool/main.c
)

set_target_properties(
	tool PROPERTIES
	OUTPUT_NAME cover
)

target_link_libraries(tool library jpeg Imlib2 gnutls)

configure_file(library/Doxyfile.in Doxyfile)

add_custom_command(
	OUTPUT html
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_CURRENT_BINARY_DIR}/html
	COMMAND doxygen ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
	DEPENDS library/Doxyfile.in library/index.md include/cover/container.h include/cover/eph5.h include/cover/rang.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_custom_target(documentation ALL DEPENDS html)

include(GNUInstallDirs)

install(
	TARGETS library tool
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/cover DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/html DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES tool/Documentation.md DESTINATION ${CMAKE_INSTALL_DOCDIR})
