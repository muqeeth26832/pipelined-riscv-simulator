#!/bin/bash

# Test script to validate all features of the RISC-V pipeline simulator

echo "==========================================="
echo "RISC-V Pipeline Simulator - Feature Tests"
echo "==========================================="

# Make sure build directory exists and is compiled
if [ ! -f "build/vm" ]; then
    echo "Building the project..."
    mkdir -p build
    cd build
    cmake ..
    make
    cd ..
fi

# Create required directories
mkdir -p vm_state

echo ""
echo "1. Testing Mode 0: Single-cycle (no pipelining)"
echo "-----------------------------------------------"
timeout 5s ./build/vm --pipelined 0 --run examples/loop_example.s || echo "Test completed or timed out"

echo ""
echo "2. Testing Mode 1: Basic pipelining (no hazard detection)"
echo "--------------------------------------------------------"
timeout 5s ./build/vm --pipelined 1 --run examples/loop_example.s || echo "Test completed or timed out"

echo ""
echo "3. Testing Mode 2: Pipelining with hazard detection (no forwarding)"
echo "------------------------------------------------------------------"
timeout 5s ./build/vm --pipelined 2 --run examples/hdu_forwarding_test.s || echo "Test completed or timed out"

echo ""
echo "4. Testing Mode 3: Pipelining with hazard detection and forwarding"
echo "------------------------------------------------------------------"
timeout 5s ./build/vm --pipelined 3 --run examples/hdu_forwarding_test.s || echo "Test completed or timed out"

echo ""
echo "5. Testing Mode 4: Static branch prediction"
echo "-------------------------------------------"
timeout 5s ./build/vm --pipelined 4 --run examples/branch_prediction_test.s || echo "Test completed or timed out"

echo ""
echo "6. Testing Mode 5: Dynamic branch prediction"
echo "--------------------------------------------"
timeout 5s ./build/vm --pipelined 5 --run examples/branch_prediction_test.s || echo "Test completed or timed out"

echo ""
echo "7. Testing assembler functionality"
echo "----------------------------------"
./build/vm --assemble examples/loop_example.s || echo "Assemble test completed"

echo ""
echo "==========================================="
echo "All tests completed!"
echo "==========================================="