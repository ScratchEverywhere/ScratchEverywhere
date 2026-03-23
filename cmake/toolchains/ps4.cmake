# Based on PacBrew's ps4.cmake and our original custom Makefile_ps4

cmake_minimum_required(VERSION 3.5)

###################################################################

if(NOT DEFINED ENV{OPENORBIS})
	if(DEFINED ENV{OO_PS4_TOOLCHAIN})
		set(OPENORBIS $ENV{OO_PS4_TOOLCHAIN})
	else()
		set(OPENORBIS /opt/openorbis)
	endif()
    set(ENV{OPENORBIS} ${OPENORBIS})
else()
    set(OPENORBIS $ENV{OPENORBIS})
endif()

if(NOT DEFINED ENV{OO_PS4_TOOLCHAIN})
	set(OO_PS4_TOOLCHAIN ${OPENORBIS})
	set(ENV{OO_PS4_TOOLCHAIN} ${OPENORBIS})
else()
    set(OO_PS4_TOOLCHAIN $ENV{OO_PS4_TOOLCHAIN})
endif()

if(NOT DEFINED ENV{PACBREW})
	set(PACBREW /opt/pacbrew)
	set(ENV{PACBREW} ${PACBREW})
else()
	set(PACBREW $ENV{PACBREW})
endif()

set(PS4 TRUE)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(TARGET x86_64-pc-freebsd-elf)
set(CMAKE_SYSTEM_VERSION 12)
set(CMAKE_CROSSCOMPILING 1)

set(CMAKE_ASM_COMPILER clang CACHE PATH "")
set(CMAKE_C_COMPILER clang CACHE PATH "")
set(CMAKE_CXX_COMPILER clang++ CACHE PATH "")
set(CMAKE_LINKER ld.lld CACHE PATH "")
set(CMAKE_AR llvm-ar CACHE PATH "")
set(CMAKE_RANLIB llvm-ranlib CACHE PATH "")
set(CMAKE_STRIP llvm-strip CACHE PATH "")

# We use the linker directly instead of using the llvm wrapper.
# CMake uses `-Xlinker` for passing llvm linker flags
# added via `add_link_options(... "LINKER:...")`.
# Force the correct linker flag generation:
macro(reset_linker_wrapper_flag)
    set(CMAKE_ASM_LINKER_WRAPPER_FLAG "")
    set(CMAKE_C_LINKER_WRAPPER_FLAG "")
    set(CMAKE_CXX_LINKER_WRAPPER_FLAG "")
endmacro()
variable_watch(CMAKE_ASM_LINKER_FLAG reset_linker_wrapper_flag)
variable_watch(CMAKE_C_LINKER_WRAPPER_FLAG reset_linker_wrapper_flag)
variable_watch(CMAKE_CXX_LINKER_WRAPPER_FLAG reset_linker_wrapper_flag)

set(CMAKE_LIBRARY_ARCHITECTURE x86_64 CACHE INTERNAL "abi")

set(CMAKE_FIND_ROOT_PATH ${OPENORBIS} ${OPENORBIS}/usr)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "Shared libs not available")

###################################################################

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ASM_FLAGS_INIT
  "-target x86_64-pc-freebsd12-elf \
   -D__PS4__ -D__OPENORBIS__ -D__ORBIS__ \
   -DPS4 -D__BSD_VISIBLE -D_BSD_SOURCE \
   -fPIC -funwind-tables \
   -isysroot ${OPENORBIS} -isystem ${OPENORBIS}/include \
   -I${PACBREW}/ps4/openorbis/usr/include -I$(PACBREW)/ps4/openorbis/usr/include/SDL2")

set(CMAKE_C_FLAGS_INIT "${CMAKE_ASM_FLAGS_INIT}")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -I${OPENORBIS}/include/c++/v1")

set(CMAKE_C_STANDARD_LIBRARIES "-lkernel -lc -lclang_rt.builtins-x86_64 -lSceLibcInternal")
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES} -lc++")

set(CMAKE_EXE_LINKER_FLAGS_INIT
  "-m elf_x86_64 -pie --eh-frame-hdr \
   --script ${OPENORBIS}/link.x \
   -L${OPENORBIS}/lib -L${PACBREW}/ps4/openorbis/usr/lib")

# crt1.o may be already added to LDFLAGS from "ps4vars.sh", so remove LDFLAGS env (todo: find a better way...)
set(ENV{LDFLAGS} "" CACHE STRING FORCE)

set(CMAKE_ASM_LINK_EXECUTABLE
  "<CMAKE_LINKER> -o <TARGET> <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> --start-group \
   <OBJECTS> <LINK_LIBRARIES> --end-group")

set(CMAKE_C_LINK_EXECUTABLE
  "<CMAKE_LINKER> -o <TARGET> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> \
  --start-group \
     ${OPENORBIS}/lib/crt1.o ${OPENORBIS}/lib/crti.o \
     <OBJECTS> <LINK_LIBRARIES> \
     ${OPENORBIS}/lib/crtn.o \
  --end-group")

set(CMAKE_CXX_LINK_EXECUTABLE
  "<CMAKE_LINKER> -o <TARGET> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> \
  --start-group \
     ${OPENORBIS}/lib/crt1.o ${OPENORBIS}/lib/crti.o \
     <OBJECTS> <LINK_LIBRARIES> \
     ${OPENORBIS}/lib/crtn.o \
  --end-group")

# Start find_package in config mode
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

# Set pkg-config for the same
find_program(PKG_CONFIG_EXECUTABLE NAMES openorbis-pkg-config HINTS "${PACBREW}/ps4/openorbis/usr/bin")
if(NOT PKG_CONFIG_EXECUTABLE)
    message(WARNING "Could not find openorbis-pkg-config: try installing ps4-openorbis-pkg-config")
