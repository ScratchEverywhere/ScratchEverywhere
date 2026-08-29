function(_recipe_nlohmann_json_system)
	find_package(nlohmann_json CONFIG QUIET)
endfunction()

function(_recipe_nlohmann_json_package)
	if(CL_PACKAGE_MANAGER STREQUAL "apt")
		set(CL_PACKAGE_NAME "nlohmann-json3-dev" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "pacman")
		set(CL_PACKAGE_NAME "nlohmann-json" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "brew")
		set(CL_PACKAGE_NAME "nlohmann-json" PARENT_SCOPE)
	elseif(CL_PACKAGE_MANAGER STREQUAL "apk")
		set(CL_PACKAGE_NAME "nlohmann-json" PARENT_SCOPE)
	endif()
endfunction()

function(_recipe_nlohmann_json_source)
	set(NLOHMANN_JSON_TAG "v3.12.0")
	if(CL_REQ_VERSION)
		set(NLOHMANN_JSON_TAG "v${CL_REQ_VERSION}")
	endif()

	cl_import_source(
		NAME nlohmann_json
		URL https://github.com/nlohmann/json/archive/refs/tags/${NLOHMANN_JSON_TAG}.tar.gz
		OPTIONS "JSON_BuildTests" "OFF" "JSON_Install" "OFF"
	)

	target_compile_definitions(nlohmann_json INTERFACE JSON_HAS_FILESYSTEM=0 JSON_HAS_EXPERIMENTAL_FILESYSTEM=0)
endfunction()
