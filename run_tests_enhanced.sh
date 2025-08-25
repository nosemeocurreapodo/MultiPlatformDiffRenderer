#!/bin/bash

# Enhanced test runner for MultiPlatformRenderer
# Provides comprehensive test execution with reporting and analysis

set -e

# Configuration
BUILD_DIR="build"
TEST_DATA_DIR="${TEST_DATA_DIR:-tests/data}"
PARALLEL_JOBS="${PARALLEL_JOBS:-$(nproc)}"
COMPILE_GL="${COMPILE_GL:-ON}"
COMPILE_XRT="${COMPILE_XRT:-OFF}"
VERBOSE="${VERBOSE:-OFF}"
GENERATE_COVERAGE="${GENERATE_COVERAGE:-OFF}"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print usage information
print_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  -h, --help              Show this help message"
    echo "  -v, --verbose           Enable verbose output"
    echo "  -j, --jobs N            Number of parallel build jobs (default: nproc)"
    echo "  -d, --data-dir PATH     Test data directory (default: tests/data)"
    echo "  --no-gl                 Disable OpenGL backend tests"
    echo "  --enable-xrt            Enable XRT/FPGA backend tests"
    echo "  --coverage              Generate code coverage report"
    echo "  --quick                 Run only fast unit tests"
    echo "  --performance           Run only performance benchmarks"
    echo "  --integration           Run only integration tests"
    echo "  --clean                 Clean build directory before testing"
}

# Parse command line arguments
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                print_usage
                exit 0
                ;;
            -v|--verbose)
                VERBOSE="ON"
                shift
                ;;
            -j|--jobs)
                PARALLEL_JOBS="$2"
                shift 2
                ;;
            -d|--data-dir)
                TEST_DATA_DIR="$2"
                shift 2
                ;;
            --no-gl)
                COMPILE_GL="OFF"
                shift
                ;;
            --enable-xrt)
                COMPILE_XRT="ON"
                shift
                ;;
            --coverage)
                GENERATE_COVERAGE="ON"
                shift
                ;;
            --quick)
                TEST_FILTER="unit"
                shift
                ;;
            --performance)
                TEST_FILTER="performance"
                shift
                ;;
            --integration)
                TEST_FILTER="integration"
                shift
                ;;
            --clean)
                CLEAN_BUILD="ON"
                shift
                ;;
            *)
                log_error "Unknown option: $1"
                print_usage
                exit 1
                ;;
        esac
    done
}

# Validate environment
validate_environment() {
    log_info "Validating test environment..."
    
    # Check required tools
    for tool in cmake make; do
        if ! command -v $tool &> /dev/null; then
            log_error "$tool is required but not installed"
            exit 1
        fi
    done
    
    # Check test data directory
    if [[ ! -d "$TEST_DATA_DIR" ]]; then
        log_warning "Test data directory not found: $TEST_DATA_DIR"
        log_info "Some tests may be skipped"
    fi
    
    # Check OpenGL support
    if [[ "$COMPILE_GL" == "ON" ]]; then
        if ! ldconfig -p | grep -q libGL; then
            log_warning "OpenGL libraries not found, GL tests may fail"
        fi
    fi
    
    log_success "Environment validation completed"
}

# Build the project
build_project() {
    log_info "Building MultiPlatformRenderer..."
    
    if [[ "$CLEAN_BUILD" == "ON" ]] && [[ -d "$BUILD_DIR" ]]; then
        log_info "Cleaning build directory..."
        rm -rf "$BUILD_DIR"
    fi
    
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    
    # Configure CMake
    CMAKE_ARGS=(
        "-DCOMPILE_GL=$COMPILE_GL"
        "-DCOMPILE_XRT=$COMPILE_XRT"
        "-DBUILD_TESTING=ON"
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo"
    )
    
    if [[ "$GENERATE_COVERAGE" == "ON" ]]; then
        CMAKE_ARGS+=("-DCMAKE_CXX_FLAGS=--coverage" "-DCMAKE_C_FLAGS=--coverage")
    fi
    
    cmake "${CMAKE_ARGS[@]}" ..
    
    # Build
    make -j"$PARALLEL_JOBS"
    
    cd ..
    log_success "Build completed successfully"
}

