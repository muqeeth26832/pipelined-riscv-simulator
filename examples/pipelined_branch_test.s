# Comprehensive test program for pipelined execution with branches
# Tests branch prediction, hazard detection, and forwarding in pipelined mode

.global _start

_start:
    # Initialize registers
    li x1, 10          # loop counter
    li x2, 0           # running sum 
    li x3, 0           # temp register
    li x4, 20          # comparison value
    li x5, 1           # increment

loop_start:
    # Check if we should continue the loop
    ble x1, x5, loop_exit    # if x1 <= 1, exit loop
    
    # Perform some computation creating forwarding opportunities
    add x3, x2, x5         # x3 = sum + 1
    mul x2, x3, x5         # sum = x3 * 1 (effectively x3)
    sub x6, x4, x1         # x6 = 20 - counter
    
    # Conditional branch that should test prediction
    blt x6, x3, branch_a   # if (20-counter) < sum, go to branch_a
    # Not taken path
    addi x2, x2, 1         # sum++
    j continue_loop

branch_a:
    # Taken path
    sub x2, x2, 1          # sum--
    
continue_loop:
    # More operations that create hazards
    add x7, x2, x6         # x7 = sum + (20-counter)
    addi x1, x1, -1        # counter--
    j loop_start           # unconditional jump back to loop

loop_exit:
    # Additional conditional branches to test prediction
    li x8, 0
test_branches:
    addi x8, x8, 1         # increment test counter
    bgt x8, x10, test_done # if test counter > x10 (x10=0), exit
    addi x9, x9, 5         # increment another counter
    blt x9, x4, test_branches  # if x9 < 20, continue loop
    j test_branches

test_done:
    # End program
    li x0, 0               # No operation for x0

end:
    j end