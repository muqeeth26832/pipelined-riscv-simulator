/**
 * @file rv5s_control_unit.cpp
 * @brief RV5S Control Unit implementation for Pipelined Implementation
 * @author Your Name
 */

#include "vm/rv5s/rv5s_control_unit.h"
#include "vm/alu.h"

#include <cstdint>

#include "common/instructions.h"
using instruction_set::Instruction;
using instruction_set::get_instr_encoding;

void RV5SControlUnit::SetControlSignals(uint32_t instruction) {
    uint8_t opcode = instruction & 0b1111111;

    // Reset all control signals
    alu_src_ = mem_to_reg_ = reg_write_ = mem_read_ = mem_write_ = branch_ = false;
    alu_op_ = false;

    switch (opcode) {
    case 0b0110011: /* R-type (kAdd, kSub, kAnd, kOr, kXor, kSll, kSrl, etc.) */ {
        reg_write_ = true;
        alu_op_ = true;
        break;
    }
    case 0b0000011: { // Load instructions (LB, LH, LW, LD)
        alu_src_ = true;
        mem_to_reg_ = true;
        reg_write_ = true;
        mem_read_ = true;
        break;
    }
    case 0b0100011: { // Store instructions (SB, SH, SW, SD)
        alu_src_ = true;
        alu_op_ = true;
        mem_write_ = true;
        break;
    }
    case 0b1100011: { // branch instructions (BEQ, BNE, BLT, BGE)
        alu_op_ = true;
        branch_ = true;
        break;
    }
    case 0b0010011: { // I-type alu instructions (ADDI, ANDI, ORI, XORI, SLTI, SLLI, SRLI)
        alu_src_ = true;
        reg_write_ = true;
        alu_op_ = true;
        break;
    }
    case 0b0110111: { // LUI (Load Upper Immediate)
        alu_src_ = true;
        reg_write_ = true;
        alu_op_ = true; // alu will add immediate to zero
        break;
    }
    case 0b0010111: { // AUIPC (Add Upper Immediate to PC)
        alu_src_ = true;
        reg_write_ = true;
        alu_op_ = true; // alu will add immediate to PC
        break;
    }
    case 0b1101111: { // JAL (Jump and Link)
        reg_write_ = true;
        branch_ = true;
        break;
    }
    case 0b1100111: { // JALR (Jump and Link Register)
        alu_src_ = true;
        reg_write_ = true;
        branch_ = true;
        break;
    }
    case 0b0000001: { // kMul
        reg_write_ = true;
        alu_op_ = true;
        break;
    }

    // F extension + D extension
    case 0b0000111: { // F-Type Load instructions (FLW, FLD)
        alu_src_ = true;
        mem_to_reg_ = true;
        reg_write_ = true;
        mem_read_ = true;
        break;
    }
    case 0b0100111: { // F-Type Store instructions (FSW, FSD)
        alu_src_ = true;
        alu_op_ = true;
        mem_write_ = true;
        break;
    }
    case 0b1010011: { // F-Type R-type instructions (FADD, FSUB, FMUL, FDIV, etc.)
        reg_write_ = true;
        alu_op_ = true;
        break;
    }

    default:
        break;
    }
}

