/**
 * @file rv5s_vm.cpp
 * @brief RV5S Pipelined VM implementation
 * @author Your Name
 */

#include "vm/rv5s/rv5s_vm.h"
#include "utils.h"
#include "globals.h"
#include "common/instructions.h"
#include "config.h"

#include <iomanip>

#include <cctype>
#include <cstdint>
#include <iostream>
#include <tuple>
#include <stack>  
#include <algorithm>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

using instruction_set::Instruction;
using instruction_set::get_instr_encoding;

RV5SVM::RV5SVM() : VmBase() {
    // Initialize pipeline buffers
    if_id_buf_ = {};
    id_ex_buf_ = {};
    ex_mem_buf_ = {};
    mem_wb_buf_ = {};
    
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}

void RV5SVM::PipelineIF() {
    // Check if we need to stall (not implemented in simple pipeline)
    if (pipeline_stall) {
        // IF stage is stalled, just pass through NOP or hold current state
        return;
    }

    // Check if we need to flush
    if (pipeline_flush) {
        // Don't fetch, pipeline will be flushed
        if_id_buf_.valid = false;
        return;
    }

    // Fetch instruction from memory
    if (program_counter_ < program_size_) {
        if_id_buf_.instruction = memory_controller_.ReadWord(program_counter_);
        if_id_buf_.pc = program_counter_;
        if_id_buf_.valid = true;
        UpdateProgramCounter(4);  // Increment PC for next fetch
    } else {
        // End of program, invalidate buffer
        if_id_buf_.valid = false;
    }
}

void RV5SVM::PipelineID() {
    // Check if we need to stall
    if (pipeline_stall) {
        // ID stage is stalled
        return;
    }

    // Check if we need to flush
    if (pipeline_flush) {
        id_ex_buf_.valid = false;
        return;
    }

    // Move instruction from IF/ID buffer to ID/EX buffer
    if (if_id_buf_.valid) {
        id_ex_buf_.instruction = if_id_buf_.instruction;
        id_ex_buf_.pc = if_id_buf_.pc;
        id_ex_buf_.valid = true;

        // Decode the instruction
        uint32_t instruction = if_id_buf_.instruction;
        uint8_t opcode = instruction & 0b1111111;
        id_ex_buf_.opcode = opcode;
        id_ex_buf_.rs1 = (instruction >> 15) & 0b11111;
        id_ex_buf_.rs2 = (instruction >> 20) & 0b11111;
        id_ex_buf_.rd = (instruction >> 7) & 0b11111;
        id_ex_buf_.imm = ImmGenerator(instruction);
        
        // Read register values
        id_ex_buf_.rs1_value = registers_.ReadGpr(id_ex_buf_.rs1);
        id_ex_buf_.rs2_value = registers_.ReadGpr(id_ex_buf_.rs2);

        // Set control signals using control unit
        control_unit_.SetControlSignals(instruction);

        // Copy control signals to buffer
        // Note: We'll access the control unit directly in the EX stage
    } else {
        id_ex_buf_.valid = false;
    }

    // Invalidate IF/ID buffer (instruction has moved to next stage)
    if_id_buf_.valid = false;
}

