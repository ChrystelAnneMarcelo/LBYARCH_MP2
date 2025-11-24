default rel
section .text
global saxpy_asm
saxpy_asm:
    ; RCX = z ptr
    ; RDX = x ptr
    ; R8  = n (size_t)
    ; R9  = y ptr
    ; XMM0 = a
    test    r8, r8
    jz      .done

.loop:
    movss   xmm1, dword [rdx]
    mulss   xmm1, xmm0
    addss   xmm1, dword [r9]
    movss   dword [rcx], xmm1

    add     rdx, 4
    add     r9, 4
    add     rcx, 4

    dec     r8
    jnz     .loop

.done:
    ret
