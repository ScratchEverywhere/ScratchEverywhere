function(_dep_system_libretro)
endfunction()

function(_dep_source_libretro)
	include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM.cmake")

	CPMAddPackage(
		NAME libretro
		GITHUB_REPOSITORY libretro/libretro-common
		GIT_TAG master
		DOWNLOAD_ONLY ON
	)

	add_library(libretro INTERFACE)
	target_include_directories(libretro INTERFACE ${libretro_SOURCE_DIR}/include)
endfunction()
