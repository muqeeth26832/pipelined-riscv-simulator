/**
 * @file rv5s_vm.h
 * @brief RV5S Pipelined VM definition
 * @author Your Name
 */

#ifndef RV5S_VM_H
#define RV5S_VM_H

#include "vm/vm_base.h"
#include "rv5s_control_unit.h"
#include <stack>
#include <vector>
#include <iostream>
#include <cstdint>

// Pipeline stage buffers (IF/ID, ID/EX, EX/MEM, MEM/WB)
struct IFIDBuffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    bool valid = false;
    bool stall = false;  // For pipeline control
};

struct IDEXBuffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t opcode = 0;
    uint8_t rs1 = 0, rs2 = 0, rd = 0;
    uint32_t imm = 0;
    uint64_t rs1_value = 0;
    uint64_t rs2_value = 0;
    bool valid = false;
    bool stall = false;  // For pipeline control
};

struct EXMEMBuffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t opcode = 0;
    uint8_t rd = 0;
    uint64_t exec_result = 0;
    uint64_t rs2_value = 0;  // for store operations
    bool mem_read = false;
    bool mem_write = false;
    bool reg_write = false;
    bool mem_to_reg = false;
    bool branch_taken = false;
    uint64_t branch_target = 0;
    bool valid = false;
    bool stall = false;  // For pipeline control
};

struct MEMWBBuffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t rd = 0;
    uint64_t result = 0;
    uint64_t mem_result = 0;
    bool reg_write = false;
    bool mem_to_reg = false;
    bool valid = false;
    bool stall = false;  // For pipeline control
};

class RV5SVM : public VmBase {
public:
    RV5SControlUnit control_unit_;
    std::atomic<bool> stop_requested_ = false;

    // Pipeline buffers
    IFIDBuffer if_id_buf_;
    IDEXBuffer id_ex_buf_;
    EXMEMBuffer ex_mem_buf_;
    MEMWBBuffer mem_wb_buf_;

    // Pipeline control
    bool pipeline_stall = false;
    bool pipeline_flush = false;
    int stall_cycles = 0;
    
    // For visualization
    int cycle_count = 0;

    // intermediate variables for pipeline
    uint32_t current_instruction_if = 0;  // instruction in IF stage
    uint32_t current_instruction_id = 0;  // instruction in ID stage
    uint32_t current_instruction_ex = 0;  // instruction in EX stage
    uint32_t current_instruction_mem = 0; // instruction in MEM stage
    uint32_t current_instruction_wb = 0;  // instruction in WB stage

    // Pipeline stage methods
    void PipelineIF();  // Instruction Fetch
    void PipelineID();  // Instruction Decode
    void PipelineEX();  // Execute
    void PipelineMEM(); // Memory Access
    void PipelineWB();  // Write Back

    // Combined pipeline execution
    void ExecutePipelineCycle();

    // Pipeline management
    void FlushPipeline();

    RV5SVM();
    ~RV5SVM() = default;

    void Run() override;
    void DebugRun() override;
    void Step() override;
    void Undo() override;
    void Redo() override;
    void Reset() override;

    void RequestStop() {
        stop_requested_ = true;
    }

    bool IsStopRequested() const {
        return stop_requested_;
    }
    
    void ClearStop() {
        stop_requested_ = false;
    }

    void PrintType() {
        std::cout << "rv5svm" << std::endl;
    }

    // Visualization method
    void PrintPipelineState();
};

#endif // RV5S_VM_H