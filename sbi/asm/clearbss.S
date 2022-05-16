.section .text
.global clear_bss
clear_bss: # a yoink from Dr. Marz' code. TODO: why is this important?
   la       t0, _bss_start
   la       t1, _bss_end

1: // zero all the bss range
   bge      t0, t1, 1f
   sd       zero, (t0)
   addi     t0, t0, 8
   j        1b
1:

   ret
