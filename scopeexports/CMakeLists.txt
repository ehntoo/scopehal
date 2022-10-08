find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED IMPORTED_TARGET gtkmm-3.0)

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
	${LIBFFTS_LIBRARIES})

target_include_directories(scopeexports
PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
       ${LIBFFTS_INCLUDE_DIR})

install(TARGETS scopeexports LIBRARY)
