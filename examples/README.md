# Test Examples for RISC-V Simulator

This directory contains various RISC-V assembly test examples that demonstrate different features of the simulator:

## Example Files

1. **loop_example.s** - Simple factorial calculation using a loop (default example)
2. **branch_prediction_test.s** - Tests static and dynamic branch prediction
3. **hdu_forwarding_test.s** - Tests Hazard Detection Unit and forwarding mechanisms
4. **pipelined_branch_test.s** - Comprehensive test for pipelined execution with branches
5. **branch_test.s** - Conditional branch operations
6. **load_store_test_1.s** - Load and store operations
7. **load_store_test_2.s** - More complex load/store operations
8. **load_test.s** - Load operations
9. **load_test_2.s** - Additional load operations
10. **lui_auipc_test.s** - LUI and AUIPC instruction tests
11. **jal_test.s** - Jump and link instruction tests
12. **various other examples** - Additional test cases

## Running Tests

To run the simulator with different pipeline modes:

### Basic pipelining (no HDU or forwarding):
```bash
./build/vm --start-vm --pipelined 1
```

### Pipelining with hazard detection (no forwarding):
```bash
./build/vm --start-vm --pipelined 2
```

### Pipelining with hazard detection and forwarding:
```bash
./build/vm --start-vm --pipelined 3
```

### Pipelining with static branch prediction:
```bash
./build/vm --start-vm --pipelined 4
```

### Pipelining with dynamic branch prediction:
```bash
./build/vm --start-vm --pipelined 5
```

## Testing Commands

From the VM interface, you can load examples:
- `LOAD examples/branch_prediction_test.s` - Load branch prediction test
- `LOAD examples/hdu_forwarding_test.s` - Load HDU/forwarding test
- `LOAD examples/pipelined_branch_test.s` - Load comprehensive test
- `RUN` - Execute the loaded program
- `STEP` - Step through instructions
- `STOP` - Stop execution
- `RESET` - Reset the VM