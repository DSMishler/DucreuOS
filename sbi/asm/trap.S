.section .text
.altmacro

.macro savegp i
   sd x\i, (8 * \i)(t6) // times 8 because a double word is 8 bytes here.
.endm

.macro loadgp i
   ld x\i, (8 * \i)(t6)
.endm

.align 4
.global sbi_trap_vector
sbi_trap_vector:
   # context switch
   csrrw t6, mscratch, t6
   # save context
   .set i, 1
   .rept 30
      savegp %i
      .set i, i + 1
   .endr

   mv t5, t6
   csrrw t6, mscratch, t6
   sd t6, 8*31(t5)
.option push
.option norelax
   la sp, _stack_end
   la gp, __global_pointer$
.option pop
   #handle possibly multiple stacks
   csrr t0, mhartid
   slli t0, t0, 12  //2**12 = 4096, stack size. Multiply that times hart number.
   sub  sp, sp, t0

   call c_trap_handler


   csrr t6, mscratch

   # load context
   # Why not load in t6 differently? t6 happens to contain the correct value,
   # and it will be overwritten by the last command in this series.
   .set i, 1
   .rept 31
      loadgp %i
      .set i, i + 1
   .endr
   mret
