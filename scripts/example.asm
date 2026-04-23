; example.asm — count r1 down from 9 to 0, printing each value, then halt.
;
; Register convention:
;   r0 = scratch  (reserved — overwritten by any 'jz <label>' / 'js <label>')
;   r1 = counter
;   r2 = zero register (kept 0 permanently so we can use 'jz r2, label'
;                        as an unconditional jump without the crack destroying the condition)
;
; NOTE: using 'jz r0, label' as an unconditional jump does NOT work — the label
;       crack overwrites r0 with the target address, so r0 != 0 at the point of
;       the actual jz test.  Use a dedicated zero register (r2 here) instead.

    movl r1, 9      ; r1 = 0x09  (upper nibble defaults to 0; movh not needed)
    movh r2, 0      ; r2 = 0  (zero register — never change this)
    movl r2, 0

loop:
    print r1
    addi  r1, -1
    jz    r1, done  ; if r1 == 0: jump to done  [CRACKED — overwrites r0]
    jz    r2, loop  ; unconditional: r2 == 0 always  [CRACKED — overwrites r0]

done:
    halt
