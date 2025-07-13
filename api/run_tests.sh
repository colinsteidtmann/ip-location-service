#!/bin/bash

set -e

echo "Building and running unit tests for IP Location Service..."

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

if [ ! -f "CMakeLists.txt" ]; then
    print_error "This script must be run from the api directory"
    exit 1
fi

print_status "Setting up build directory..."
mkdir -p build_tests
cd build_tests

print_status "Configuring project with CMake..."
cmake ..

print_status "Building project and tests..."
make -j$(nproc)

print_status "Running unit tests..."
if make test VERBOSE=1; then
    print_status "All tests passed!"
else
    print_error "Some tests failed!"
    exit 1
fi

print_status "Running tests with detailed output..."
if [ -f "tests/unit_tests" ]; then
    ./tests/unit_tests --gtest_output=xml:test_results.xml
    print_status "Test results saved to build_tests/test_results.xml"
else
    print_error "Test executable not found!"
    exit 1
fi

print_status "Test run completed successfully!"
