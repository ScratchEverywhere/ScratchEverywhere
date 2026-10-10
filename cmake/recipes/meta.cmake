set(CATALOG_RECIPES
    libcurl:libcurl.cmake
    lua51:lua51.cmake
    sol2:sol2.cmake
    libdlgmod:libdlgmod.cmake
    ryuJS:ryujs.cmake
    plutovg:plutovg.cmake
)

# TODO: "you should make your
# recipies support all platforms,
# just only apply Xbox-specific stuff
# if the variable is set"

if(XBOX)
    list(APPEND CATALOG_RECIPES
        lunasvg:lunasvg.cmake
        miniz:miniz.cmake
    )
endif()
