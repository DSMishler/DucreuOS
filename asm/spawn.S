# code adapted from the inclass repo

// taken from the printed output of process_print_frame_offsets
# 0000:  gpregs
# 0256:  fpregs
# 0512:  sepc
# 0520:  sstatus
# 0528:  sie
# 0536:  satp
# 0544:  sscratch
# 0552:  stvec 
# 0560:  trap_satp
# 0568:  trap_stack

# Macros to load using t6 as the address, which is x31
# (thus, the last iteration of such a macro might overwrite t6)
# Two macro systems in GAS: we need to use the alternative one
.altmacro
.macro loadgp i
   ld    x\i,  (8*\i)(t6)
.endm
.macro loadfp i
   fld   f\i,  (8*\i+256)(t6)
.endm
.macro storegp i
   sd    x\i,  (8*\i)(t6)
.endm
.macro storefp i
   fsd   f\i,  (8*\i+256)(t6)
.endm

.section .text
_spawn_kthread:
   csrr     t6, sscratch # sscratch held the top of the process_frame_t
   # this sscratch (above) might have been physical, whereas the one in
   # the process frame is surely virtual and in the process page table.

   # observe when satp and sscratch are loaded.
   # Case 1: we're the SBI.
   #    Answer: We just went to OS mode.
   #            sscratch is a physical address and won't be in the process
   #            page table. We need to make sure that the new version is
   #            loaded alongside the new page table
   # Case 2: we're the OS.
   #    Answer: satp will contain a page table that has sscratch mapped
   #            to the top of the frame anyway. in this case,
   #            old sscratch == new sscratch.
   ld       t0, 512(t6)
   ld       t1, 520(t6)
   ld       t2, 528(t6)
   ld       t3, 536(t6)
   ld       t4, 544(t6)
   ld       t5, 552(t6)

   csrw     sepc, t0
   csrw     sstatus, t1
   csrw     sie, t2
   csrw     satp, t3
   csrw     sscratch, t4
   csrw     stvec, t5

   csrr     t6, sscratch # Possible need to re-load t6

   // if we used floating point, save those registers. Otherwise, skip it.

   srli     t1, t1, 13
   andi     t1, t1, 3
   beqz     t1, tdskip_fp_loads

   .set i, 0
   .rept 32
      loadfp %i
      .set i, i+1
   .endr

   tdskip_fp_loads:
   .set i, 1 # skip x0, the zero register
   .rept 31  # load everything, including overwriting t6
      loadgp %i
      .set i, i+1
   .endr

   sret
# Necessary to align here since this is also the address for _spawn_trap.
# Note that _spawn_trap is an address that will be stored in stvec, so it must
# Be 4-byte aligned (last two bits are reserved for the mode of the vector)
.align 4
_spawn_kthread_end:

_spawn_trap:
   csrrw t6, sscratch, t6 # Atomic swap
   # Store all registers
   addi t0, t0, 3
   .set i, 1
   .rept 30
      storegp %i
      .set i, i+1
   .endr

   # now store t6
   mv    t5, t6
   csrrw t6, sscratch, t6
   sd    t6, 248(t5)

   csrr  t6, sscratch
   csrr  t1, sstatus
   srli  t1, t1, 13
   andi  t1, t1, 3
   beqz  t1, tpsskip_fp_loads

   .set i, 0
   .rept 32
      storefp %i
      .set i, i+1
   .endr
   tpsskip_fp_loads:


   # load trap's stack and kernel's page table
   ld    t1, 560(t6)
   ld    sp, 568(t6)
   # Since the kernel page table and the process page table have this
   # code segment mapped for S-mode, we could do this earlier. However,
   # it *must* be done before we call the os trap handler
   csrw  satp, t1

   call os_trap_handler


   # Now load back in the page table
   # (once again, it could be loaded later, but must be loaded before the sret)
   csrr  t6, sscratch
   ld    t1, 536(t6)
   csrw  satp, t1

   csrr  t1, sstatus # Assume the OS trap handler didn't change sstatus
   # (I have trust issues about this for some reason but I'm going on faith)
   srli  t1, t1, 13
   andi  t1, t1, 3
   beqz  t1, tpeskip_fp_loads

   .set i, 0
   .rept 32
      loadfp %i
      .set i, i+1
   .endr
   tpeskip_fp_loads:

   .set i, 1 # skip x0, the zero register
   .rept 31  # load everything, including overwriting t6
      loadgp %i
      .set i, i+1
   .endr

   sret

_spawn_trap_end:


# .dword: an assembler directive to enforce that this will be returned as
# an 8-byte number
.section .rodata
.global spawn_thread_start
.global spawn_thread_end
spawn_thread_start: .dword _spawn_kthread
spawn_thread_end:   .dword _spawn_kthread_end

.global spawn_trap_start
.global spawn_trap_end
spawn_trap_start:   .dword _spawn_trap
spawn_trap_end:     .dword _spawn_trap_end
