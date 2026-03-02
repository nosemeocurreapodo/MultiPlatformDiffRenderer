include_directories(
    "tests/common"
)

# Ground truth validation tests
add_subdirectory(validation)

# Integration tests for cross-backend comparison
add_subdirectory(integration)

