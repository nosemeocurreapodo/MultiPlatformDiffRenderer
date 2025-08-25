include(GoogleTest)

# Unit tests for individual backends
add_subdirectory(unit)

# Performance benchmarking tests
add_subdirectory(performance)

# Ground truth validation tests
add_subdirectory(validation)

# Integration tests for cross-backend comparison
add_subdirectory(integration)

# Edge case and error handling tests
add_subdirectory(edge_cases)

# if(COMPILE_XRT)
#   add_subdirectory(xrt)
# endif()

