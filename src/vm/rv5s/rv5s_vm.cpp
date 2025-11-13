/**
 * @file rv5s_vm.cpp
 * @brief RV5S Pipelined VM implementation with hazard detection and forwarding
 *
 * This implementation provides a 5-stage pipeline (IF, ID, EX, MEM, WB) with:
 * - Load-use hazard detection and resolution
 * - Data hazard detection for ALU operations
 * - Forwarding unit to eliminate unnecessary stalls
 * - Configurable pipeline modes (0-3)
 *
 * Pipeline Stages:
 * - IF: Instruction Fetch
 * - ID: Instruction Decode & Register Read
 * - EX: Execute/ALU Operation
 * - MEM: Memory Access
 * - WB: Write Back
 *
 * Hazard Handling:
 * - Load-Use Hazards: Detected between MEM and ID stages, resolved with stalls
 * - Data Hazards: Detected and resolved with forwarding in Mode 3, stalls in Mode 2
 *
 * Example of hazard resolution:
 * For sequence: li a0,1; li a1,1; addi a2,a0,a1
 * - In Mode 2: Will insert appropriate stalls to resolve dependencies
 * - In Mode 3: Will use forwarding to eliminate most stalls
 *
 * @author Vishank Singh, https://github.com/VishankSingh
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
#include <thread>
#include <chrono>
#include <sstream>
#include<mutex>

std::mutex print_mutex_;

using instruction_set::Instruction;
using instruction_set::get_instr_encoding;
RV5SVM::RV5SVM() : VmBase() {
    if_id_buf_ = {};
    id_ex_buf_ = {};
    ex_mem_buf_ = {};
    mem_wb_buf_ = {};
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}
bool RV5SVM::DetectLoadUseHazard() {
    // Mode 1: No hazard detection (no stalls)
    if (globals::pipelined_mode == 1) {
        return false;
    }
    /**
     * Load-use hazard detection:
     * This occurs when a load instruction is about to enter the MEM stage (in EX/MEM buffer)
     * and its result is needed by an instruction in the ID stage as an operand.
     *
     * In a 5-stage pipeline:
     * - Load instruction is in EX/MEM buffer (address calculated, about to read memory in MEM)
     * - Instruction in ID stage (needs to read registers) depends on the load result
     * - The load result is not yet available (MEM stage not executed yet)
     * - This creates a data hazard that requires a stall (1 cycle in standard designs)
     */
    if (!if_id_buf_.valid || if_id_buf_.is_nop) {
        return false;
    }
    uint32_t id_instr = if_id_buf_.instruction;
    uint8_t rs1 = (id_instr >> 15) & 0b11111;
    uint8_t rs2 = (id_instr >> 20) & 0b11111;
    // Check if the instruction going to MEM (ex_mem_buf_ after EX) is a load
    // and its destination matches rs1/rs2 of ID instruction
    if (ex_mem_buf_.valid && ex_mem_buf_.mem_read && ex_mem_buf_.reg_write && ex_mem_buf_.rd != 0 &&
        (ex_mem_buf_.rd == rs1 || ex_mem_buf_.rd == rs2)) {
        return true;
    }
    return false;
}
bool RV5SVM::DetectDataHazard() {
    // Mode 1: No hazard detection (no stalls)
    if (globals::pipelined_mode == 1) {
        return false;
    }

    /**
     * Data hazard detection:
     * Check for RAW (Read After Write) hazards where an instruction in ID stage
     * needs to read a register that will be written by instructions in EX or MEM stages.
     */
    if (!if_id_buf_.valid || if_id_buf_.is_nop) {
        return false;
    }

    uint32_t id_instr = if_id_buf_.instruction;
    uint8_t rs1 = (id_instr >> 15) & 0b11111;
    uint8_t rs2 = (id_instr >> 20) & 0b11111;
    bool hazard = false;

    // Mode 2: Stall for all data hazards (no forwarding)
    if (globals::pipelined_mode == 2) {
        // Check EX stage instruction (now in ex_mem_buf_ after EX, going to MEM)
        if (ex_mem_buf_.valid && ex_mem_buf_.reg_write && ex_mem_buf_.rd != 0) {
            if (ex_mem_buf_.rd == rs1 || ex_mem_buf_.rd == rs2) {
                hazard = true;
            }
        }
        // Check MEM stage instruction (now in mem_wb_buf_ after MEM, going to WB)
        if (mem_wb_buf_.valid && mem_wb_buf_.reg_write && mem_wb_buf_.rd != 0) {
            if (mem_wb_buf_.rd == rs1 || mem_wb_buf_.rd == rs2) {
                hazard = true;
            }
        }
    }
    // Mode 3+: Only detect load-use hazards, data hazards handled by forwarding
    else if (globals::pipelined_mode >= 3) {
        // Only load-use hazards need stalling even with forwarding
        // This is already handled in DetectLoadUseHazard()
        hazard = false;
    }

    return hazard;
}
ForwardingUnit RV5SVM::DetectForwarding() {
    ForwardingUnit fu;

    // Only enable forwarding for modes 3 and above
    if (globals::pipelined_mode < 3) {
        return fu; // No forwarding for modes 1-2
    }

    // Rest of the forwarding logic remains the same...
    if (!id_ex_buf_.valid || id_ex_buf_.is_nop) {
        return fu;
    }

    uint8_t rs1 = id_ex_buf_.rs1;
    uint8_t rs2 = id_ex_buf_.rs2;

    // Forward from MEM stage (higher priority) to EX stage
    if (ex_mem_buf_.valid && ex_mem_buf_.reg_write && ex_mem_buf_.rd != 0) {
        if (ex_mem_buf_.rd == rs1) {
            fu.forward_a = ForwardingUnit::FORWARD_FROM_MEM;
        }
        if (ex_mem_buf_.rd == rs2) {
            fu.forward_b = ForwardingUnit::FORWARD_FROM_WB;
        }
    }

    // Forward from WB stage (lower priority) to EX stage
    if (mem_wb_buf_.valid && mem_wb_buf_.reg_write && mem_wb_buf_.rd != 0) {
        if (mem_wb_buf_.rd == rs1 && fu.forward_a == ForwardingUnit::NO_FORWARD) {
            fu.forward_a = ForwardingUnit::FORWARD_FROM_WB;
        }
        if (mem_wb_buf_.rd == rs2 && fu.forward_b == ForwardingUnit::NO_FORWARD) {
            fu.forward_b = ForwardingUnit::FORWARD_FROM_WB;
        }
    }

    return fu;
}
uint64_t RV5SVM::GetForwardedValue(uint8_t reg, ForwardingUnit::ForwardType forward_type) {
    /**
     * Return the appropriate value based on forwarding type:
     * - FORWARD_FROM_MEM: Use result from MEM stage
     * - FORWARD_FROM_WB: Use result from WB stage
     * - NO_FORWARD: Use value from register file
     */
    switch (forward_type) {
        case ForwardingUnit::FORWARD_FROM_MEM:
            return ex_mem_buf_.exec_result;
        case ForwardingUnit::FORWARD_FROM_WB:
            return mem_wb_buf_.mem_to_reg ? mem_wb_buf_.mem_result : mem_wb_buf_.result;
        default:
            // For non-zero registers, read from register file
            // For register x0 (zero register), always return 0
            return (reg != 0) ? registers_.ReadGpr(reg) : 0;
    }
}
void RV5SVM::InsertStall() {
    // Insert a bubble (NOP) in the pipeline
    id_ex_buf_.valid = true;
    id_ex_buf_.is_nop = true;
    id_ex_buf_.instruction = 0;  // Set to NOP encoding
    id_ex_buf_.reg_write = false;
    id_ex_buf_.mem_read = false;
    id_ex_buf_.mem_write = false;
    id_ex_buf_.branch = false;
    stall_cycles++;
}
void RV5SVM::PipelineIF() {
    if (pipeline_stall) {
        return; // Hold PC and don't fetch
    }

    // Handle branch misprediction recovery (for modes 4-5)
    // if (branch_mispredicted_) {
    //     if_id_buf_.instruction = memory_controller_.ReadWord(correct_branch_target_);
    //     if_id_buf_.pc = correct_branch_target_;
    //     if_id_buf_.valid = true;
    //     if_id_buf_.is_nop = false;
    //     program_counter_ = correct_branch_target_ + 4;
    //     branch_mispredicted_ = false;
    //     return;
    // }

    if (pipeline_flush) {
        if_id_buf_.valid = false;
        if_id_buf_.is_nop = false;
        return; // Don't fetch this cycle, PC already updated
    }

    if (program_counter_ < program_size_) {
        uint32_t instruction = memory_controller_.ReadWord(program_counter_);

        if_id_buf_.instruction = instruction;
        if_id_buf_.pc = program_counter_;
        if_id_buf_.valid = true;
        if_id_buf_.is_nop = false;

        // Branch prediction for modes 4-5
        if (globals::pipelined_mode >= 4) {
            uint8_t opcode = instruction & 0b1111111;
            bool is_branch = (opcode == 0b1100011); // Conditional branches
            bool is_jal = (opcode == get_instr_encoding(Instruction::kjal).opcode);
            bool is_jalr = (opcode == get_instr_encoding(Instruction::kjalr).opcode);

            if (is_jal) {
                // JAL: Always taken, calculate target
                int32_t imm = ImmGenerator(instruction);
                program_counter_ = program_counter_ + imm;
            } else if (is_jalr) {
                // JALR: Can't predict target in IF, just increment
                program_counter_ += 4;
            } else if (is_branch) {
                // Conditional branch: use predictor
                bool predict_taken = PredictBranch(program_counter_, instruction);
                if (predict_taken) {
                    // Mode 5 might predict taken
                    int32_t imm = ImmGenerator(instruction);
                    program_counter_ = program_counter_ + imm;
                } else {
                    // Mode 4 always predicts not-taken
                    program_counter_ += 4;
                }
            } else {
                program_counter_ += 4;
            }
        } else {
            // Modes 2-3: Always predict not-taken (PC+4)
            program_counter_ += 4;
        }
    } else {
        if_id_buf_.valid = false;
    }
}
void RV5SVM::PipelineID() {
    if (pipeline_flush) {
        id_ex_buf_.valid = false;
        id_ex_buf_.is_nop = false;
        return;
    }

    // Check for hazards
    if (DetectLoadUseHazard() || (DetectDataHazard() && globals::pipelined_mode == 2)) {
        data_hazards++;
        InsertStall();
        pipeline_stall = true;
        return;
    }

    if (pipeline_stall) {
        pipeline_stall = false;
    }

    if (if_id_buf_.valid && !if_id_buf_.is_nop) {
        id_ex_buf_.instruction = if_id_buf_.instruction;
        id_ex_buf_.pc = if_id_buf_.pc;
        id_ex_buf_.valid = true;
        id_ex_buf_.is_nop = false;

        uint32_t instruction = if_id_buf_.instruction;
        id_ex_buf_.opcode = instruction & 0b1111111;
        id_ex_buf_.rs1 = (instruction >> 15) & 0b11111;
        id_ex_buf_.rs2 = (instruction >> 20) & 0b11111;
        id_ex_buf_.rd = (instruction >> 7) & 0b11111;
        id_ex_buf_.imm = ImmGenerator(instruction);

        control_unit_.SetControlSignals(instruction);
        id_ex_buf_.alu_src = control_unit_.GetAluSrc();
        id_ex_buf_.mem_to_reg = control_unit_.GetMemToReg();
        id_ex_buf_.reg_write = control_unit_.GetRegWrite();
        id_ex_buf_.mem_read = control_unit_.GetMemRead();
        id_ex_buf_.mem_write = control_unit_.GetMemWrite();
        id_ex_buf_.branch = control_unit_.GetBranch();
        id_ex_buf_.alu_op = control_unit_.GetAluOp();

        id_ex_buf_.rs1_value = registers_.ReadGpr(id_ex_buf_.rs1);
        id_ex_buf_.rs2_value = registers_.ReadGpr(id_ex_buf_.rs2);
    } else {
        id_ex_buf_.valid = false;
    }
    if_id_buf_.valid = false;
}
void RV5SVM::PipelineEX() {
    // NO flush check - branch must complete through pipeline

    if (id_ex_buf_.valid && !id_ex_buf_.is_nop) {
        ex_mem_buf_.instruction = id_ex_buf_.instruction;
        ex_mem_buf_.pc = id_ex_buf_.pc;
        ex_mem_buf_.rd = id_ex_buf_.rd;
        ex_mem_buf_.valid = true;
        ex_mem_buf_.is_nop = false;

        uint32_t instruction = id_ex_buf_.instruction;
        uint8_t opcode = id_ex_buf_.opcode;
        uint8_t funct3 = (instruction >> 12) & 0b111;

        // Copy control signals
        ex_mem_buf_.mem_read = id_ex_buf_.mem_read;
        ex_mem_buf_.mem_write = id_ex_buf_.mem_write;
        ex_mem_buf_.reg_write = id_ex_buf_.reg_write;
        ex_mem_buf_.mem_to_reg = id_ex_buf_.mem_to_reg;
        ex_mem_buf_.branch_taken = false;

        // Detect forwarding opportunities
        ForwardingUnit fu = DetectForwarding();
        uint64_t alu_input1 = (id_ex_buf_.rs1 != 0) ? GetForwardedValue(id_ex_buf_.rs1, fu.forward_a) : 0;
        uint64_t alu_input2 = (id_ex_buf_.rs2 != 0) ? GetForwardedValue(id_ex_buf_.rs2, fu.forward_b) : 0;

        ex_mem_buf_.rs2_value = alu_input2;

        if (id_ex_buf_.alu_src) {
            alu_input2 = static_cast<uint64_t>(static_cast<int64_t>(id_ex_buf_.imm));
        }

        // Execute ALU operation
        alu::AluOp aluOperation = control_unit_.GetAluSignal(instruction, id_ex_buf_.alu_op);
        bool overflow = false;
        int64_t exec_result;
        std::tie(exec_result, overflow) = alu_.execute(aluOperation, alu_input1, alu_input2);
        ex_mem_buf_.exec_result = static_cast<uint64_t>(exec_result);
        ex_mem_forward_data = ex_mem_buf_.exec_result;

        // Handle branches
        if (id_ex_buf_.branch) {
            control_hazards++;
            bool actually_taken = false;
            uint64_t actual_target = 0;

            // Determine if branch should be taken
            if (opcode == get_instr_encoding(Instruction::kjalr).opcode) {
                actually_taken = true;
                actual_target = exec_result & ~1ULL;
            } else if (opcode == get_instr_encoding(Instruction::kjal).opcode) {
                actually_taken = true;
                actual_target = id_ex_buf_.pc + id_ex_buf_.imm;
            } else if (opcode == 0b1100011) { // Conditional branches
                bool take_branch = false;
                switch (funct3) {
                    case 0b000: take_branch = (alu_input1 == alu_input2); break; // BEQ
                    case 0b001: take_branch = (alu_input1 != alu_input2); break; // BNE
                    case 0b100: take_branch = (static_cast<int64_t>(alu_input1) < static_cast<int64_t>(alu_input2)); break; // BLT
                    case 0b101: take_branch = (static_cast<int64_t>(alu_input1) >= static_cast<int64_t>(alu_input2)); break; // BGE
                    case 0b110: take_branch = (alu_input1 < alu_input2); break; // BLTU
                    case 0b111: take_branch = (alu_input1 >= alu_input2); break; // BGEU
                }
                actually_taken = take_branch;
                actual_target = take_branch ? (id_ex_buf_.pc + id_ex_buf_.imm) : (id_ex_buf_.pc + 4);
            }

            ex_mem_buf_.branch_taken = actually_taken;
            ex_mem_buf_.branch_target = actual_target;

            // Mode-specific branch handling
            if (globals::pipelined_mode >= 4) {
                // Modes 4-5: Predict and flush only on misprediction
                bool was_predicted_taken = PredictBranch(id_ex_buf_.pc, id_ex_buf_.instruction);
                bool mispredicted = (was_predicted_taken != actually_taken);

                if (mispredicted) {
                    // Misprediction: need to correct and flush
                    // branch_mispredicted_ = true;
                    correct_branch_target_ = actual_target;
                    pipeline_flush = true;
                    flush_cycles += 2; // Flush IF and ID stages

                    // Update predictor for mode 5
                    if (globals::pipelined_mode == 5) {
                        UpdateBranchPredictor(id_ex_buf_.pc, actually_taken);
                    }
                }
                // If correctly predicted, no flush needed - pipeline continues normally
            } else {
                // Modes 2-3: ALWAYS flush when branch is resolved (no prediction)
                // Update PC immediately so next IF can fetch correct instruction
                program_counter_ = actual_target;
                pipeline_flush = true;
                flush_cycles += 2; // Flush IF and ID stages (instructions already in those stages)
            }
        }
    } else {
        ex_mem_buf_.valid = false;
    }
    id_ex_buf_.valid = false;
}
void RV5SVM::PipelineMEM() {
    // NO flush check - instructions must complete

    if (ex_mem_buf_.valid && !ex_mem_buf_.is_nop) {
        mem_wb_buf_.instruction = ex_mem_buf_.instruction;
        mem_wb_buf_.pc = ex_mem_buf_.pc;
        mem_wb_buf_.rd = ex_mem_buf_.rd;
        mem_wb_buf_.valid = true;
        mem_wb_buf_.is_nop = false;
        mem_wb_buf_.reg_write = ex_mem_buf_.reg_write;
        mem_wb_buf_.mem_to_reg = ex_mem_buf_.mem_to_reg;
        mem_wb_buf_.result = ex_mem_buf_.exec_result;

        uint32_t instruction = ex_mem_buf_.instruction;
        uint8_t funct3 = (instruction >> 12) & 0b111;

        // Handle memory reads
        if (ex_mem_buf_.mem_read) {
            switch (funct3) {
                case 0b000: mem_wb_buf_.mem_result = static_cast<int8_t>(memory_controller_.ReadByte(ex_mem_buf_.exec_result)); break;
                case 0b001: mem_wb_buf_.mem_result = static_cast<int16_t>(memory_controller_.ReadHalfWord(ex_mem_buf_.exec_result)); break;
                case 0b010: mem_wb_buf_.mem_result = static_cast<int32_t>(memory_controller_.ReadWord(ex_mem_buf_.exec_result)); break;
                case 0b011: mem_wb_buf_.mem_result = memory_controller_.ReadDoubleWord(ex_mem_buf_.exec_result); break;
                case 0b100: mem_wb_buf_.mem_result = static_cast<uint8_t>(memory_controller_.ReadByte(ex_mem_buf_.exec_result)); break;
                case 0b101: mem_wb_buf_.mem_result = static_cast<uint16_t>(memory_controller_.ReadHalfWord(ex_mem_buf_.exec_result)); break;
                case 0b110: mem_wb_buf_.mem_result = static_cast<uint32_t>(memory_controller_.ReadWord(ex_mem_buf_.exec_result)); break;
            }
        }

        // Handle memory writes
        if (ex_mem_buf_.mem_write) {
            switch (funct3) {
                case 0b000: memory_controller_.WriteByte(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFF); break;
                case 0b001: memory_controller_.WriteHalfWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFFFF); break;
                case 0b010: memory_controller_.WriteWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value & 0xFFFFFFFF); break;
                case 0b011: memory_controller_.WriteDoubleWord(ex_mem_buf_.exec_result, ex_mem_buf_.rs2_value); break;
            }
        }

        mem_wb_forward_data = mem_wb_buf_.mem_to_reg ? mem_wb_buf_.mem_result : mem_wb_buf_.result;
    } else {
        mem_wb_buf_.valid = false;
    }
    ex_mem_buf_.valid = false;
}
void RV5SVM::PipelineWB() {
    if (mem_wb_buf_.valid && !mem_wb_buf_.is_nop) {
        uint32_t instruction = mem_wb_buf_.instruction;
        uint8_t opcode = instruction & 0b1111111;
        uint8_t rd = mem_wb_buf_.rd;

        if (mem_wb_buf_.reg_write && rd != 0) {
            uint64_t write_value;
            if (mem_wb_buf_.mem_to_reg) {
                write_value = mem_wb_buf_.mem_result;
            } else {
                write_value = mem_wb_buf_.result;
            }

            // Handle special instructions
            if (opcode == get_instr_encoding(Instruction::kjal).opcode ||
                opcode == get_instr_encoding(Instruction::kjalr).opcode) {
                write_value = mem_wb_buf_.pc + 4;
            } else if (opcode == get_instr_encoding(Instruction::klui).opcode) {
                int32_t imm = ImmGenerator(instruction);
                write_value = static_cast<uint64_t>(static_cast<int64_t>(imm << 12));
            } else if (opcode == get_instr_encoding(Instruction::kauipc).opcode) {
                int32_t imm = ImmGenerator(instruction);
                write_value = mem_wb_buf_.pc + (static_cast<uint64_t>(imm) << 12);
            }

            registers_.WriteGpr(rd, write_value);
        }
        instructions_retired_++;
    }

    mem_wb_buf_.valid = false;
}
void RV5SVM::ExecutePipelineCycle() {
    // Execute all stages - flush flag is checked within stages
    PipelineWB();
    PipelineMEM();
    PipelineEX();    // May set pipeline_flush = true
    PipelineID();    // Will see flush and clear itself
    PipelineIF();    // Will see flush and clear itself

    // Clear flush flag AFTER all stages have seen it
    // This ensures flush affects IF and ID in THIS cycle
    // Then next cycle they can fetch/decode new instructions
    if (pipeline_flush) {
        pipeline_flush = false;
    }

    cycle_s_++;
    cycle_count++;
    PrintPipelineState();
}
std::string RV5SVM::GetInstructionName(uint32_t instruction) {
    if (instruction == 0) return "NOP";
    uint8_t opcode = instruction & 0b1111111;
    uint8_t funct3 = (instruction >> 12) & 0b111;
    uint8_t funct7 = (instruction >> 25) & 0b1111111;
    // R-type
    if (opcode == 0b0110011) {
        if (funct3 == 0b000 && funct7 == 0b0000000) return "ADD";
        if (funct3 == 0b000 && funct7 == 0b0100000) return "SUB";
        if (funct3 == 0b001) return "SLL";
        if (funct3 == 0b010) return "SLT";
        if (funct3 == 0b011) return "SLTU";
        if (funct3 == 0b100) return "XOR";
        if (funct3 == 0b101 && funct7 == 0b0000000) return "SRL";
        if (funct3 == 0b101 && funct7 == 0b0100000) return "SRA";
        if (funct3 == 0b110) return "OR";
        if (funct3 == 0b111) return "AND";
    }
    // I-type
    else if (opcode == 0b0010011) {
        if (funct3 == 0b000) return "ADDI";
        if (funct3 == 0b010) return "SLTI";
        if (funct3 == 0b011) return "SLTIU";
        if (funct3 == 0b100) return "XORI";
        if (funct3 == 0b110) return "ORI";
        if (funct3 == 0b111) return "ANDI";
        if (funct3 == 0b001) return "SLLI";
        if (funct3 == 0b101) return "SRLI/SRAI";
    }
    // Load
    else if (opcode == 0b0000011) {
        if (funct3 == 0b000) return "LB";
        if (funct3 == 0b001) return "LH";
        if (funct3 == 0b010) return "LW";
        if (funct3 == 0b011) return "LD";
        if (funct3 == 0b100) return "LBU";
        if (funct3 == 0b101) return "LHU";
        if (funct3 == 0b110) return "LWU";
    }
    // Store
    else if (opcode == 0b0100011) {
        if (funct3 == 0b000) return "SB";
        if (funct3 == 0b001) return "SH";
        if (funct3 == 0b010) return "SW";
        if (funct3 == 0b011) return "SD";
    }
    // Branch
    else if (opcode == 0b1100011) {
        if (funct3 == 0b000) return "BEQ";
        if (funct3 == 0b001) return "BNE";
        if (funct3 == 0b100) return "BLT";
        if (funct3 == 0b101) return "BGE";
        if (funct3 == 0b110) return "BLTU";
        if (funct3 == 0b111) return "BGEU";
    }
    else if (opcode == 0b1101111) return "JAL";
    else if (opcode == 0b1100111) return "JALR";
    else if (opcode == 0b0110111) return "LUI";
    else if (opcode == 0b0010111) return "AUIPC";
    return "UNKNOWN";
}
void RV5SVM::PrintPipelineState() {
    std::lock_guard<std::mutex> lock(print_mutex_);
    // Save/restore state around prints (or reset at end)
    auto flags = std::cout.flags();  // Save current flags
    auto precision = std::cout.precision();
    auto fill = std::cout.fill();
    // ANSI color codes
    const std::string RESET = "\033[0m";
    const std::string BOLD = "\033[1m";
    const std::string RED = "\033[31m";
    const std::string GREEN = "\033[32m";
    const std::string YELLOW = "\033[33m";
    const std::string BLUE = "\033[34m";
    const std::string MAGENTA = "\033[35m";
    const std::string CYAN = "\033[36m";
    const std::string WHITE = "\033[37m";
    const std::string BRIGHT_BLACK = "\033[90m";
    const std::string BRIGHT_RED = "\033[91m";
    const std::string BRIGHT_GREEN = "\033[92m";
    const std::string BRIGHT_YELLOW = "\033[93m";
    const std::string BRIGHT_BLUE = "\033[94m";
    const std::string BRIGHT_MAGENTA = "\033[95m";
    const std::string BRIGHT_CYAN = "\033[96m";
    std::cout << "\n" << BOLD << CYAN << "╔════════════════════════════════════════════════════════════════════════════╗" << RESET << std::endl;
    std::cout << BOLD << CYAN << "║ PIPELINE STATE - Cycle " << std::setw(5) << cycle_count;
    std::cout << " (Mode " << globals::pipelined_mode << ") ║" << RESET << std::endl;
    std::cout << BOLD << CYAN << "╠════════════════════════════════════════════════════════════════════════════╣" << RESET << std::endl;
    // IF Stage - Blue
    std::cout << BOLD << BLUE << "║ IF │ " << RESET;
    if (if_id_buf_.valid && !if_id_buf_.is_nop) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << if_id_buf_.pc
                  << " │ " << std::setw(10) << std::setfill(' ') << std::left
                  << BRIGHT_BLUE << GetInstructionName(if_id_buf_.instruction) << RESET
                  << " │ 0x" << std::hex << std::setw(8) << std::setfill('0') << if_id_buf_.instruction;
    } else if (if_id_buf_.valid && if_id_buf_.is_nop) {
        std::cout << BRIGHT_YELLOW << "NOP" << RESET << " │ 0x" << std::hex << std::setw(8) << std::setfill('0') << 0x00000000;
    } else {
        std::cout << BRIGHT_BLACK << "EMPTY " << RESET;
    }
    std::cout << std::dec << std::setfill(' ') << BOLD << CYAN << " ║" << RESET << std::endl;
    // ID Stage - Green
    std::cout << BOLD << GREEN << "║ ID │ " << RESET;
    if (id_ex_buf_.valid && !id_ex_buf_.is_nop) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << id_ex_buf_.pc
                  << " │ " << std::setw(10) << std::setfill(' ') << std::left
                  << BRIGHT_GREEN << GetInstructionName(id_ex_buf_.instruction) << RESET
                  << " │ rs1:x" << std::dec << std::setw(2) << (int)id_ex_buf_.rs1
                  << " rs2:x" << std::setw(2) << (int)id_ex_buf_.rs2
                  << " rd:x" << std::setw(2) << (int)id_ex_buf_.rd << " ";
    } else if (id_ex_buf_.valid && id_ex_buf_.is_nop) {
        std::cout << BRIGHT_YELLOW << "NOP" << RESET
                  << " │ rs1:x00 rs2:x00 rd:x00 ";
    } else {
        std::cout << BRIGHT_BLACK << "EMPTY " << RESET;
    }
    std::cout << BOLD << CYAN << " ║" << RESET << std::endl;
    // EX Stage with forwarding info - Yellow
    std::cout << BOLD << YELLOW << "║ EX │ " << RESET;
    if (ex_mem_buf_.valid && !ex_mem_buf_.is_nop) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << ex_mem_buf_.pc
                  << " │ " << std::setw(10) << std::setfill(' ') << std::left
                  << BRIGHT_YELLOW << GetInstructionName(ex_mem_buf_.instruction) << RESET
                  << " │ Result: 0x" << std::hex << std::setw(8) << std::setfill('0')
                  << ex_mem_buf_.exec_result;
        if (globals::pipelined_mode == 3) {
            ForwardingUnit fu = DetectForwarding();
            if (fu.forward_a != ForwardingUnit::NO_FORWARD ||
                fu.forward_b != ForwardingUnit::NO_FORWARD) {
                std::cout << BRIGHT_MAGENTA << " [FWD]" << RESET;
            }
        }
    } else if (ex_mem_buf_.valid && ex_mem_buf_.is_nop) {
        std::cout << BRIGHT_YELLOW << "NOP" << RESET
                  << " │ Result: 0x" << std::hex << std::setw(8) << std::setfill('0') << 0x00000000;
    } else {
        std::cout << BRIGHT_BLACK << "EMPTY " << RESET;
    }
    std::cout << std::dec << std::setfill(' ') << BOLD << CYAN << " ║" << RESET << std::endl;
    // MEM Stage - Magenta
    std::cout << BOLD << MAGENTA << "║ MEM │ " << RESET;
    if (mem_wb_buf_.valid && !mem_wb_buf_.is_nop) {
        std::cout << "PC: 0x" << std::hex << std::setw(8) << std::setfill('0') << mem_wb_buf_.pc
                  << " │ " << std::setw(10) << std::setfill(' ') << std::left
                  << BRIGHT_MAGENTA << GetInstructionName(mem_wb_buf_.instruction) << RESET;
        if (mem_wb_buf_.mem_to_reg) {
            std::cout << " │ MemData: 0x" << std::hex << std::setw(8) << std::setfill('0')
                      << mem_wb_buf_.mem_result;
        } else {
            std::cout << " │ AluData: 0x" << std::hex << std::setw(8) << std::setfill('0')
                      << mem_wb_buf_.result;
        }
    } else if (mem_wb_buf_.valid && mem_wb_buf_.is_nop) {
        std::cout << BRIGHT_YELLOW << "NOP" << RESET
                  << " │ AluData: 0x" << std::hex << std::setw(8) << std::setfill('0') << 0x00000000;
    } else {
        std::cout << BRIGHT_BLACK << "EMPTY " << RESET;
    }
    std::cout << std::dec << std::setfill(' ') << BOLD << CYAN << " ║" << RESET << std::endl;
    // WB Stage - Cyan
    std::cout << BOLD << CYAN << "║ WB │ " << RESET;
    std::cout << BRIGHT_CYAN << "Instruction completed and retired " << RESET << BOLD << CYAN << "║" << RESET << std::endl;
    std::cout << BOLD << CYAN << "╠════════════════════════════════════════════════════════════════════════════╣" << RESET << std::endl;
    // Statistics
    std::cout << BOLD << CYAN << "║ " << RESET << "Stats: Instructions Retired: " << BRIGHT_GREEN << std::setw(5) << instructions_retired_ << RESET
              << BOLD << CYAN << " │ " << RESET << "Stalls: " << BRIGHT_YELLOW << std::setw(4) << stall_cycles << RESET
              << BOLD << CYAN << " │ " << RESET << "Flushes: " << BRIGHT_RED << std::setw(4) << flush_cycles << RESET
              << BOLD << CYAN << " │ " << RESET << "CPI: " << BRIGHT_CYAN << std::fixed << std::setprecision(2)
              << (instructions_retired_ > 0 ? (double)cycle_count / instructions_retired_ : 0.0) << RESET
              << BOLD << CYAN << " ║" << RESET << std::endl;
    if (globals::pipelined_mode >= 2) {
        std::cout << BOLD << CYAN << "║ " << RESET << "Hazards: Data: " << BRIGHT_YELLOW << std::setw(4) << data_hazards << RESET
                  << BOLD << CYAN << " │ " << RESET << "Control: " << BRIGHT_RED << std::setw(4) << control_hazards << RESET;
        if (pipeline_stall) {
            std::cout << BOLD << CYAN << " │ " << RESET << "STATUS: " << BRIGHT_RED << "STALLED " << RESET << BOLD << CYAN << "║" << RESET << std::endl;
        } else if (pipeline_flush) {
            std::cout << BOLD << CYAN << " │ " << RESET << "STATUS: " << BRIGHT_MAGENTA << "FLUSHING " << RESET << BOLD << CYAN << "║" << RESET << std::endl;
        } else {
            std::cout << BOLD << CYAN << " ║" << RESET << std::endl;
        }
    }
    std::cout << BOLD << CYAN << "╚════════════════════════════════════════════════════════════════════════════╝" << RESET << std::endl;
    // At very end:
    std::cout.flags(flags);  // Restore flags (base, etc.)
    std::cout.precision(precision);
    std::cout.fill(fill);
    std::cout << std::dec << std::setfill(' ') << std::resetiosflags(std::ios::fixed | std::ios::scientific | std::ios::hex) << std::nouppercase;

}
void RV5SVM::Run() {
    ClearStop();
    uint64_t instruction_executed = 0;
    // Clear pipeline buffers
    if_id_buf_ = {};
    id_ex_buf_ = {};
    ex_mem_buf_ = {};
    mem_wb_buf_ = {};
    pipeline_stall = false;
    pipeline_flush = false;

    //Initialize branch predictor for mode 5
    if (globals::pipelined_mode == 5) {
        branch_predictor_.clear();
    }

    std::cout << "\n════════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Starting Pipelined Execution - Mode " << globals::pipelined_mode << std::endl;
    switch (globals::pipelined_mode) {
        case 0:
            std::cout << "Mode 0: Single-cycle (no pipelining)" << std::endl;
            break;
        case 1:
            std::cout << "Mode 1: Basic pipelining (no hazard detection)" << std::endl;
            break;
        case 2:
            std::cout << "Mode 2: Pipelining with hazard detection (no forwarding)" << std::endl;
            break;
        case 3:
            std::cout << "Mode 3: Pipelining with hazard detection and forwarding" << std::endl;
            break;
        case 4:
                std::cout << "Mode 4: Pipelining with hazard detection, forwarding, and static branch prediction" << std::endl;
                break;
        case 5:
            std::cout << "Mode 5: Pipelining with hazard detection, forwarding, and dynamic 1-bit branch prediction" << std::endl;
            break;
    }
    std::cout << "════════════════════════════════════════════════════════════════\n" << std::endl;
    while (!stop_requested_) {
        if (instruction_executed > vm_config::config.getInstructionExecutionLimit())
            break;
        ExecutePipelineCycle();
        // Check if pipeline is completely empty
        if (!if_id_buf_.valid && !id_ex_buf_.valid &&
            !ex_mem_buf_.valid && !mem_wb_buf_.valid &&
            program_counter_ >= program_size_) {
            break;
        }
        instruction_executed++;
        // Add small delay for visualization
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "\n════════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Execution Complete" << std::endl;
    std::cout << "════════════════════════════════════════════════════════════════" << std::endl;
    std::cout << "Total Cycles: " << cycle_count << std::endl;
    std::cout << "Instructions Retired: " << instructions_retired_ << std::endl;
    std::cout << "Stall Cycles: " << stall_cycles << std::endl;
    std::cout << "Flush Cycles: " << flush_cycles << std::endl;
    std::cout << "Data Hazards Detected: " << data_hazards << std::endl;
    std::cout << "Control Hazards: " << control_hazards << std::endl;
    std::cout << "CPI: " << std::fixed << std::setprecision(3)
              << (instructions_retired_ > 0 ? (double)cycle_count / instructions_retired_ : 0.0) << std::endl;
    std::cout << "════════════════════════════════════════════════════════════════\n" << std::endl;
    if (program_counter_ >= program_size_) {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}
void RV5SVM::DebugRun() {
    Run();
}
void RV5SVM::Step() {
    if (program_counter_ < program_size_ ||
        if_id_buf_.valid || id_ex_buf_.valid || ex_mem_buf_.valid || mem_wb_buf_.valid) {
        ExecutePipelineCycle();

        {
            std::lock_guard<std::mutex> lock(print_mutex_);
            std::cout << std::dec << std::setfill(' ') << std::resetiosflags(std::ios::fixed | std::ios::scientific | std::ios::hex) << std::nouppercase;
        }

        if (program_counter_ < program_size_ ||
            if_id_buf_.valid || id_ex_buf_.valid || ex_mem_buf_.valid || mem_wb_buf_.valid) {
            std::cout << "VM_STEP_COMPLETED" << std::endl;
            output_status_ = "VM_STEP_COMPLETED";
        } else {
            std::cout << "VM_LAST_INSTRUCTION_STEPPED" << std::endl;
            output_status_ = "VM_LAST_INSTRUCTION_STEPPED";
        }
    } else {
        std::cout << "VM_PROGRAM_END" << std::endl;
        output_status_ = "VM_PROGRAM_END";
    }
    DumpRegisters(globals::registers_dump_file_path, registers_);
    DumpState(globals::vm_state_dump_file_path);
}
void RV5SVM::Undo() {
    std::cout << "VM_UNDO_NOT_SUPPORTED" << std::endl;
    output_status_ = "VM_UNDO_NOT_SUPPORTED";
}
void RV5SVM::Redo() {
    std::cout << "VM_REDO_NOT_SUPPORTED" << std::endl;
    output_status_ = "VM_REDO_NOT_SUPPORTED";
}
void RV5SVM::Reset() {
    program_counter_ = 0;
    instructions_retired_ = 0;
    cycle_s_ = 0;
    cycle_count = 0;
    stall_cycles = 0;
    flush_cycles = 0;
    data_hazards = 0;
    control_hazards = 0;
    registers_.Reset();
    memory_controller_.Reset();
    control_unit_.Reset();
    if_id_buf_ = {};
    id_ex_buf_ = {};
    ex_mem_buf_ = {};
    mem_wb_buf_ = {};
    pipeline_stall = false;
    pipeline_flush = false;
}
void RV5SVM::FlushPipeline() {
    if_id_buf_.valid = false;
    id_ex_buf_.valid = false;
    ex_mem_buf_.valid = false;
    pipeline_flush = false;
}

bool RV5SVM::PredictBranch(uint64_t pc, uint32_t instruction) {
    uint8_t opcode = instruction & 0b1111111;

    // For unconditional jumps, always predict taken
    if (opcode == get_instr_encoding(Instruction::kjal).opcode ||
        opcode == get_instr_encoding(Instruction::kjalr).opcode) {
        return true;
    }

    // Only for conditional branches (0b1100011)
    if (opcode != 0b1100011) {
        return false;
    }

    // Mode 4: Static branch prediction (always not taken for conditional branches)
    if (globals::pipelined_mode == 4) {
        return false; // Always predict NOT TAKEN
    }

    // Mode 5: Dynamic 1-bit branch prediction
    if (globals::pipelined_mode == 5) {
        if (branch_predictor_.find(pc) == branch_predictor_.end()) {
            branch_predictor_[pc] = false; // Initialize to not taken
        }
        return branch_predictor_[pc];
    }

    // Modes 2-3: No prediction (doesn't matter, always flush)
    return false;
}

void RV5SVM::UpdateBranchPredictor(uint64_t pc, bool taken) {
    if (globals::pipelined_mode == 5) {
        branch_predictor_[pc] = taken;
    }
}
