.section .text
.altmacro

.macro savegp i
   sd x\i, (8 * \i)(t6) // times 8 because a double word is 8 bytes here.
.endm

.macro loadgp i
   ld x\i, (8 * \i)(t6)
.endm

// office (solved) hours moment: why save the context anyway?
// We might end up getting lucky if we don't save the context, especially
// if we trap right before the return of a void function. That said, we
// should always save the context
.align 4
.global os_trap_vector
os_trap_vector:
   # context switch
   csrrw t6, sscratch, t6
   # save context
   .set i, 1 # We don't need to save x0. RISC-V moment.
   .rept 30
      savegp %i
      .set i, i + 1
   .endr

   mv t5, t6
   csrrw t6, sscratch, t6
   sd t6, 8*31(t5)

   call os_trap_handler


   csrr t6, sscratch

   # load context
   # Why not load in t6 differently? t6 happens to contain the correct value,
   # and it will be overwritten by the last command in this series.
   .set i, 1
   .rept 31
      loadgp %i
      .set i, i + 1
   .endr
   sret
