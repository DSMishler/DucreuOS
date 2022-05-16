.section .text.init
.global _start
_start:
.option push
.option norelax
   la       sp, _stack_end
   la       gp, __global_pointer$
.option pop


   la       t1, main
   csrw     sepc, t1

   //    (supervisor mode (why not << 11?)) (SPP) (SPIE) //TODO: look up further.
   // SPP (1 << 8): supervisor priveleges. Cannot set above 0 or 1. Can lower.
            # FS_INITIAL # SPP_SUPERVISOR # SPIE
   li       t1, (1 << 13) | (1 << 8) | (1 << 5)
   csrw sstatus, t1

   # PLIC
   li       t1, (1 << 9) // TODO: look up further
   csrw     sip, t1

   # enable interrupts
              # SIE_SSIE   #SIE_STIE #SIE_SEIE
   li       t1, (1 << 1) | (1 << 5) | (1 << 9)
   csrw     sie, t1

   la       t1, os_trap_vector // where to jump when the OS traps
   csrw     stvec, t1

   la       ra, supervisor_hang # sret is not a jump and link instruction

   csrr a0, sscratch // let the OS know which hart we are
   sret

.global supervisor_hang
.align 4
supervisor_hang:
   wfi
   j      supervisor_hang
supervisor_hang_end:

.section .rodata
.global process_idle_code_start
.global process_idle_code_end
process_idle_code_start:     .dword supervisor_hang
process_idle_code_end:       .dword supervisor_hang_end
