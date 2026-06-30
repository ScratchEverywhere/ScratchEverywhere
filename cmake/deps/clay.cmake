# Force source, it's header only anyway...
CPMAddPackage(
	NAME clay
	GITHUB_REPOSITORY nicbarker/clay
	VERSION 0.15 # We're kind of in between versions...
	DOWNLOAD_ONLY TRUE
	GIT_TAG e6cc369
	PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/clay.patch"
)

add_library(clay INTERFACE)
target_include_directories(clay INTERFACE "${clay_SOURCE_DIR}")
