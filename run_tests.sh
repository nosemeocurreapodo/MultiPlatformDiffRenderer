#!/bin/bash

# Comprehensive test runner script for MultiPlatformRenderer
# This script builds and runs all test suites with proper environment setup

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
TEST_DATA_DIR="${TEST_DATA_DIR:-$(pwd)/tests/data}"
PARALLEL_JOBS=${PARALLEL_JOBS:-$(nproc)}

echo -e "${BLUE}=== MultiPlatformRenderer Test Suite ===${NC}"
echo "Build directory: $BUILD_DIR"
echo "Test data directory: $TEST_DATA_DIR"
echo "Parallel jobs: $PARALLEL_JOBS"
echo

# Function to print section headers
print_section() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

# Function to print success
print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

# Function to print warning
print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Function to print error
print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Check prerequisites
print_section "Checking Prerequisites"

# Check if test data exists
if [ ! -d "$TEST_DATA_DIR" ]; then
    print_warning "Test data directory not found at $TEST_DATA_DIR"
    print_warning "Some tests may be skipped or fail"
else
    print_success "Test data directory found"
fi

# Export test data directory for tests
export TEST_DATA_DIR

# Create build directory
print_section "Setting up Build Environment"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
print_section "Configuring with CMake"
cmake .. \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCOMPILE_GL=ON \
    -DCOMPILE_XRT=OFF \
    -DBUILD_TESTING=ON

print_success "CMake configuration completed"

# Build the project
print_section "Building Project"
make -j"$PARALLEL_JOBS"
print_success "Build completed"

# Function to run a test suite
run_test_suite() {
    local test_name=$1
    local test_executable=$2
    
    echo -e "\n${YELLOW}Running $test_name...${NC}"
    
    if [ -f "$test_executable" ]; then
        if ./"$test_executable" --gtest_output=xml:${test_name}_results.xml; then
            print_success "$test_name passed"
            return 0
        else
            print_error "$test_name failed"
            return 1
        fi
    else
        print_warning "$test_name executable not found, skipping"
        return 0
    fi
}

# Run all test suites
print_section "Running Test Suites"

failed_tests=0
total_tests=0

# Unit tests
((total_tests++))
if ! run_test_suite "CPU Unit Tests" "tests/unit/cpu_tests"; then
    ((failed_tests++))
fi

((total_tests++))
if ! run_test_suite "GL Unit Tests" "tests/unit/gl_tests"; then
    ((failed_tests++))
fi

# Performance tests
((total_tests++))
if ! run_test_suite "Performance Benchmarks" "tests/performance/benchmark_tests"; then
    ((failed_tests++))
fi

# Validation tests
((total_tests++))
if ! run_test_suite "Ground Truth Validation" "tests/validation/ground_truth_tests"; then
    ((failed_tests++))
fi

# Integration tests
((total_tests++))
if ! run_test_suite "Cross-Backend Integration" "tests/integration/cross_backend_tests"; then
    ((failed_tests++))
fi

# Edge case tests
((total_tests++))
if ! run_test_suite "Error Handling Tests" "tests/edge_cases/error_handling_tests"; then
    ((failed_tests++))
fi

# Legacy GL tests (if available)
((total_tests++))
if ! run_test_suite "Legacy GL Tests" "tests/gl/renderergl_test"; then
    ((failed_tests++))
fi

# Summary
print_section "Test Results Summary"
passed_tests=$((total_tests - failed_tests))

echo "Total test suites: $total_tests"
echo -e "Passed: ${GREEN}$passed_tests${NC}"
echo -e "Failed: ${RED}$failed_tests${NC}"

if [ $failed_tests -eq 0 ]; then
    print_success "All test suites passed!"
    echo
    echo -e "${GREEN}🎉 Test infrastructure validation completed successfully!${NC}"
    exit 0
else
    print_error "$failed_tests test suite(s) failed"
    echo
    echo -e "${RED}❌ Some tests failed. Check the output above for details.${NC}"
    exit 1
fi
