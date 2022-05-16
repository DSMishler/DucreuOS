// start a process (most of code taken from Marz' course repo)
.section .text
.global _start
_start:
   la    t0, _bss_start
   la    t1, _bss_end

   clear_bss_loop:
      bge   t0, t1, bss_cleared
      sd    x0, 0(t0)
      addi  t0, t0, 8
      j     clear_bss_loop

bss_cleared:
   la    gp, __global_pointer$

   call main

   li    a7, 0 #(exit syscall code)
   ecall

