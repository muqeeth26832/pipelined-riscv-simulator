# Simple test program to be used as default example
# Computes factorial of 5 using a loop

.global _start

_start:
    # Initialize registers for factorial calculation
    li x1, 5           # n = 5
    li x2, 1           # result = 1

factorial_loop:
    beq x1, zero, factorial_done  # if n == 0, exit loop
    mul x2, x2, x1     # result = result * n
    addi x1, x1, -1    # n = n - 1
    j factorial_loop   # continue loop

factorial_done:
    # Result is now in x2
    # Loop indefinitely to stop program
end:
    j end