# Test program for branch prediction
# This program creates branches that can test static and dynamic prediction

.global _start

_start:
    # Initialize registers
    li x1, 10          # counter
    li x2, 0           # sum
    li x3, 5           # threshold for branch

loop:
    # Check if counter > threshold
    bgt x1, x3, branch_taken    # Branch if x1 > x3 (x1 > 5)
    # If not taken path
    addi x2, x2, 1     # increment sum by 1
    jal x0, continue

branch_taken:
    # If taken path
    addi x2, x2, 10    # increment sum by 10

continue:
    addi x1, x1, -1    # decrement counter
    bnez x1, loop      # branch if counter != 0

# End program - infinite loop
end:
    jal x0, end
