file(GLOB tests_list *)

foreach(test_dir ${tests_list})
  if(IS_DIRECTORY ${test_dir})
    message(STATUS "Adding test: ${test_dir}")
    add_subdirectory(${test_dir})
  endif()
endforeach()
