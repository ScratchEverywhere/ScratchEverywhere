# Force source, it's header only anyway...
CPMAddPackage(
	NAME clay
	GITHUB_REPOSITORY nicbarker/clay
	VERSION 0.14
	DOWNLOAD_ONLY TRUE
	GIT_TAG main # TODO: Pin Commit
	PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/clay.patch"
)

add_library(deps::clay INTERFACE)
target_include_directories(deps::clay INTERFACE "${clay_SOURCE_DIR}")
