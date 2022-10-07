find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)
pkg_check_modules(SIGCXX REQUIRED IMPORTED_TARGET sigc++-2.0)

set(SCOPEEXPORTS_SOURCES
	ExportWizard.cpp
	CSVExportWizard.cpp
	TouchstoneExportWizard.cpp
	VCDExportWizard.cpp

	scopeexports.cpp
	)

add_library(scopeexports SHARED
	${SCOPEEXPORTS_SOURCES})

target_link_libraries(scopeexports
	scopehal
	PkgConfig::GTKMM
	PkgConfig::SIGCXX
	${LIBFFTS_LIBRARIES})

target_include_directories(scopeexports
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
       ${LIBFFTS_INCLUDE_DIR})

install(TARGETS scopeexports LIBRARY)
