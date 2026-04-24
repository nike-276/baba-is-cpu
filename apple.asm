    movl    r2, 0
    movh    r2, 0

    la      r0, P1
    st      r2, r0
    movh    r3, 0xc
    movl    r3, 0x0
    jz      r2, phrase
P1:
    la      r0, P2
    st      r2, r0
    movh    r3, 0xb
    movl    r3, 0x0
    jz      r2, phrase
P2:
    la      r0, P3
    st      r2, r0
    movh    r3, 0xc
    movl    r3, 0x0
    jz      r2, phrase
P3:
    la      r0, P4
    st      r2, r0
    movh    r3, 0xb
    movl    r3, 0x0
    jz      r2, phrase
P4:
    la      r0, P5
    st      r2, r0
    movh    r3, 0xa
    movl    r3, 0x0
    jz      r2, phrase
P5:
    la      r0, P6
    st      r2, r0
    movh    r3, 0xa
    movl    r3, 0x0
    jz      r2, phrase
P6:
    la      r0, P7
    st      r2, r0
    movh    r3, 0x9
    movl    r3, 0x0
    jz      r2, phrase
P7:
    la      r0, P8
    st      r2, r0
    movh    r3, 0xc
    movl    r3, 0x0
    jz      r2, phrase
P8:
    la      r0, P9
    st      r2, r0
    movh    r3, 0xb
    movl    r3, 0x0
    jz      r2, phrase
P9:
    la      r0, P10
    st      r2, r0
    movh    r3, 0xc
    movl    r3, 0x0
    jz      r2, phrase
P10:
    la      r0, P11
    st      r2, r0
    movh    r3, 0xb
    movl    r3, 0x0
    jz      r2, phrase
P11:
    la      r0, P12
    st      r2, r0
    movh    r3, 0xa
    movl    r3, 0x0
    jz      r2, phrase
P12:
    la      r0, P13
    st      r2, r0
    movh    r3, 0xa
    movl    r3, 0x0
    jz      r2, phrase
P13:
    la      r0, P14
    st      r2, r0
    movh    r3, 0x9
    movl    r3, 0x0
    jz      r2, phrase
P14:
    la      r0, P15
    st      r2, r0
    movh    r3, 0x8
    movl    r3, 0x0
    jz      r2, phrase
P15:
    play

# r0 = XX, r1 = retaddr, r2 = 00, r3 = [bar addr]
bar:
    ld      r2, r3
    ld      r0, r2
    print   r0
    addi    r2, 1
    ld      r0, r2
    print   r0
    addi    r2, 1
    ld      r0, r2
    print   r0
    addi    r2, 1
    ld      r0, r2
    print   r0

    movl    r2, 0
    movh    r2, 0
    jz      r2, r1

# r0 = XX, r1 = arg1, r2 = 00, r3 = phrase addr
phrase:
    movl    r1, 0b0000 # -16
    movh    r1, 0b1111
    movl    r2, 1 # counter at mem[01]
loop:
    st      r2, r1
    movl    r2, 0
    la      r1, postbar
    jz      r2, bar
postbar:
    addi    r3, 1 # phrase addr++
    movl    r2, 1
    ld      r1, r2
    addi    r1, 1
    js      r1, loop
    movl    r2, 0 # ret addr at mem[00]
    ld      r1, r2
    jz      r2, r1