# Run tests with filtering
run_tests() {
    log_info "Running tests..."
    
    cd "$BUILD_DIR"
    
    # Set test environment
    export TEST_DATA_DIR="../$TEST_DATA_DIR"
    
    # Configure CTest arguments
    CTEST_ARGS=("--output-on-failure")
    
    if [[ "$VERBOSE" == "ON" ]]; then
        CTEST_ARGS+=("--verbose")
    fi
    
    # Apply test filtering
    if [[ -n "$TEST_FILTER" ]]; then
        case "$TEST_FILTER" in
            unit)
                CTEST_ARGS+=("-R" "CPURendererTests|GLRendererTests|XRTRendererTests")
                ;;
            performance)
                CTEST_ARGS+=("-R" "benchmark|Performance")
                ;;
            integration)
                CTEST_ARGS+=("-R" "CrossBackend|Integration")
                ;;
        esac
    fi
    
    # Run tests
    if ctest "${CTEST_ARGS[@]}"; then
        log_success "All tests passed!"
        TEST_RESULT=0
    else
        log_error "Some tests failed"
        TEST_RESULT=1
    fi
    
    cd ..
    return $TEST_RESULT
}

# Generate test reports
generate_reports() {
    log_info "Generating test reports..."
    
    cd "$BUILD_DIR"
    
    # Generate XML report for CI
    if command -v ctest &> /dev/null; then
        ctest --output-junit test_results.xml || true
    fi
    
    # Generate coverage report if enabled
    if [[ "$GENERATE_COVERAGE" == "ON" ]] && command -v gcov &> /dev/null; then
        log_info "Generating coverage report..."
        find . -name "*.gcda" -exec gcov {} \; > coverage.log 2>&1 || true
        
        if command -v lcov &> /dev/null; then
            lcov --capture --directory . --output-file coverage.info
            lcov --remove coverage.info '/usr/*' --output-file coverage.info
            lcov --remove coverage.info '*/tests/*' --output-file coverage.info
            
            if command -v genhtml &> /dev/null; then
                genhtml coverage.info --output-directory coverage_html
                log_success "Coverage report generated in build/coverage_html/"
            fi
        fi
    fi
    
    cd ..
    log_success "Reports generated successfully"
}

# Print test summary
print_summary() {
    log_info "Test Summary:"
    echo "============================================"
    echo "Build Configuration:"
    echo "  OpenGL Backend: $COMPILE_GL"
    echo "  XRT Backend: $COMPILE_XRT"
    echo "  Parallel Jobs: $PARALLEL_JOBS"
    echo "  Test Data Dir: $TEST_DATA_DIR"
    echo ""
    
    if [[ -f "$BUILD_DIR/Testing/Temporary/LastTest.log" ]]; then
        cd "$BUILD_DIR"
        TOTAL_TESTS=$(ctest --show-only=json-v1 2>/dev/null | grep -o '"name"' | wc -l || echo "Unknown")
        PASSED_TESTS=$(grep -c "Test.*Passed" Testing/Temporary/LastTest.log || echo "0")
        FAILED_TESTS=$(grep -c "Test.*Failed" Testing/Temporary/LastTest.log || echo "0")
        
        echo "Test Results:"
        echo "  Total Tests: $TOTAL_TESTS"
        echo "  Passed: $PASSED_TESTS"
        echo "  Failed: $FAILED_TESTS"
        
        if [[ "$FAILED_TESTS" -gt 0 ]]; then
            echo ""
            echo "Failed Tests:"
            grep "Test.*Failed" Testing/Temporary/LastTest.log | sed 's/^/  /' || true
        fi
        
        cd ..
    fi
    
    echo "============================================"
}

# Main execution
main() {
    log_info "Starting MultiPlatformRenderer Test Suite"
    
    parse_args "$@"
    validate_environment
    build_project
    
    if run_tests; then
        generate_reports
        print_summary
        log_success "Test suite completed successfully!"
        exit 0
    else
        generate_reports
        print_summary
        log_error "Test suite completed with failures!"
        exit 1
    fi
}

# Execute main function
main "$@"
