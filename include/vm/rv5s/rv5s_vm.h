/**
 * @file rv5s_vm.h
 * @brief RV5S Pipelined VM header with multiple pipeline modes
 * @author
 */

#ifndef RV5S_VM_H
#define RV5S_VM_H

#include "vm/vm_base.h"
#include "vm/rv5s/rv5s_control_unit.h"
#include <cstdint>
#include <atomic>
#include <vector>

// Pipeline buffer structures
struct IF_ID_Buffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    bool valid = false;
    bool is_nop = false;  // For inserted bubbles
    bool branch_predicted_taken = false;  // For branch prediction
    uint64_t predicted_target = 0;        // Predicted branch target if taken
};

struct ID_EX_Buffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t opcode = 0;
    uint8_t rs1 = 0;
    uint8_t rs2 = 0;
    uint8_t rd = 0;
    int32_t imm = 0;
    uint64_t rs1_value = 0;
    uint64_t rs2_value = 0;
    bool valid = false;
    bool is_nop = false;

    // Control signals
    bool alu_src = false;
    bool mem_to_reg = false;
    bool reg_write = false;
    bool mem_read = false;
    bool mem_write = false;
    bool branch = false;
    bool alu_op = false;
    
    // Branch prediction
    bool branch_predicted_taken = false;
    uint64_t predicted_target = 0;
};

struct EX_MEM_Buffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t rd = 0;
    uint64_t exec_result = 0;
    uint64_t rs2_value = 0;
    bool valid = false;
    bool is_nop = false;

    // Control signals
    bool mem_read = false;
    bool mem_write = false;
    bool reg_write = false;
    bool mem_to_reg = false;
    bool branch_taken = false;
    uint64_t branch_target = 0;
    
    // Branch prediction
    bool branch_predicted_taken = false;
    bool branch_mispredicted = false;
    
    // Branch control flag passed from ID/EX to EX/MEM
    bool branch = false;
};

struct MEM_WB_Buffer {
    uint32_t instruction = 0;
    uint64_t pc = 0;
    uint8_t rd = 0;
    uint64_t result = 0;
    uint64_t mem_result = 0;
    bool valid = false;
    bool is_nop = false;

    // Control signals
    bool reg_write = false;
    bool mem_to_reg = false;
};

// Forwarding unit structure
struct ForwardingUnit {
    enum ForwardType {
        NO_FORWARD = 0,
        FORWARD_FROM_MEM = 1,
        FORWARD_FROM_WB = 2
    };

    ForwardType forward_a = NO_FORWARD;
    ForwardType forward_b = NO_FORWARD;
};

class RV5SVM : public VmBase {
public:
    RV5SVM();
    ~RV5SVM() override = default;

    void Run() override;
    void DebugRun() override;
    void Step() override;
    void Undo() override;
    void Redo() override;
    void Reset() override;

    // Implement pure virtual functions from VmBase
    void RequestStop() override { stop_requested_.store(true); }
    bool IsStopRequested() const override { return stop_requested_.load(); }

private:
    // Clear stop flag
    void ClearStop() { stop_requested_.store(false); }

    // Pipeline stage functions
    void PipelineIF();
    void PipelineID();
    void PipelineEX();
    void PipelineMEM();
    void PipelineWB();

    // Execute one complete pipeline cycle
    void ExecutePipelineCycle();

    // Hazard detection and handling
    bool DetectDataHazard();
    bool DetectLoadUseHazard();
    void HandleDataHazard();
    void InsertStall();

    // Forwarding
    ForwardingUnit DetectForwarding();
    uint64_t GetForwardedValue(uint8_t reg, ForwardingUnit::ForwardType forward_type);

    // Branch handling
    void HandleBranch();
    void FlushPipeline();
    
    // Branch prediction
    bool PredictBranch(uint64_t pc);
    void UpdateBranchPredictor(uint64_t pc, bool actual_taken);
    void HandleBranchPrediction();

    // Utility functions
    void PrintPipelineState();
    void PrintHazardInfo();
    std::string GetInstructionName(uint32_t instruction);

    // Pipeline buffers
    IF_ID_Buffer if_id_buf_;
    ID_EX_Buffer id_ex_buf_;
    EX_MEM_Buffer ex_mem_buf_;
    MEM_WB_Buffer mem_wb_buf_;
    
    // Branch prediction structures
    struct BranchPredictorEntry {
        bool prediction = false;  // Predicted branch outcome (false = not taken, true = taken)
        uint8_t state = 0;        // For dynamic prediction (two-bit saturating counter)
    };

    // Control unit
    RV5SControlUnit control_unit_;

    // Pipeline control signals
    bool pipeline_stall = false;
    bool pipeline_flush = false;

    // Stop flag
    std::atomic<bool> stop_requested_{false};

    // Statistics
    uint64_t cycle_count = 0;
    uint64_t stall_cycles = 0;
    uint64_t flush_cycles = 0;
    uint64_t data_hazards = 0;
    uint64_t control_hazards = 0;

    // Forwarding paths
    uint64_t ex_mem_forward_data = 0;
    uint64_t mem_wb_forward_data = 0;
    
    // Branch prediction
    std::vector<BranchPredictorEntry> branch_predictor_table_;
    size_t branch_predictor_size_ = 1024;  // Default size, configurable
    
    // Branch statistics
    uint64_t branch_predictions = 0;
    uint64_t branch_mispredictions = 0;
};

#endif // RV5S_VM_H