endif()

# Stolen from dkp
function(__ps4_target_derive_name outvar target suffix)
	get_target_property(dir ${target} BINARY_DIR)
	get_target_property(outname ${target} OUTPUT_NAME)
	if(NOT outname)
		set(outname "${target}")
	endif()

	set(${outvar} "${dir}/${outname}${suffix}" PARENT_SCOPE)
endfunction()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
	set(PS4_CDIR "linux")
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
	set(PS4_CDIR "macos")
endif()

function(add_self target)
    set(AUTH_INFO "000000000000000000000000001C004000FF000000000080000000000000000000000000000000000000008000400040000000000000008000000000000000080040FFFF000000F000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000")

	set(TARGET_FILE $<TARGET_FILE:${target}>)
	__ps4_target_derive_name(SELF_OUTPUT ${target} ".self")
	__ps4_target_derive_name(OELF_OUTPUT ${target} ".oelf")

    add_custom_command(
		OUTPUT "${SELF_OUTPUT}"
		COMMAND ${CMAKE_COMMAND} -E env "OO_PS4_TOOLCHAIN=${OPENORBIS}" "${OPENORBIS}/bin/${PS4_CDIR}/create-fself" "-in=${TARGET_FILE}" "-out=${OELF_OUTPUT}" "--eboot" "eboot.bin" "--paid" "0x3900000000000002" "--authinfo" "${AUTH_INFO}"
        VERBATIM
        DEPENDS "${target}"
    )
    add_custom_target(
        "${target}_self" ALL
		DEPENDS "${SELF_OUTPUT}"
    )
endfunction()

function(add_pkg target title-id title version)
	set(TARGET_FILE $<TARGET_FILE:${target}>)
	get_target_property(outname ${target} OUTPUT_NAME)
	if(NOT outname)
		set(outname "${target}")
	endif()
	__ps4_target_derive_name(PKG_OUTPUT ${target} ".pkg")
	__ps4_target_derive_name(GP4_OUTPUT ${target} ".gp4")

	set(FILES_TO_EMBED ${ARGN})
	list(APPEND FILES_TO_EMBED eboot.bin sce_module/libc.prx sce_module/libSceFios2.prx sce_sys/param.sfo sce_sys/icon0.png sce_sys/pic0.png sce_sys/pic1.png sce_sys/about/right.sprx)
	string(REPLACE ";" " " FILES_STR "${FILES_TO_EMBED}")

    # Title must not exceed 128 characters
    string(SUBSTRING "${title}" 0 127 title)

    # Format version string in such a way that is acceptable by the PS4
    string(SUBSTRING "${version}" 0 7 verclean)
    string(REGEX MATCH "([0-9]+\\.[0-9]+)" verclean ${verclean})
    if("${verclean}" STREQUAL "")
        message(WARNING "The version string '${version}' is formatted in a way that is incompatable with the PS4, using '01.00'")
        set(verclean "01.00")
    endif()

    # Format content-id based on title-id and version
    string(REPLACE "." "0" vercont ${verclean})
    string(APPEND vercont "00000000")
    string(SUBSTRING "${vercont}" 0 7 vercont)
    set(content_id "IV0001-${title-id}_00-${title-id}${vercont}")
    # export pkg name for end user
    set(PKG_OUT_NAME "${content_id}.pkg" CACHE STRING "ps4 pkg name" FORCE)

    set(attribute 0)
    if(${ARGC} GREATER 5)
        set(attribute ${ARGV5})
    endif()

    set(category "gde")
    if(${ARGC} GREATER 6)
        set(category "${ARGV6}")
    endif()
  
	set(PKGTOOL "${OPENORBIS}/bin/${PS4_CDIR}/PkgTool.Core" CACHE STRING "PKGTOOL" FORCE)
    set(DOTFIX "DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1" CACHE STRING "DOTFIX" FORCE)
  
    add_custom_command(
			OUTPUT "${PKG_OUTPUT}"
            # generate sfo
			COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_new ${CMAKE_BINARY_DIR}/sce_sys/param.sfo
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo APP_TYPE --type Integer --maxsize 4 --value 1
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo APP_VER --type Utf8 --maxsize 8 --value "${verclean}"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo ATTRIBUTE --type Integer --maxsize 4 --value ${attribute}
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo CATEGORY --type Utf8 --maxsize 4 --value "${category}"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo FORMAT --type Utf8 --maxsize 4 --value "obs"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo CONTENT_ID --type Utf8 --maxsize 48 --value "${content_id}"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo SYSTEM_VER --type Integer --maxsize 4 --value 1020
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo TITLE --type Utf8 --maxsize 128 --value "${title}"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo TITLE_ID --type Utf8 --maxsize 12 --value "${title-id}"
            COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} sfo_setentry ${CMAKE_BINARY_DIR}/sce_sys/param.sfo VERSION --type Utf8 --maxsize 8 --value "${verclean}"
            # generate gp4 file
			COMMAND "${OPENORBIS}/bin/${PS4_CDIR}/create-gp4" -out "${GP4_OUTPUT}" --content-id "${content_id}" --files "${FILES_STR}"
            # generate pkg
			COMMAND ${CMAKE_COMMAND} -E env ${DOTFIX} ${PKGTOOL} pkg_build "${GP4_OUTPUT}" ${CMAKE_BINARY_DIR}
			# rename pkg
			COMMAND ${CMAKE_COMMAND} -E rename "${CMAKE_BINARY_DIR}/${content_id}.pkg" "${PKG_OUTPUT}"

            VERBATIM
			DEPENDS "${target}_self"
    )
    add_custom_target(
            "${target}_pkg" ALL
			DEPENDS "${PKG_OUTPUT}"
    )
endfunction()
