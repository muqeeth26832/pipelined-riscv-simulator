# RISC-V Pipeline Simulator

This project implements a RISC-V simulator with pipelined execution, hazard detection, forwarding, and branch prediction capabilities.

## Features

### Pipeline Modes
The simulator supports multiple pipeline configurations:

- **Mode 0**: Single-cycle (no pipelining)
- **Mode 1**: Basic pipelining (no hazard detection or forwarding)
- **Mode 2**: Pipelining with hazard detection (no forwarding) - HDU only
- **Mode 3**: Pipelining with hazard detection and forwarding - HDU + Forwarding
- **Mode 4**: Pipelining with static branch prediction (predict not taken)
- **Mode 5**: Pipelining with dynamic branch prediction (2-bit counter)

### Core Components
- **Hazard Detection Unit (HDU)**: Detects data hazards and load-use hazards
- **Forwarding Unit**: Enables data forwarding to reduce pipeline stalls
- **Branch Prediction**: Static and dynamic prediction strategies

## Building the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Running Examples

### Command-line Execution
```bash
# Run with different pipeline modes
./vm --pipelined 3 --run examples/loop_example.s
./vm --pipelined 5 --run examples/branch_prediction_test.s

# Interactive mode
./vm --start-vm --pipelined 3
```

### Interactive Commands
In interactive mode, you can use the following commands:

```
LOAD examples/hdu_forwarding_test.s    # Load a program
RUN                                  # Execute the loaded program
STEP                                 # Execute one instruction
STOP                                 # Stop execution
RESET                                # Reset the VM
EXIT                                 # Exit the VM
```

### Example Programs

1. `examples/loop_example.s` - Basic factorial calculation
2. `examples/branch_prediction_test.s` - Tests branch prediction
3. `examples/hdu_forwarding_test.s` - Tests HDU and forwarding
4. `examples/pipelined_branch_test.s` - Comprehensive test with branches

## Testing Different Features

### Test Hazard Detection Unit (HDU)
```bash
./vm --pipelined 2 --run examples/hdu_forwarding_test.s
```

### Test HDU + Forwarding
```bash  
./vm --pipelined 3 --run examples/hdu_forwarding_test.s
```

### Test Static Branch Prediction
```bash
./vm --pipelined 4 --run examples/branch_prediction_test.s
```

### Test Dynamic Branch Prediction
```bash
./vm --pipelined 5 --run examples/branch_prediction_test.s
```

## Key Implementation Details

### Pipeline Stages
- **IF**: Instruction Fetch
- **ID**: Instruction Decode & Register Read  
- **EX**: Execute/ALU Operation
- **MEM**: Memory Access
- **WB**: Write Back

### Hazard Detection
- **Load-Use Hazards**: Detected between MEM and ID stages
- **Data Hazards**: RAW hazards between EX/MEM/WB and ID stages

### Forwarding Paths
- From EX/MEM buffer result to EX stage (higher priority)
- From MEM/WB buffer result to EX stage (lower priority)

## Performance Statistics
The simulator shows:
- Total cycles
- Instructions retired
- Stall cycles
- Flush cycles  
- Data hazards detected
- Control hazards
- Branch prediction accuracy (for modes 4-5)
- CPI (Cycles Per Instruction)

## Example Output
Running in pipelined mode shows visual pipeline state:
```
╔════════════════════════════════════════════════════════════════════════════╗
║ PIPELINE STATE - Cycle 00003 (Mode 3) ║
╠════════════════════════════════════════════════════════════════════════════╣
║ IF │ PC: 0x0000000c │ JAL     │ 0x0000006f                          ║
║ ID │ PC: 0x00000008 │ ADDI    │ rs1:x02 rs2:x00 rd:x03  [FWD]       ║
║ EX │ PC: 0x00000004 │ ADDI    │ Result: 0x0000000a                  ║
║ MEM│ PC: 0x00000000 │ LUI     │ AluData: 0x00100000                 ║
║ WB │ Instruction completed and retired                             ║
```