void RV5SVM::PipelineEX() {
    // Check if we need to stall
    if (pipeline_stall) {
        // EX stage is stalled
        return;
    }

    // Check if we need to flush
    if (pipeline_flush) {
        ex_mem_buf_.valid = false;
        return;
    }

    // Move instruction from ID/EX buffer to EX/MEM buffer
    if (id_ex_buf_.valid) {
        ex_mem_buf_.instruction = id_ex_buf_.instruction;
        ex_mem_buf_.pc = id_ex_buf_.pc;
        ex_mem_buf_.rd = id_ex_buf_.rd;
        ex_mem_buf_.valid = true;

        uint32_t instruction = id_ex_buf_.instruction;
        uint8_t opcode = id_ex_buf_.opcode;
        uint8_t funct3 = (instruction >> 12) & 0b111;
        uint8_t funct7 = (instruction >> 25) & 0b1111111;

        // Set control signals in EX/MEM buffer
        control_unit_.SetControlSignals(instruction);
        ex_mem_buf_.mem_read = control_unit_.GetMemRead();
        ex_mem_buf_.mem_write = control_unit_.GetMemWrite();
        ex_mem_buf_.reg_write = control_unit_.GetRegWrite();
        ex_mem_buf_.mem_to_reg = control_unit_.GetMemToReg();
        ex_mem_buf_.branch_taken = control_unit_.GetBranch();

        // Get ALU operation
        alu::AluOp aluOperation = control_unit_.GetAluSignal(instruction, control_unit_.GetAluOp());

        // Prepare ALU inputs
        uint64_t alu_input1 = id_ex_buf_.rs1_value;
        uint64_t alu_input2;
        
        // Apply ALU source control
        if (control_unit_.GetAluSrc()) {
            alu_input2 = static_cast<uint64_t>(static_cast<int64_t>(id_ex_buf_.imm));
        } else {
            alu_input2 = id_ex_buf_.rs2_value;
        }

        // Execute ALU operation
        int64_t exec_result;
        bool overflow = false;
        std::tie(exec_result, overflow) = alu_.execute(aluOperation, alu_input1, alu_input2);
        ex_mem_buf_.exec_result = static_cast<uint64_t>(exec_result);
        ex_mem_buf_.rs2_value = id_ex_buf_.rs2_value;  // Needed for store operations

        // Handle branch instructions
        if (control_unit_.GetBranch()) {
            if (opcode == get_instr_encoding(Instruction::kjalr).opcode || 
                opcode == get_instr_encoding(Instruction::kjal).opcode) {
                // JAL/JALR - update branch target and PC
                ex_mem_buf_.branch_target = exec_result;
            } else if (opcode == 0b1100011) { // Branch instructions (BEQ, BNE, etc.)
                int64_t result = 0;
                switch (funct3) {
                    case 0b000: // BEQ
                        result = (id_ex_buf_.rs1_value == id_ex_buf_.rs2_value) ? 0 : 1;
                        ex_mem_buf_.branch_taken = (result == 0);
                        break;
                    case 0b001: // BNE
                        result = (id_ex_buf_.rs1_value != id_ex_buf_.rs2_value) ? 0 : 1;
                        ex_mem_buf_.branch_taken = (result != 0);
                        break;
                    case 0b100: // BLT
                        result = (static_cast<int64_t>(id_ex_buf_.rs1_value) < static_cast<int64_t>(id_ex_buf_.rs2_value)) ? 1 : 0;
                        ex_mem_buf_.branch_taken = (result == 1);
                        break;
                    case 0b101: // BGE
                        result = (static_cast<int64_t>(id_ex_buf_.rs1_value) >= static_cast<int64_t>(id_ex_buf_.rs2_value)) ? 0 : 1;
                        ex_mem_buf_.branch_taken = (result == 0);
                        break;
                    case 0b110: // BLTU
                        result = (id_ex_buf_.rs1_value < id_ex_buf_.rs2_value) ? 1 : 0;
                        ex_mem_buf_.branch_taken = (result == 1);
                        break;
                    case 0b111: // BGEU
                        result = (id_ex_buf_.rs1_value >= id_ex_buf_.rs2_value) ? 0 : 1;
                        ex_mem_buf_.branch_taken = (result == 0);
                        break;
                }
                
                if (ex_mem_buf_.branch_taken) {
                    ex_mem_buf_.branch_target = id_ex_buf_.pc + id_ex_buf_.imm;
                }
            }
        }
    } else {
        ex_mem_buf_.valid = false;
    }

    // Invalidate ID/EX buffer (instruction has moved to next stage)
    id_ex_buf_.valid = false;
}

