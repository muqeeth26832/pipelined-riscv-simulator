    .data
    .dword -14,13
    .text

    lui    x10, 0x10000
    ld     x7,0(x10)
    ld     x8,8(x10)
    addi   x9,x0,0

    add    x5, x7, x0
    add    x6, x8, x0

    slt    x28, x5, x0
    slt    x29, x6, x0
    xor    x30, x28, x29


    bge    x5, x0, skip_neg_x5
    sub    x5, x0, x5

skip_neg_x5:

    bge    x6, x0, skip_neg_x6
    sub    x6, x0, x6

skip_neg_x6:

FOR:
    beq    x6, x0, FINAL_RETURN
    andi   x28, x6, 1
    beq    x28, x0, HELPER
    add    x9, x9, x5

HELPER:
    slli   x5, x5, 1
    srli   x6, x6, 1
    jal    x0, FOR

FINAL_RETURN:
    beq    x30, x0, END_FN
    sub    x9, x0, x9

END_FN:
    sd     x9, 50(t0)
