# MultiPlatformRenderer Test Suite

This directory contains comprehensive tests for the MultiPlatformRenderer project, covering all backends (CPU, OpenGL, XRT/FPGA) and various testing scenarios.

## Recent Improvements (Latest Update)

The testing infrastructure has been significantly enhanced with:

- **Enhanced Test Framework**: Added test result reporting, memory monitoring, and automated analytics
- **Improved CI/CD Integration**: GitHub Actions workflow for automated testing across different configurations
- **Advanced Test Runner**: New `run_tests_enhanced.sh` script with filtering, coverage, and reporting capabilities
- **Better Performance Monitoring**: Integrated memory usage tracking and performance thresholds
- **Centralized Configuration**: All test parameters now managed through `test_config.h`

## Test Architecture

The test infrastructure is organized into several specialized test suites:

### 📁 `common/`
- **`test_framework.h`**: Shared test framework with utilities for:
  - Dataset loading and preprocessing
  - Mesh creation and texture operations
  - Performance timing and validation
  - Cross-backend comparison utilities
  - Debug image saving

### 📁 `unit/`
- **`cpu_tests.cpp`**: Isolated CPU renderer tests
- **`gl_tests.cpp`**: Isolated OpenGL renderer tests
- Tests basic functionality, error handling, memory management, and numerical precision

### 📁 `performance/`
- **`benchmark_tests.cpp`**: Performance benchmarking suite
- Measures execution times, memory usage, and comparative speedup
- Tests scaling with different resolutions and mesh complexities

### 📁 `validation/`
- **`ground_truth_tests.cpp`**: Ground truth validation tests
- Compares renderer outputs against expected dataset results
- Validates depth, image, gradient (DIDxy), and Jacobian pipelines

### 📁 `integration/`
- **`cross_backend_tests.cpp`**: Cross-backend comparison tests
- Validates consistency between CPU and OpenGL implementations
- Tests numerical precision and performance characteristics

### 📁 `edge_cases/`
- **`error_handling_tests.cpp`**: Edge case and error handling tests
- Tests empty meshes, degenerate triangles, extreme transformations
- Memory pressure scenarios and thread safety basics

### 📁 `gl/` (Legacy)
- **`renderergl_test.cpp`**: Original GL test (kept for compatibility)

## Quick Start

### Prerequisites
- CMake 3.10+
- OpenCV 4.x
- Eigen3
- GoogleTest
- OpenGL/EGL (for GL tests)
- Test dataset (ICL-NUIM format recommended)

### Running All Tests
```bash
# From project root
./run_tests.sh
```

### Running Individual Test Suites
```bash
# Build the project
mkdir build && cd build
cmake .. -DCOMPILE_GL=ON -DBUILD_TESTING=ON
make -j$(nproc)

# Run specific test suites
./tests/unit/cpu_tests
./tests/unit/gl_tests
./tests/performance/benchmark_tests
./tests/validation/ground_truth_tests
./tests/integration/cross_backend_tests
./tests/edge_cases/error_handling_tests
```

## Test Data Setup

Set the `TEST_DATA_DIR` environment variable to point to your test dataset:

```bash
export TEST_DATA_DIR=/path/to/test/data
```

Expected directory structure:
```
test_data/
├── rgbd_dataset_freiburg1_floor_part/
│   ├── depth/
│   ├── rgb/
│   └── groundtruth.txt
└── trajectory_files/
    └── *.txt
```

## Test Categories

### 🔧 Unit Tests
- **Purpose**: Test individual renderer components in isolation
- **Coverage**: Basic functionality, error handling, memory management
- **Backends**: CPU and OpenGL tested separately
- **Key Features**:
  - Depth, image, gradient, and Jacobian renderers
  - Edge case handling (empty meshes, invalid parameters)
  - Memory management validation
  - Numerical precision checks