void RV5SVM::PipelineMEM() {
    // Check if we need to stall
    if (pipeline_stall) {
        // MEM stage is stalled
        return;
    }

    // Check if we need to flush (like after a branch is taken)
    if (pipeline_flush) {
        mem_wb_buf_.valid = false;
        return;
    }

    // Move instruction from EX/MEM buffer to MEM/WB buffer
    if (ex_mem_buf_.valid) {
        mem_wb_buf_.instruction = ex_mem_buf_.instruction;
        mem_wb_buf_.pc = ex_mem_buf_.pc;
        mem_wb_buf_.rd = ex_mem_buf_.rd;
        mem_wb_buf_.valid = true;

        // Set control signals in MEM/WB buffer
        mem_wb_buf_.reg_write = ex_mem_buf_.reg_write;
        mem_wb_buf_.mem_to_reg = ex_mem_buf_.mem_to_reg;

        uint32_t instruction = ex_mem_buf_.instruction;
        uint8_t opcode = instruction & 0b1111111;
        uint8_t funct3 = (instruction >> 12) & 0b111;

        // Handle memory operations
        if (ex_mem_buf_.mem_read) {
            // Load operations
            switch (funct3) {
                case 0b000: // LB
                    mem_wb_buf_.mem_result = static_cast<int8_t>(memory_controller_.ReadByte(ex_mem_buf_.exec_result));
                    break;
                case 0b001: // LH
                    mem_wb_buf_.mem_result = static_cast<int16_t>(memory_controller_.ReadHalfWord(ex_mem_buf_.exec_result));
                    break;
                case 0b010: // LW
                    mem_wb_buf_.mem_result = static_cast<int32_t>(memory_controller_.ReadWord(ex_mem_buf_.exec_result));
                    break;
                case 0b011: // LD
                    mem_wb_buf_.mem_result = memory_controller_.ReadDoubleWord(ex_mem_buf_.exec_result);
                    break;
                case 0b100: // LBU
                    mem_wb_buf_.mem_result = static_cast<uint8_t>(memory_controller_.ReadByte(ex_mem_buf_.exec_result));
                    break;
                case 0b101: // LHU
                    mem_wb_buf_.mem_result = static_cast<uint16_t>(memory_controller_.ReadHalfWord(ex_mem_buf_.exec_result));
                    break;
                case 0b110: // LWU
                    mem_wb_buf_.mem_result = static_cast<uint32_t>(memory_controller_.ReadWord(ex_mem_buf_.exec_result));
                    break;
                default:
                    break;
            }
        }

        if (ex_mem_buf_.mem_write) {
            // Store operations
            switch (funct3) {
                case 0b000: // SB
                    memory_controller_.WriteByte(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFF);
                    break;
                case 0b001: // SH
                    memory_controller_.WriteHalfWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFFFF);
                    break;
                case 0b010: // SW
                    memory_controller_.WriteWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFFFFFFFF);
                    break;
                case 0b011: // SD
                    memory_controller_.WriteDoubleWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value);
                    break;
                default:
                    break;
            }
        }

        // Update PC if branch is taken (simple implementation without hazard detection)
        if (ex_mem_buf_.branch_taken && opcode == 0b1100011) {
            program_counter_ = ex_mem_buf_.branch_target;
            // For simple pipeline without hazard detection, we need to flush the next instructions
            pipeline_flush = true; // This will be handled in the next cycle
        }

        // For JAL/JALR, update PC immediately
        if (ex_mem_buf_.branch_taken && 
            (opcode == get_instr_encoding(Instruction::kjal).opcode || 
             opcode == get_instr_encoding(Instruction::kjalr).opcode)) {
            program_counter_ = ex_mem_buf_.branch_target;
            // For simple pipeline without hazard detection, we need to flush the next instructions
            pipeline_flush = true; // This will be handled in the next cycle
        }

        // Set the result for writeback
        mem_wb_buf_.result = ex_mem_buf_.exec_result;
    } else {
        mem_wb_buf_.valid = false;
    }

    // Invalidate EX/MEM buffer (instruction has moved to next stage)
    ex_mem_buf_.valid = false;
}

