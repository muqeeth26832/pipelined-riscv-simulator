# RISC-V Pipeline Simulator

A comprehensive RISC-V simulator supporting multiple pipeline configurations with hazard detection, forwarding, and branch prediction capabilities.

## Features

- **5-Stage Pipeline**: Instruction Fetch (IF), Instruction Decode (ID), Execute (EX), Memory Access (MEM), Write Back (WB)
- **Configurable Pipeline Modes**: 6 different execution modes (0-5) with varying capabilities
- **Hazard Detection**: Automatic detection and resolution of data hazards
- **Result Forwarding**: Eliminates pipeline stalls through result forwarding mechanisms
- **Branch Prediction**: Both static and dynamic branch prediction support
- **Interactive Execution**: Command-line interface for program loading and execution
- **Visual Pipeline Monitoring**: Real-time visualization of pipeline stage states
- **Performance Analysis**: Detailed metrics including CPI, stall cycles, and prediction accuracy

## Pipeline Modes

The simulator supports multiple pipeline configurations:

- **Mode 0**: Single-cycle execution (no pipelining)
- **Mode 1**: Basic pipelining without hazard detection
- **Mode 2**: Pipelining with hazard detection unit (HDU) only
- **Mode 3**: Pipelining with HDU and forwarding unit
- **Mode 4**: Pipelining with static branch prediction
- **Mode 5**: Pipelining with dynamic branch prediction

## Building the Project

The project is written in C++20 and uses CMake for building:

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

### Command-line Execution

```bash
# Show available options
./vm --help

# Execute a program with specific pipeline mode
./vm --pipelined 3 --run examples/loop_example.s

# Start interactive VM with specific mode
./vm --start-vm --pipelined 5
```

### Interactive Mode Commands

In interactive mode (`--start-vm`), use these commands:

- `LOAD <file>` - Load a RISC-V assembly file
- `RUN` - Execute the loaded program
- `STEP` - Execute one pipeline cycle
- `STOP` - Stop program execution
- `RESET` - Reset the virtual machine
- `EXIT` - Exit the VM

## Included Examples

The `examples/` directory contains various test programs:

- `loop_example.s` - Simple factorial calculation
- `branch_prediction_test.s` - Branch prediction test
- `hdu_forwarding_test.s` - HDU and forwarding test
- `pipelined_branch_test.s` - Comprehensive pipeline test
- Additional examples for various RISC-V instructions

## Performance Metrics

The simulator provides detailed performance statistics:
- Total execution cycles
- Instructions retired
- Pipeline stall cycles
- Pipeline flush cycles
- Data and control hazard counts
- Branch prediction accuracy (for prediction modes)

## Architecture Support

- RISC-V RV64I instruction set
- Support for R, I, S, B, U, and J-type instructions
- Memory load/store operations
- Conditional and unconditional branching
- Jump and link instructions

## Testing

Run the automated feature tests:

```bash
./test_features.sh
```

## Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 10+)
- CMake 3.21 or higher
- Standard Unix-like environment

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for more details.

## References

- [RISC-V Specifications](https://riscv.org/specifications/)
- [Five EmbedDev ISA Manual](https://five-embeddev.com/riscv-isa-manual/)