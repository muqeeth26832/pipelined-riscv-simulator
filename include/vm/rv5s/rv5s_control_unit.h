/**
 * @file rv5s_control_unit.h
 * @brief RV5S Control Unit for Pipelined Implementation
 * @author Your Name
 */

#ifndef RV5S_CONTROL_UNIT_H
#define RV5S_CONTROL_UNIT_H

#include "../control_unit_base.h"

class RV5SControlUnit : public ControlUnit {
public:
    void SetControlSignals(uint32_t instruction) override;

    alu::AluOp GetAluSignal(uint32_t instruction, bool ALUOp) override;

};

#endif // RV5S_CONTROL_UNIT_H