void RV5SVM::PipelineWB() {
    // Check if we need to stall
    if (pipeline_stall) {
        // WB stage is stalled
        return;
    }

    // Check if we need to flush
    if (pipeline_flush) {
        // Just invalidate buffer and continue
        mem_wb_buf_.valid = false;
        return;
    }

    // Execute writeback for instruction in MEM/WB buffer
    if (mem_wb_buf_.valid) {
        uint32_t instruction = mem_wb_buf_.instruction;
        uint8_t opcode = instruction & 0b1111111;
        uint8_t rd = mem_wb_buf_.rd;
        uint8_t funct3 = (instruction >> 12) & 0b111;

        // Perform writeback if enabled
        if (mem_wb_buf_.reg_write && rd != 0) { // rd=0 means x0, which is always 0
            uint64_t write_value;

            if (mem_wb_buf_.mem_to_reg) {
                // Write memory result to register
                write_value = mem_wb_buf_.mem_result;
            } else {
                // Write ALU result to register
                write_value = mem_wb_buf_.result;
            }

            // Handle special instructions
            if (opcode == get_instr_encoding(Instruction::kjal).opcode ||
                opcode == get_instr_encoding(Instruction::kjalr).opcode) {
                // For JAL/JALR, write the return address (PC + 4)
                write_value = mem_wb_buf_.pc;
            } else if (opcode == get_instr_encoding(Instruction::klui).opcode) {
                // For LUI, write the immediate shifted left by 12
                int32_t imm = ImmGenerator(instruction);
                write_value = static_cast<uint64_t>(static_cast<int64_t>(imm << 12));
            } else if (opcode == get_instr_encoding(Instruction::kauipc).opcode) {
                // For AUIPC, write PC + immediate shifted left by 12
                int32_t imm = ImmGenerator(instruction);
                write_value = (mem_wb_buf_.pc - 4) + (static_cast<uint64_t>(imm) << 12);
            }

            // Write to register file
            uint64_t old_reg = registers_.ReadGpr(rd);
            registers_.WriteGpr(rd, write_value);

            // Track changes for undo/redo if needed
            if (old_reg != write_value) {
                // For now, we're not tracking these changes in the simple pipeline
                // In a full implementation, this would go in current_delta_
            }
        }
    }

    // Invalidate MEM/WB buffer (instruction has completed)
    mem_wb_buf_.valid = false;
}

void RV5SVM::ExecutePipelineCycle() {
    // Execute each pipeline stage in reverse order to prevent data hazards in this simple implementation
    // In a real implementation, we'd need forwarding and hazard detection
    
    // Reset pipeline control signals for this cycle
    pipeline_flush = false;
    
    // Execute WB stage first
    PipelineWB();
    
    // Execute MEM stage
    PipelineMEM();
    
    // Execute EX stage
    PipelineEX();
    
    // Execute ID stage
    PipelineID();
    
    // Execute IF stage last
    PipelineIF();
    
    // Update cycle count
    cycle_s_++;
    cycle_count++;
    
    // Print pipeline state for visualization
    PrintPipelineState();
}

void RV5SVM::Run() {
    ClearStop();
    uint64_t instruction_executed = 0;

    // Clear pipeline buffers at the start
    if_id_buf_.valid = false;
    id_ex_buf_.valid = false;
    ex_mem_buf_.valid = false;
    mem_wb_buf_.valid = false;
    
    // Reset pipeline control
    pipeline_stall = false;
    pipeline_flush = false;

    while (!stop_requested_ && program_counter_ < program_size_) {
        if (instruction_executed > vm_config::config.getInstructionExecutionLimit())
            break;

        ExecutePipelineCycle();
        
        // Check if we're done (no more valid instructions in pipeline)
        if (!if_id_buf_.valid && !id_ex_buf_.valid && !ex_mem_buf_.valid && !mem_wb_buf_.valid) {
            // All pipeline stages are empty - we've finished execution
            break;
        }
        
        instruction_executed++;
    }
    
    if (program_counter_ >= program_size_) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}

void RV5SVM::DebugRun() {
    Run(); // For now, just run normally in debug mode
}

void RV5SVM::Step() {
    if (program_counter_ < program_size_ || 
        if_id_buf_.valid || id_ex_buf_.valid || ex_mem_buf_.valid || mem_wb_buf_.valid) {
        
        ExecutePipelineCycle();
        
        if (program_counter_ < program_size_ || 
            if_id_buf_.valid || id_ex_buf_.valid || ex_mem_buf_.valid || mem_wb_buf_.valid) {
            std::cout << "VM_STEP_COMPLETED" << std::endl;
            output_status_ = "VM_STEP_COMPLETED";
        } else if (program_counter_ >= program_size_) {
            std::cout << "VM_LAST_INSTRUCTION_STEPPED" << std::endl;
            output_status_ = "VM_LAST_INSTRUCTION_STEPPED";
        }
    } else if (program_counter_ >= program_size_) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}

