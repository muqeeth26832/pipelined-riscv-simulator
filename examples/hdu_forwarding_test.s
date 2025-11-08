# Test program for Hazard Detection Unit and Forwarding
# This program creates data hazards that should be resolved by HDU and forwarding

.global _start

_start:
    # Load immediate to x1
    li x1, 100         # x1 = 100
    
    # Load-use hazard: lw followed immediately by use
    lw x2, 0(x1)       # Load from address 100 to x2
    add x3, x2, x2     # Use x2 immediately - load-use hazard!
    
    # RAW hazard: instruction using result of previous ALU operation
    addi x4, x3, 1     # x4 = x3 + 1
    sub x5, x4, x1     # x5 = x4 - x1, should cause data hazard
    
    # Forwarding test: multiple dependent operations
    addi x6, x5, 5     # x6 = x5 + 5
    mul x7, x6, x4     # x7 = x6 * x4 - test forwarding from multiple sources
    
    # Create another dependency chain
    and x8, x7, x3     # x8 = x7 & x3
    or x9, x8, x2      # x9 = x8 | x2
    xor x10, x9, x1    # x10 = x9 ^ x1
    
    # End program
    li x0, 0           # No operation for x0 (should not change anything)

end:
    j end