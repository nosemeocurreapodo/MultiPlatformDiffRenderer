include(GoogleTest)

#add_subdirectory(cpu)

#add_subdirectory(multi)

if(COMPILE_GL)
  add_subdirectory(gl)
endif()

if(COMPILE_XRT)
  add_subdirectory(xrt)
endif()

