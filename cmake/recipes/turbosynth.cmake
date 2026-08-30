function(_recipe_turbosynth_source)
  set(DTC_REF "master")
  if(CL_REQ_VERSION)
    set(DTC_REF "${CL_REQ_VERSION}")
  endif()

  cl_import_source(
    NAME turbosynth
    REPO https://github.com/pyrite-dev/pmidi.git
    REF ${DTC_REF}
    OPTIONS
      "DECTALKMINI_NO_FILESYSTEM" "ON"
      "DECTALKMINI_NO_CHARSET" "ON"
      "DECTALKMINI_EXAMPLES" "OFF"
      "DECTALKMINI_SDL2_EXAMPLE" "OFF"
  )
endfunction()