alu::AluOp RV5SControlUnit::GetAluSignal(uint32_t instruction, bool ALUOp) {
    (void)ALUOp; // Suppress unused variable warning
    uint8_t opcode = instruction & 0b1111111; 
    uint8_t funct3 = (instruction >> 12) & 0b111;
    uint8_t funct7 = (instruction >> 25) & 0b1111111;
    uint8_t funct5 = (instruction >> 20) & 0b11111;
    uint8_t funct2 = (instruction >> 25) & 0b11;

    switch (opcode) {
    case 0b0110011: { // R-Type
        switch (funct3) {
        case 0b000: { // kAdd, kSub, kMul
            switch (funct7) {
            case 0x0000000: { // kAdd
                return alu::AluOp::kAdd;
            }
            case 0b0100000: { // kSub
                return alu::AluOp::kSub;
            }
            case 0b0000001: { // kMul
                return alu::AluOp::kMul;
            }
            }
            break;
        }
        case 0b001: { // kSll, kMulh
            switch (funct7) {
            case 0b0000000: { // kSll
                return alu::AluOp::kSll;
            }
            case 0b0000001: { // kMulh
                return alu::AluOp::kMulh;
            }
            }
            break;
        }
        case 0b010: { // kSlt, kMulhsu
            switch (funct7) {
            case 0b0000000: { // kSlt
                return alu::AluOp::kSlt;
            }
            case 0b0000001: { // kMulhsu
                return alu::AluOp::kMulhsu;
            }
            }
            break;
        }
        case 0b011: { // kSltu, kMulhu
            switch (funct7) {
            case 0b0000000: { // kSltu
                return alu::AluOp::kSltu;
            }
            case 0b0000001: { // kMulhu
                return alu::AluOp::kMulhu;
            }
            }
            break;
        }
        case 0b100: { // kXor, kDiv
            switch (funct7) {
            case 0b0000000: { // kXor
                return alu::AluOp::kXor;
            }
            case 0b0000001: { // kDiv
                return alu::AluOp::kDiv;
            }
            }
            break;
        }
        case 0b101: { // kSrl, kSra, kDivu
            switch (funct7) {
            case 0b0000000: { // kSrl
                return alu::AluOp::kSrl;
            }
            case 0b0100000: { // kSra
                return alu::AluOp::kSra;
            }
            case 0b0000001: { // kDivu
                return alu::AluOp::kDivu;
            }
            }
            break;
        }
        case 0b110: { // kOr, kRem
            switch (funct7) {
            case 0b0000000: { // kOr
                return alu::AluOp::kOr;
            }
            case 0b0000001: { // kRem
                return alu::AluOp::kRem;
            }
            }
            break;
        }
        case 0b111: { // kAnd, kRemu
            switch (funct7) {
            case 0b0000000: { // kAnd
                return alu::AluOp::kAnd;
            }
            case 0b0000001: { // kRemu
                return alu::AluOp::kRemu;
            }
            }
            break;
        }
        }
        break;
    }
    case 0b0010011: { // I-Type
        switch (funct3) {
        case 0b000: { // ADDI
            return alu::AluOp::kAdd;
        }
        case 0b001: { // SLLI
            return alu::AluOp::kSll;
        }
        case 0b010: { // SLTI
            return alu::AluOp::kSlt;
        }
        case 0b011: { // SLTIU
            return alu::AluOp::kSltu;
        }
        case 0b100: { // XORI
            return alu::AluOp::kXor;
        }
        case 0b101: { // SRLI & SRAI
            switch (funct7) {
            case 0b0000000: { // SRLI
                return alu::AluOp::kSrl;
            }
            case 0b0100000: { // SRAI
                return alu::AluOp::kSra;
            }
            }
            break;
        }
        case 0b110: { // ORI
            return alu::AluOp::kOr;
        }
        case 0b111: { // ANDI
            return alu::AluOp::kAnd;
        }
        }
        break;
    }
    case 0b1100011: { // B-Type
        switch (funct3) {
        case 0b000: { // BEQ
            return alu::AluOp::kSub;
        }
        case 0b001: { // BNE
            return alu::AluOp::kSub;
        }
        case 0b100: { // BLT
            return alu::AluOp::kSlt;
        }
        case 0b101: { // BGE
            return alu::AluOp::kSlt;
        }
        case 0b110: { // BLTU
            return alu::AluOp::kSltu;
        }
        case 0b111: { // BGEU
            return alu::AluOp::kSltu;
        }
        }
        break;
    }
    case 0b0000011: { // Load
        return alu::AluOp::kAdd;
    }
    case 0b0100011: { // Store
        return alu::AluOp::kAdd;
    }
    case 0b1100111: { // JALR
        return alu::AluOp::kAdd;
    }
    case 0b1101111: { // JAL
        return alu::AluOp::kAdd;
    }
    case 0b0110111: { // LUI
        return alu::AluOp::kAdd;
    }
    case 0b0010111: { // AUIPC
        return alu::AluOp::kAdd;
    }
    case 0b0000000: { // FENCE
        return alu::AluOp::kNone;
    }
    case 0b1110011: { // SYSTEM
        switch (funct3) {
        case 0b000: // ECALL
            return alu::AluOp::kNone;
        case 0b001: // CSRRW
            return alu::AluOp::kNone;
        default:
            break;
        }
        break;
    }
    case 0b0011011: { // R4-Type
        switch (funct3) {
            case 0b000: { // ADDIW
                return alu::AluOp::kAddw;
            }
            case 0b001: { // SLLIW
                return alu::AluOp::kSllw;
            }
            case 0b101: { // SRLIW & SRAIW
                switch (funct7) {
                case 0b0000000: { // SRLIW
                    return alu::AluOp::kSrlw;
                }
                case 0b0100000: { // SRAIW
                    return alu::AluOp::kSraw;
                }
                }
            }
        }
    }

    default:
        break;
    }
    
    // Default return
    return alu::AluOp::kAdd; // Default ALU operation
}