# Preserve per-stage local effect values. These resources never enable RT.
foreach(name rt-light.cfg rt-materials.cfg)
  if(NOT EXISTS "${DEST}/${name}")
    configure_file("${SOURCE}/${name}" "${DEST}/${name}" COPYONLY)
  endif()
endforeach()