### ⚡ Performance Tests
- **Purpose**: Benchmark renderer performance and compare backends
- **Metrics**: Execution time, memory usage, throughput
- **Key Features**:
  - Multi-resolution scaling tests
  - CPU vs GPU speedup measurements
  - Memory usage profiling
  - Performance regression detection

### ✅ Validation Tests
- **Purpose**: Validate against ground truth data
- **Coverage**: All rendering pipelines with real-world data
- **Key Features**:
  - Ground truth comparison with error metrics
  - Cross-backend consistency validation
  - Performance threshold validation
  - Debug image output for visual inspection

### 🔗 Integration Tests
- **Purpose**: Test cross-backend compatibility and consistency
- **Coverage**: CPU-GL comparison across all pipelines
- **Key Features**:
  - Pixel-level accuracy comparison
  - Performance consistency validation
  - Mesh complexity scaling
  - Numerical precision analysis

### 🛡️ Edge Case Tests
- **Purpose**: Test robustness and error handling
- **Coverage**: Boundary conditions and error scenarios
- **Key Features**:
  - Empty and degenerate geometry handling
  - Extreme parameter values
  - Memory pressure scenarios
  - Thread safety basics

## Configuration

### CMake Options
- `COMPILE_GL=ON/OFF`: Enable/disable OpenGL backend tests
- `COMPILE_XRT=ON/OFF`: Enable/disable XRT/FPGA backend tests
- `BUILD_TESTING=ON/OFF`: Enable/disable all tests

### Environment Variables
- `TEST_DATA_DIR`: Path to test dataset directory
- `PARALLEL_JOBS`: Number of parallel build jobs (default: nproc)

### Test Thresholds
Key validation thresholds (configurable in test files):
- **Pixel error tolerance**: 1e-3 for depth, 1e-2 for images
- **Performance thresholds**: Backend-specific timing limits
- **Memory limits**: Resolution-dependent memory usage bounds

## Debugging and Troubleshooting

### Debug Image Output
Tests automatically save debug images when failures occur:
- Location: `build/test_debug_images/`
- Formats: Depth maps, error visualizations, difference images
- Naming: `{test_name}_{backend}_{pipeline}_{timestamp}.png`

### Common Issues
1. **Missing test data**: Set `TEST_DATA_DIR` correctly
2. **GL context errors**: Ensure EGL/X11 is available in test environment
3. **Memory issues**: Reduce test resolution or increase system memory
4. **Precision failures**: Check numerical thresholds in test configuration

### Verbose Output
Run tests with additional debugging:
```bash
./tests/unit/cpu_tests --gtest_verbose
GLOG_v=2 ./tests/validation/ground_truth_tests
```

## Performance Expectations

### Typical Performance (1024x768 resolution):
- **CPU Depth Rendering**: ~50-100ms
- **GPU Depth Rendering**: ~5-15ms
- **CPU Image Rendering**: ~100-200ms
- **GPU Image Rendering**: ~10-25ms
- **Expected GPU Speedup**: 3-10x depending on complexity

### Memory Usage:
- **Base overhead**: ~50MB per backend
- **Per-megapixel**: ~20MB for textures and buffers
- **Peak usage**: 2-3x steady state during initialization

## Contributing

When adding new tests:
1. Follow the existing test structure and naming conventions
2. Use the shared test framework utilities
3. Add appropriate CMakeLists.txt entries
4. Update this README with new test descriptions
5. Ensure tests are deterministic and reproducible

### Test Naming Convention
- Test files: `{category}_tests.cpp`
- Test cases: `{Backend}{Pipeline}_{TestType}`
- Example: `CPUDepthRenderer_BasicFunctionality`

## Integration with CI/CD

The test suite is designed for automated testing:
- All tests return proper exit codes
- XML output supported for CI integration
- Configurable timeouts and resource limits
- Parallel execution support

Example CI configuration:
```yaml
- name: Run Tests
  run: |
    export TEST_DATA_DIR=${{ github.workspace }}/test_data
    ./run_tests.sh
  timeout-minutes: 30
```