void RV5SVM::PrintPipelineState() {
    // Print pipeline state for pipelined VM
    // This can be controlled by a config setting if needed in the future
    
    std::cout << "\n╔══════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║                    PIPELINE STATE (Cycle " << std::setw(3) << cycle_count << ")                 ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════════════╣" << std::endl;
    
    // Show what instruction is in each stage with clearer formatting
    std::cout << "║ IF: ";
    if (if_id_buf_.valid) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << if_id_buf_.pc 
                  << ", Instr: 0x" << std::setw(8) << if_id_buf_.instruction << std::setfill(' ') << std::dec;
        std::cout << std::string(32 - (31 - (if_id_buf_.pc > 0 ? 10 : 0) - 12), ' ') << "║" << std::endl;
    } else {
        std::cout << "STAGE EMPTY" << std::string(51, ' ') << "║" << std::endl;
    }
    
    std::cout << "║ ID: ";
    if (id_ex_buf_.valid) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << id_ex_buf_.pc 
                  << ", Instr: 0x" << std::setw(8) << id_ex_buf_.instruction << std::setfill(' ') << std::dec;
        std::cout << std::string(32 - (31 - (id_ex_buf_.pc > 0 ? 10 : 0) - 12), ' ') << "║" << std::endl;
    } else {
        std::cout << "STAGE EMPTY" << std::string(51, ' ') << "║" << std::endl;
    }
    
    std::cout << "║ EX: ";
    if (ex_mem_buf_.valid) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << ex_mem_buf_.pc 
                  << ", Instr: 0x" << std::setw(8) << ex_mem_buf_.instruction << std::setfill(' ') << std::dec;
        std::cout << std::string(32 - (31 - (ex_mem_buf_.pc > 0 ? 10 : 0) - 12), ' ') << "║" << std::endl;
    } else {
        std::cout << "STAGE EMPTY" << std::string(51, ' ') << "║" << std::endl;
    }
    
    std::cout << "║ MEM: ";
    if (mem_wb_buf_.valid) {  // This is wrong - should be ex_mem_buf_
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << ex_mem_buf_.pc 
                  << ", Instr: 0x" << std::setw(8) << ex_mem_buf_.instruction << std::setfill(' ') << std::dec;
        std::cout << std::string(31 - (31 - (ex_mem_buf_.pc > 0 ? 10 : 0) - 12), ' ') << "║" << std::endl;
    } else {
        std::cout << "STAGE EMPTY" << std::string(51, ' ') << "║" << std::endl;
    }
    
    std::cout << "║ WB: ";
    // Show if there was an instruction in WB stage in the previous cycle
    // that completed in this cycle
    if (mem_wb_buf_.valid) {  // This means there's an instruction currently in WB, not one that completed
        std::cout << "INSTRUCTION IN WB STAGE" << std::string(37, ' ') << "║" << std::endl;
    } else {
        std::cout << "NO INSTRUCTION IN WB" << std::string(39, ' ') << "║" << std::endl;
    }
    
    std::cout << "╚══════════════════════════════════════════════════════════════╝" << std::endl;
    std::cout << std::endl;
}

void RV5SVM::Undo() {
    // Not implemented for pipeline version
    std::cout << "VM_UNDO_NOT_SUPPORTED" << std::endl;
    output_status_ = "VM_UNDO_NOT_SUPPORTED";
}

void RV5SVM::Redo() {
    // Not implemented for pipeline version
    std::cout << "VM_REDO_NOT_SUPPORTED" << std::endl;
}

void RV5SVM::Reset() {
    program_counter_ = 0;
    instructions_retired_ = 0;
    cycle_s_ = 0;
    cycle_count = 0;
    
    registers_.Reset();
    memory_controller_.Reset();
    control_unit_.Reset();
    
    // Reset pipeline buffers
    if_id_buf_ = {};
    id_ex_buf_ = {};
    ex_mem_buf_ = {};
    mem_wb_buf_ = {};
    
    // Reset pipeline control
    pipeline_stall = false;
    pipeline_flush = false;
    stall_cycles = 0;
}

void RV5SVM::FlushPipeline() {
    if_id_buf_.valid = false;
    id_ex_buf_.valid = false;
    ex_mem_buf_.valid = false;
    mem_wb_buf_.valid = false;
}