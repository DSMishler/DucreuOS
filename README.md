# Daniel Mishler

Testing to see if publickey auth works. Should now be able to commit normally!
# Build

Simply type `make drive`, then `make run`. Everything should be in order

# Notes

Some notes along the way:
   - Took RISCV linker script, SBI start.S, and SBI linker script from Prof. Marz


Some questions:
   - Why did two of my harts make it to main? (6:00 Monday lecture video)
   - Why does writing to the wrong adress (`0x1000000` instead of `0x10000000`)
     lead to so many issues? (PC ends up at zero!)


Notes: 2022-02-02
   - Spurious interrupt: the PLIC's claim is zero for some reason!
   - All Harts might receive an interrupt, but this is based on their threshold
     and the interrupt's priority.
   - We might also end up dropping events
   - No TOC on his website to ensure course materials don't get loose
   - Use the CSR read and write macros
   - Are we sure we don't have to save the PC in SBI mode? MEPC only does so
     much, does it not? What if we get back to back interrupts?

Notes: 2022-02-04
   - Monday: MMU
      - Have ring buffer done
      - go to syscall.c instead of uart_get

Notes: 2022-02-07
   - Before class working:
      - Enabling or disabling PMP (by commenting PMP_init) causes errors!
        Errors with PMP enabled. But interrupts not noticed without it?
      - Issue possibly discovered: "csrc" cmd in asm file. Why not csrw? Changed
      - pmp init function now commented out again.
      - Working!
      - Printf works from OS, but the SBI seems to hear uart interrupts first!
        Interesting :)
      - TODO: get makefile to work from top-level without needing to go into
        sbi directory.

   - In class now
   - There is now a weekly repo that has everything we did each week.
     They are tagged branches.
      - To look for any piece of code, go through gitlab.
   - We must import: mutex, semaphore, barrier
   - Critical section: where preemption would case failure
      - TODO: what is the rigorous definition of preemption?
   - Semaphore: blocks if zero, but can be much higher. Resource guard.
   - Mutex: binary semaphore. One in, one out.
   - "Get in there, get it done" >:)
   - Barriers: wait until multiple processes reach the barrier.
      - Not convinced of their utility
   - Atomic operations: try the lock over and over until you get it.
      - aq: an ordering primitive: Anything past the aq must be in order
      - rl: until the release instruction.
      - So can anybody do this? (TODO)
   - Generally you don't protect a critical section with a semaphore
   - Ring buffer suggestion: start and size instead of start and end
   - SBI printf is for debug only. At the end, we should be able to rm it
     and nothing happens.
   - Note: we can only spinlock in SBI. Not sleep lock.
   - TODO for weds: locking
   - hart race: all four of them go to 0x80000000
      - create a hart table with maximum of 8 harts.
      - hartid is index (not guaranteed by RISC-V, but in virt it works!)
      - hart states: stopped, started, stopping, starting
         - Stopping: preempt and shut off a stuck hart

Notes: 2022-02-09
   - Hart Management
   - gcc expects globals to be set to zero.
   - TODO: store the global pointer in the SBI's trap.s along with the stack
   - a tail call: it's not expecting to return
   - "naked" function: no stack (still not sure why it's wanted)
   - "noret" function: no prologue or epilogue
   - You can restart and shut down qemu
   - hart start: prep the table, then use the CLINT to cause the interrupt.
   - TODO: Console by Friday.
   - Next week: very busy week. Take a look at MMU first.
      - mmu
      - page allocator
      - malloc

Notes: 2022-02-10
   - Interested to know more about how PMP works. What is NA4 and what is NAPOT?

Notes: 2022-02-11
   - Yo why no string.h?
   - I managed to break stuff while adding in the hart structures.

   - Class notes begin:
   - interrupt -> mtvec --> ctrap --> check mcause --> look for async 3 -->
         hart_handle_msip
      - Aggressively use if statements, account for spurious interrupts
   - TODO: ensure comments on submission by Sunday
   - Note: git push --tags
   - UART review: "my mouth connected to your ear"

Notes: 2022-02-12
   - Found the bug: it's the global pointer!
   - TODO: make a 'shared' directory where shared header files between the sbi
     and OS can live
      - Looking at you, sbi/src/include/hart.h and sra/include/sbi.h
   - Absolutely stuck on why my hart status isn't showing up.
   - Still stuck on it an hour later. Pain.

   - Doing locking lecture to calm down from the frustration.
   - woah - you can atomically increment and decrement? I guess so.
   - TODO: post "multiply a matrix" typo from LOCK lecture notes.
      - Also: caught him saying "AMO Operations"
   - Also: barriers are kinda complicated with process implementations. That
     explains why I couldn't grasp how it might be done naievely.

Notes: 2022-02-13
   - We'll give it another good college try
   - GOT IT: turns out I wasn't saving the global pointer when I did a context
     switch. Now I know. Oof.
   - tagged SBI. Ready to grade. added another function or two :)

Notes: 2022-02-14
   - SV-39 MMU for RISCV CPU
   - Now ASID in TLB
   - 4096 is the standard page size
   - Mode: 0 for none, 8 for SV39
   - "the answer to anything the MMU can't do is page fault"
   - Sections of an executable:
      - Text (X)
      - bss (RW)
      - ROData (R)
      - Some 4th one (probably stack/heap)
   - Flush stuff out of the tlb: sfence
   - Consider the lecture powerpoint and lecture.
   - Page allocators
   - Take a look at what he's pushed
   - Definitely re-watch this lecture: most of it was over my head
   - Thoughts: how to solve the problem of the OS's page table changing
     before SRET?

Notes: 2022-02-16
   - *great* video
   - How exactly does the OS see user memory?
   - Why do mmu if you have a larger physical address space than virtual?

   - Class begins
   - Page allocator: first make one byte implement one page
   - COW: Copy on Write
   - TODO: Build the page allocator. zalloc for noncontiguous, znalloc for 
     contiguous
   - Heap allocator: kmalloc keeps track of bookkeeping bytes
   - Seems like a watch the lecture video moment

Notes: 2022-02-17
   - So for instruction reads, do you have to have *both* R and X bits in
     MMU set to 1?
   - Hmm... so AMO's always trigger page faults?
   - Why always asm volatile?
   - What marks corruption?
   - Don't feel quite so confident on HEAP

Notes: 2022-02-18
   - It is now that I'm trying to implement all the memory management things.
     Better late than never.
   - Class notes begin
   - We need to locate user processes in virtual space
      - But we might need to copy kernel pages into a user process
      - init - just sleep and spawn (like a college kid)
   - I don't think this is on Canvas modules: this is a watch the video moment
   - `size: . - init` gives us the size of the binary file (if placed at end)
   - Note: mtimecmp is a *signed* 64 bit number
   - And we need an SBI call to acknowledge that timer!
   - objcopy - flatten a binary
   - Definitely screenshot Marz flexin at the end
   - TODO: reading before Monday

Notes: 2022-02-20
   - Finalizing my OS to make it tag-worthy.
   - Marz's code has issues, bro.
   - double mutex unlock for hart stuff?
   - TODO: My clint doesn't work and I don't know why.
   - TODO: the Kernel's ASID isn't unique at 0xFFFF. Why that choice?
   - TODO: still don't know how symbols.h works
   - TODO: what if 10 processes are all competing for a semaphore? The swarm
     of AMO's would make the mean value quite low!
   - There is still a ton to do. I didn't do a good job of staying on top of
     this. There's catch up to do now :(

Notes: 2022-02-21
   - So, why does the x1, x2, etc. PCIE not look 16x smaller than the 16x?

Notes: 2022-02-21 in class
   - Peripheral Component Interconnect
   - Based Marz (Late because talking to Dr. Barry)
   - PCI Configures the device. Then we can talk to it directly.
   - y2k reference.
   - enumerate the bus: look for ventor ID != 0xffff
      - Use the structure unions to deal with type.
   - type 1: bridge
      - Set up the bridge
      - You can't see the devices until you set up the bridges
   - type 0: normal device
      - Set up the bars

Notes: 2022-02-22
   - Clearly my strategy hasn't been working. I'm *slowing down* and putting in
     some hours. Adding utility functions.
   - TODO: geeez... why won't probe work?
   - Some minor issues with mmu. Why is is messing with my page table? Man, I
     wish probe worked. Anyway, calling it for now...

Notes: 2022-02-23
   - Moved things into utils file
   - Marz' office hours: a godsend.
      - Probe didn't work because of sign extension
      - Couldn't hear the clint because of a lack of mret-ing!

   - Class
   - PCI: We match vendor IDs and device IDs to a driver
   - There will be some reference code at the start of this video
   - Think mtree
   - Recall DMRAM address starts at 0x8000\_0000
   - WARL: Write any read legal
   - Start PCI code now
   - Shivam has a head wobble ASCII graphic EMF5?
   - TODO: Fix comments from main after the fixes from today
      - MSIE
      - wfi after other harts boot
   - note: the offset is not offset from *here* to next, but from *base* to next
   - Two videos on virtio now.
   - 1: read the ECAM, device idea and vendor idea
   - 2: learn about how you're connecting the PLIC
   - "What's worse than managing people is managing students"
   - Friday will be Q&A. **start the PCI code**

Notes: 2022-02-25
   - Thoughts: should we split a free heap node even if one of the new nodes
     will have size 0? I suppose it sets us up for more likely coalescing.
   - Anyway, I'm looking at Marz's code and I think he's smoking cigarettes
   - I'm going to write my own in a much different fashion.
   - Note: printf-ing hex number more than 32 bits: use %lx.
   - Thoughts: why keep a free list instead of just an everything list?
      - Say we do this, then we could also keep track of whether it's free with
        just a little more bookkeeping.
   - Malloc done!

   - TODO: rename OS to DucreuOS
   - TODO: add more debug flags to makefile

Notes: 2022-02-28
   - Class: VirtI/O
   - see virtio-v1.1.pdf
   - We will only use block, entropy, GPU, and input devices (subtract 0x1040)
   - MMIO read will reset the vector!
   - Marz recommendation to share device memory: give each device up to
     0x100000 bytes (use the 6th bit at the device index)
   - Favor writing to memory as opposed to the I/O
   - reading and writing to these addresses correlates to us writing to the
     device itself
      - use 'volatile' judiciously
   - available: driver (that's us)
   - used: device
   - most of the time, the notification multiplier is zero
   - Much of the device stuff is negotiable, but the manner by which
     'negotiation' is used has strange implications.

Notes: 2022-03-02
   - Working before class to get PCIE up
   - Crazy how the memory is already set from 0x30000000 to 0x3FFFFFFF
   - Note: PCIe bridges are ways to tell the hardware where to send messages
   - Bus #1 *must* be behind a bridge. Bus #0 may have devices or bridges.

   - In class
   - Going through virtIO 3.1 (Device Initialization)
   - queue\_desc: size of the descriptor table
   - queue driver: physicl address
   - the device and driver ring might be different size
   - 1) choose an unused descriptor
   - 2) set phys to wherever we want to dump random bytes
      - size = \_\_\_\_\_
   - 3) ring[idx%] = that descriptor from 1)
   - 4) idx++
      - The device needs to resolve the discrepancy
   - 5) queue\_notify = 0
      - The device gets to speculate until we set to it.
      - Doesn't matter what we write to it. Anything will do.
   - Recall: configure the PLIC to listen to 32,33,34,35 and listen in S-mode
   - "arpha"

Notes: 2022-03-07 (entropy / RNG)
   - VirtIO not due until Sunday *after* Spring break. Large break moment
   - Writing the entropy device: requires all the things from before
   - Just noticed this is the first no mask classroom I've seen in a long time.
   - don't pretend offsets are hard-coded (even though in QEMU they are)
   - We need to give the device the right flags so that it can write to our
     physical memory (see RNG lecture notes)
   - See 2:25 in lecture for queue configuration coverage
      - Whenever we increment our index pointers, we may need to lock
      - Talk about unbounded arrays
   - How to find the qnotify? See 2:35
      - the go button
   - Steps
      - Driver updates index (driver ring)
      - Driver notices device
      - Device services
      - Device updates index (driver ring)
      - Driver writes to device ring
      - We see interrupt from PCI
      - Look at ISR Status (type 3) to see if that device did the interrupt
      - Now we (the driver) go to compare the driver vs device
      - Go to the PLIC and clear the interrupt
   - Best speech in 2:43
   - Debugging: do more than 65535 requests 
   - (note: RNG page is pseudocode)
   - Note: it's a quick system so we may still get random bytes if we don't wfi
   - From the past: use `info pci` in qemu

Notes: 2022-03-08
   - Looking into BARs, why is it so often bar[4]?
   - I noticed the capabilities seem to grow downwards
   - Set the command\_reg MEMORY\_SPACE\_BIT to 1 after initializing the BARs
   - I notice that the command\_reg seems to be zero beforehand anyway.

Notes: 2022-03-09
   - What is the deal with the function pointer syntax?
   - Frustrated trying to work through this with lack of full understanding.

   - Class: Block device I (BLK)
   - 0x1042 is device ID (yup)
   - create a block file (Marz says use fallocate)
   - talking about the hardware (512 bytes apiece)
   - if you want *some* memory, you have to read the whole sector
   - lots of hardware-accommodating functionality. We might implement the
     request merging
   - Possible TODO: make memcopy do 8 bytes at a time instead of byte by byte.
   - Headers:
      - A
         - header
         - type (I/O)
         - sector
      - B
         - data (the address to go to in memory)
      - C
         - status (0 ok, 1 error, 2 unsupported)
   - Send it to the ring, which knows the size
   - using function pointers: we don't know what the underlying system is
   - When you pass it the status, let the device write to it (and we don't put
     0, 1, or 2 so we can know it was the device)
   - set driver\_ring[idx%queue\_size] = idx to say we have chained descriptors
   - Marz recommendation
      - In your block header, we can add extra fields
      - add processes that are waiting
   - Talks on Spring Break. Nothing due over break, but things will quickly
     build.

Notes: 2022-03-19
   - Finally catching up on virtIO. Finger blisters on my hand :pain:
   - Watch Virtio video
   - Update everything and add it up to virtqueues.
   - I don't see how checking to see whether we recognize the config
     type is going to help us know how to access data in the bar.

Notes: 2022-03-21
   - TODO: Change flags from O0 to O2
      - If it breaks (probably will), check for missing `volatile` somewhere.
   - Watching virtqueues 
      - other names: (driver: available, device: used)
      - Drver and device rings index wraps around
      - Note queue size is in cfg type 1
   - Class (GPU)
      - 2D GPU (not 3D (virtgl))
      - Variable amount of descriptors per request
      - Device specific (#4) gives us the GPU struct
      - Frame buffer
         - R
         - G
         - B
         - alpha
      - This will require X11 forwarding. Remove the --nographic flag
      - There are 16 display scanouts, but we only use the first one.
      - Steps
         - 1 get dimensions
         - 2 creqte memory: width x height x 4
            - create a resource and say where the memory will come from
         - 3 create 2D resource
            - set_scanout
         - 4 attach backing store
      - UNORM: unsigned and normalized
      - resource id: set to 1
      - The order is very particular here
      - feel free to yoink the drawing primitives
      - Wednesday: keyboard and tablet. Add the keyboard in makefile

Notes: 2022-03-23
   - Some work on BARs before class
   - Class (GPU II & Input I)
      - uses a ring buffer (FIFO)
      - Each event: 8 bytes (type, code, value)
         - This type field can tell us whether it was a mouse or keyboard event
           (their device code is the same with 'input device'!)
            - Mouse pressing a button on the mouse gives you a *key* event
         - Code: it's NOT ASCII! You'll have to build a table
         - Value: pressed or unpressed. We have to manage shift on our own!
      - just set the write flag to give the buffer over
      - Give queue\_size number of buffers over
      - We will not use *this* as the ring buffer, we will make our own ring
        buffer
      - High comedy type of day
      - at: first piece of data not popped out yet
      - size: number of things in the queue (starting at 'at')
      - push things into the buffer at `at+size`
      - Add a 'behavior' field
      - Note: when using dd for elf files (next week) we can use the flag
        conv=notrunc
         - `dd if=console.elf of=hdd1.dsk conv=notrunc`

Notes: 2022-03-25
   - Working on virtio
   - trying to make good function for managing BARs
   - taking a while but worth it
   - back at it after ICL lunch
   - typo to fix in VIRTIO: search 'apossibly'

Notes: 2022-03-26
   - Hard grind today
   - TODO: swap from BARs in a random spot to Marz recommendation
      - This would also mean fixing up bridge forwarding

Notes: 2022-03-27
   - Did a little MMU checking. Probably not the mmu.
   - IT WAS THE MEMORY SPACE BIT.
   - Also messed up virtio structure a little bit.

   - No way you can get away with locally allocated structs sticking around
     in larger structures just by passing by address, Marz.
   - Noticed I *am* leaning on Marz code rn. I don't know how I would've found
     the notify offset without it.
   - Hard days' work. Interrupts not quite done yet. Noticed a bug or two.
     But it's time to get some sleep. Maybe Marz will give me the IRQ points
     anyway, God bless.

Notes: 2022-03-28
   - Class: (ELF)
      - Executable and Linkable Format
      - Other one: dynamically loading
      - We won't be doing the dynamic loading
      - Don't forget about setting sepc.
         - set it equal to the entry point and then sret
      - TODO: make sure you clear the BSS for the OS
         - Yeah you're rollin' the dice bud.
      - Sections might overlap: do the one which is least restrictive first
         - OR-in flags. Make sure we map in with least restrictive permissions.
      - process image: where we'd find text, bss, stack, etc.
         - it needs a page
      - We have a riscv.... -readelf command
      - TODO: things get faster if memset is not byte-wise but 8-byte-wise
      - Processes
         - Sleeping
         - Waiting
            - Know that a waiting process is waiting on a device
      - RECAP: only worry about ELF header and program table

Notes: 2022-03-29
   - More work on RNG. Fixed the bug. Added a strong virtio print/show function.
   - The bug was humilating. `&` instead of `%`. Pain.
   - Now time to look for ISR.
   - Also noticed a heap allocator bug: not translating page address back to
     physical before freeing. Fixed now.
   - Haven't fixed everything in kmalloc. Spent some time trying to debug
   - Also ended up working with Max
   - Still haven't gotten the PLIC claim to work. Plic just doesn't seem
     to be getting interrupts.

Notes: 2022-03-30
   - Oh shit RNG don't work no more
   - It's if you call it with size of 0, it seems.
   - Console: args cleanup
   - Seems like rng device just gives up when asked to do size of 0.
      - Office (solved) hours moment: it just can't

   - Work on Block Driver to get things started
   
   - office hours once again helpful
   - office (solved) hours moment: why save context on trap frame?
      - If you trap at the end of the function, you might get away with it.
   - Class (Processes I)
      - Seems like tiredness is setting in around right now :(
      - PS_RUNNING: Just means that the prcoess *can* be run!
      - We can know how much time the process took
      - We can configurable latency
      - We choose our default quantum *not* to be `1` so that we can modify
        it to go up or down as needed.
      - use mmap to get a process more space
      - facts that blow your mind first time: malloc is not a system call
      - We perform an ecall from U mode
         - Go to S mode
         - Go to ST-vec
         - csrr t6, sscratch, t6 (get the trap frame from sscratch)
      - Geez there's a lot to processes. Following the details meant couldn't
        follow notes simultaneously.
      - You can have every process have the same virtual address for its
        stack pointer (they have their own page tables)

   - More work
   - Fascinating that the OS\_LOAD\_ADDR happens to be main
   - Ah, no. It's start.S. God bless. Why does riscV.S exist?
   - TODO: fix the MAX\_SUPPORTED\_HARTS
   - WE GOT THEM INTERRUPTS: the lynchpin was fixing up the headers in the
     `plic\_set\_priority`. Feels good man. Everything is in order.
   - We were getting close to sanity wear and tear moments. Unit test, boys.
     I was about to think that I didn't understand shit.
   - I would like to re-do my BARs and turn them into a linked list. But for
     now let's get hose IRQs to work.
   - Went home and got back to it
   - When you get back, work on:
      - context switch in os_trap_vector
      - PLIC calls the right IRQ handler
      - Also what does IRQ stand for?


   - Got back in the evening
   - PLIC is not clearing, but we do save it!!
   - more debugging to do. For now, let's try to relax.

Notes: 2022-03-31
   - midnight oil.
   - still can't figure out why the PLIC only interrupts us the first time.
   - So, the PLIC keeps interrupting us. But how does the PLIC know how often?
     We could end up in an infinite trap loop. Office (solved) hours moment.
      - This doesn't happen because when an interrupt is triggered, interrupts
        are disabled by taking the [m,s]status registers and swapping previous
        and current.
   - I'll have to message the Piazza about this

Notes: 2022-03-31
   - Figured out why the PLIC wasn't getting interrupted: I wasn't always
     reading the register in the RNG IRQ handler! Why? Because it was getting
     optimized out by the compiler as it was not a volatile
   - I don't think I was particularly close to solving the issue on my own.
     I was getting at wondering whether the RNG was generating the interrupt,
     so perhaps. But thankful for the Piazza help. God bless

Notes: 2022-04-01
   - Not working on it today
      - April fools grind never stops
   - Are we supposed to be able to run the inclass repo from a pull only?
   - I suppose it wouldn't hurt to mess around with the code a bit to try
     to run. If I needed.
   - Added sbi who am i. Cleaned up prints.

   - Class:
      - When you trap, the prev and current [M,S]status bits swap
         - so you won't get interrupted in a trap vector
      - pid1 fathers all other processes
      - We will have an idle process for each hart
      - What does the `ack` mean?
      - Marz can meme hard
      - uid, gid: user and group number
      - But the environment is in the resource control part of the process
      - stderr: no buffer. Writes immediately to OS.
      - you can handle/disable POSIX signals
      - If we cannot guarantee memory is physically contiguous, we have to
        have a wrapper around mset.
      - Are we supposed to have background in file systems and red black trees?
      - Did Marz roast ACM?

Notes: 2022-04-03
   - Sunday Grind
   - Idea: could make the makefile call a script to make the disks if they're
     not already there.
   - TODO: PCIe's to PCI's for consistency.
   - cool, looks like my block device will take a queue size of 512.
   - TODO: add ERRORMSG macro to SBI. Does `__FILE__` work properly?
   - TODO: add ecam header to the bars\_occupied struct? 
   - Note: if you mess with the BARS settings after setting the `DEVICE_OK`
     bit on common config for virtio, then the `DEVICE_OK` bit gets uset.
   - TODO: can we do better than multiples of 512 for block device?
   - Noticing my bug rate is pretty high today.
   - Notice that we're just kinda SOL if the Block driver queue gets too full
   - block driver works lets goooo
   - It took 6 hours, though. We've gotta call it here for the day.

Notes: 2022-04-04
   - Class: schedulers
      - 1: Round robin
         - If none are schedulable, put on the idle process
         - if running, it's our candidate
      - 2: multilevel
         - Break up each priority level into its own round robins
         - Do a round-robin per bucket
         - risks starvation
      - 3: multilevel feedback (MLF)
         - We take a process, increase the quantum, decrease the priority
         - Therefore all processes eventually reach lowest priority
      - 4: Completely Fair Scheduler (CFS)
         - requires a red-black tree
         - key: vruntime
         - self-balancing binary tree
         - implement priorities as (or with) coefficients
      - Aside: hyperthreads do not have their own instruction fetch cycle
      - YOOO we got him with the living in your walls
      - End of a process: system call exit
      - exceptions like segfault can also end a process
      - We're going to have a per-hart current process
      - Hmmm never knew the origin of that term gaslighting
      - we also implement a wait queue for the process I/O
      - and we have to implement exit system call
      - Can't overlap PIDs either!

Notes: 2022-04-06
   - Class: file systems
      - Marz suggestion: implement in C or C++ *not* in OS first.
      - Testing this is not easy
      - All will connect to a virtual file system. Why? We'll have three
        physical files that should be thought about as one file system
      - 0x4d5a describes a MINIX3 files system
         - Or you just happened to get a file and be lucky and 4d5a is right
           there
      - mode: a 16-bit value telling you what type of flie (directory)
         - Leading 0: it's now octal
      - Design philosophy: smacking the device off the geyser and starting
        again
      - TODO: focus on the block driver: you might want to make a polling
        function since it can service our requests out of order.
      - indirect poiniter: pointer table
      - See a zone value of 0? Skip it. It's not the end of the list.

Notes: 2022-04-08
   - Gotta stay on that grind
   - Jeez, man, my BARS code is a mess
   - Programmatically determine BARS offset. But it's a mess, man.
   - Man, I'm blessed for having leak bugs and nothing goes wrong
   - Leave off: difference between two structs.
   - Cleaned up BARS some. still more could/should be done. But for now
     we need to work on GPU and stop being behind.
      - The improvements would not change the output, so I'm just gonna call it
        good enough for now.

   - Back on the grind
   - FS: it's floating point status! It's in the RISC-V documentation
   - typo in GPU lecture notes: "then, then"
   - typo in GPU lecture notes: extra comma in the large `enum` at end.

Notes: 2022-04-10
   - Let's finish up the GPU
   - Then input, then ELF files
   - Marz' code looks a little weird. I don't think I like the interface that
     I 'tactically acquired'. As a result, we might have
     strange conventions for gpu request functions.

Notes: 2022-04-11
   - Still on the GPU bug:
   - it seems like at that point in particular, (the `memory\_entry`),
     the memory isn't even being set properly!!
   - Class: Files systems II
      - Note: we'll have to handle that names aren't zero-padded for EXT4

Notes: 2022-04-12
   - GPU init
   - fix the scanout resource id in part 4
   - IT WORKS SHEESH LETS GO
   - Working on writing a whole image to GPU. Yeah, it's something that will
     take lots of extra time, but I'm doing it anyway. My OS, gotta understand
     it.
   - Chris Muncie has a cool script file. Stuff to learn from it.
   - image showing conclusions: it sucks. Image formats are too complicated
     and I got sent down a rabbit hole that turned out to be incorrect. I just
     wanted bitmapped bytes.

Notes: 2022-04-13
   - Grind never stops. Input today.
   - BUG NOTED: shrink the GPU window too small and my OS can't recover
      - Fix me... but maybe later. Input for now.
   - possible todo: make a function that combs the page table for virtual
     addresses that could possibly line up to your physical address
   - That grind went *well*. Good shape for input driver.
   - Office hours:
      - GPU: what to request:
      - GPU: how to make files;

   - Class: File Systems III
      - Virtual File System VFS
      - If we want our OS to support glibc, then we must support each of the
        Linux system calls
      - Stat: the `pathname` is a user-space memory address
      - We have to handle combining forward slashes `/`
      - We have to store which block device the inode came from with the inode
      - If we determine it's a bad block: we don't want anything allocated there
      - TODO: what is a 'sentinel value'
      - Marz recommendation: always start with Minix, then move to ext4
      - ext4 has a name length and record length (unlike minix3)
      - We have to make sure the file exists when someone calls fopen
      - there is no delete system call in linux. There is only unlink

      - schedulers: red-black or AVL?
      - Schedulers: if you just want to store a link list, that can work
      
      - Opening a file
         - Open is passed path and flags
            - translate the path from user to kernel space
            - go through the inodes to find '/'
               - (we need to know which block each inode is on)
            - OR: use the environment variable `getcwd()` to get relative path
            - then walk that tree to find the file

   - After class work on BMP images
   - TODO: get block device to poll

Notes: 2022-04-14
   - Did we ever say the grind would stop?
   - Noted on block device: the block device only reads the 12 least significant
     bits of a length! This means the largest value it can read it 65024.
      - Office hours moment: confirm this.
   - Input ring seems to work just fine!
      - ERROR behavior tested
      - Cleaning up print statements
   - Next step: adding other input device
   - Ctrl+Alt+G to release grab boys. Glad someone was thinking of me
   - Bug: multiple input devices not working
      - I know why... the BARS struct. Figured we'd have to re-do it sometime.
         - A: re-order checking so that it works (band-aid)
         - B: find a way to work around this pain with a good software solution.
      - A for now. TODO: B
   - Bugs squashed with Max:
      - csrr t6 and sscratch in sbi
      - pre-increment bridge number

Notes: 2022-04-18
   - Class: user space
      - Missed first 10 minutes. Arrive at 2:25. Review this.
      - System Calls
         - open
         - close
         - read
         - write
         - seek
         - stat
      - Slider application: break a pictures off into 16 different tiles
      - libc: what does it do to get you started?
         - 1: set global pointer
         - 2: set up argc and argv
         - 3: clear bss
      - What does libc do before exit? `atexit()`.
      - Everyone clear for what's next. Classes now done. Just work time

Notes: 2022-04-19
   - Possible suggestion: don't make the filesystem on the disk until later
   - Lots of cleanup. Multiple block devices. Went great. Now we're just
     missing a mutex, probably...
   - Further work: can't figure out the async bug. TODO
      - Looks like placing prints in the poll is enough to make the bug stop...
      - TODO: Give the input device the same treatment
   - Also looks like GPU goes inactive after sixth resize
      - TODO: zeroing in on this - it's the last element of the virtq.
         - Wait... no it can't be.
   - Aha... seems like I'm interrupted to init the GPU in the middle of init-ing
     the GPU
      - Which is why I'm not seeing interrupts
   - Also TODO: ring buffer filling too fast.
      - solved: on campus events are just flying!
   - Lots of bugs. But after 4 hours of debugging we just gotta hold the L high
     and move on.

Notes: 2022-04-20
   - THEORY: the GPU will not interrupt you if the display info changed unless
     you actually ask for the display info

Notes: 2022-04-21
   - Testing the theory
   - Theory: naw, incorrect.
   - The GPU will always interrupt you when it's done being set up.
      - I just wasn't noticing it because I wouldn't do anything if there
        was already a frame buffer in place.
      - Still a little uneasy about re-init-ing in the middle of an interrupt,
        but we'll let it be for now.
   - Input events: still looks normal for now (against all odds)
   - Done with all of that and we're off to the races.
   - Onward to elf files
   - TODO: input driver gets the device ID and name through DEVIDS

Notes: 2022-04-24
   - Office (solved) hours moments
      - How does zone bitmap offset work? (quiz)
         - Zone bitmap starts with the first zone available for use (stored
           in `first_data_zone` in the superblock).
      - How does max file size work (quiz)
         - The amount of zones for direct pointers, then your indirect pointers,
           then doubly indirect, then triply indirect. If it's greater than
           the MINIX3 max file size, then you can forget about it.
   - Typo in reading notes: MINIX3 log\_block\_bits described inconsistently
      - Also on the quiz, it asked for a variable that doesn't exist in the
        lecture notes

Notes: 2022-04-26
   - ELF grind
   - use `'\177'` in a string? It's octal. That's helf `0x7f`
   - Possible TODO: go back through src/elf.c and be surgical with page
     permissions instead of just mapping everything.
   - possible typo: 'will use the hart 100\%' on lecture notes (PROC)
   - office (solved) hours moment: what does it even mean to rely on being able
     to move the position of the code (PROC lecture notes)
      - See -fPIC Makefile comments: we want the libraries to stay packaged
        right where they are so we know where to find them.

Notes: 2022-04-27
   - big pushes forward today
   - still working on processes. Next? spawning processes
   - TODO: compare input driver code to Marz' to see about interrupt frequency
   - TODO: dynamically determine how many harts are active.
   - left off: revisiting clint and preparing to implement those sbi calls
     that use the timer to push forward with processes
   - TODO: rtc
   - NOTE that the satp loaded currently is just a band-aid
   - Make sure we map the instructions as well
      - left off with instruction page fault...

Notes: 2022-04-28
   - There are 300% bugs in the inclass code xd (spawn trap code would skip
     calling the trap handler if FS bits were 0!)
   - Office (solved) hours moment: why do elf files start at 0x400000?
      - Because the linker script in user/lds gives them that!
   - lowhonor for calling SBI's and OS's trap handler the same thing
   - Note: don't forget to add 4 to the PC if you get an ecall from U-mode.
     Otherwise it will just keep happening!
   - We've got system calls recognized from U-mode to S-mode. Next: handling
     them!

Notes: 2022-04-29
   - Time to make my system call handler.
   - Done.
      - Now see if we can get printf.
      - Then make a process spawn on another hart.

Notes: 2022-04-29
   - Office hours notes (very helpful)
   - gcc -E: stop at the preprocessor stage
      - s: after compiler (assembly)
      - o: after linking
      - c: after assembler (object files)
   - office hours moment (just to note to do it): try some C business with
     recursive #defines, like:
      - #define A B
      - #define B A (or #define B C)
      - And see what happens
   - TODO: move the OS into its own folder (so we have SBI, OS, and USER
     folders) and go crazy on reorganizaton. Make sure the three parts are
     not dependent upon each other

Notes: 2022-05-01
   - Idle processes

Notes: 2022-05-02
   - Working on scheduler
   - I suspect ciggy-smoking on the inclass repos.
     I see no measurement of runtime.
   - Debug train: can you WFI in user mode?
      - Answer: no!

   - Next task: how can we have the scheduler return the process to exactly
     where it left off? Consider how to manage sepc.
     
   - Still thinking about what the bug must have been when I got the illegal
     instruction the *first* time when I wasn't memcpy-ing the whole page...

Notes: 2022-05-03
   - Solved the bug, but extremely dissatisfied with my *why*. Can we do better?
   - It wasn't making it volatile. Where is this thing `pl->head` changing?

Notes: 2022-05-04
   - Onto MINIX3
   - TODO: I'm still thinking about those trap stacks... if the stack starts at
     0x1000 later than the previous thing mapped, and grows up,
     isn't the first element of the stack not in range? That's hart 8's trap
     stack, no?
   - Seeing that ls from "/" has got to be one of the coolest moments I've had
     in this class
   - office hours moment: what does it do when I run the make filesystem
     command in the terminal? Like how my makefile does it with hdd.dsk?

Notes: 2022-05-05
   - current assumption: everything in a directory must belong to the same
     filesystem. Once something is mounted, you can't mix mount trees.
     Office hours moment: is this reasonable?
   - What about the first data zone? Doesn't seem to match
     where I see the data zones being used...
      - The `first_data_zone` field in the superblock tells us where the
        location in the bitmap starts.
   - EMF4 bug: call kfree after 'minix3 print', program loops.

Notes: 2022-05-06
   - We fixed the bug. Turns out that kfree wouldn't check to see if the
     thing being freed would end up being the largest thing in the free list.
     As a result, it would infinitely loop while looking. Fixed now.
      - Back down to EMF-1
      - Also added locks to kmalloc
   - I now also realize that my kmalloc is going to throw the error if it
     returns NULL. I can be free with the malloc checks for my OS. We can let
     go, for kmalloc itself will be the judge.
   - TODO: make the arrow keys cycle through previous commands in the OS

Notes: 2022-05-09
   - Office hours moment: there was an office hours page?

Notes: 2022-05-12
   - Grind never stopped
   - possible TODO: make it so you can't write to the gpu frame buffer if
     we're not running with make rung

Notes: 2022-05-13
   - Working on parsing input events
   - ack idx is getting reset EMF5?
      - fixed. Always not what's a pointer and what's local, kids
   - TODO: we could get the context switch timer to go off right after a process
     calls sleep. What happens then?

   - Game plan (system calls):
      - Get x pixels
      - Get y pixels
      - rectangle draw
      - region add
      - circle draw

Notes: 2022-05-14
   - Did all of that and got paint working

Notes: 2022-05-15
   - Grind never stops
   - Can we get a bmp file onto the minix3 file system?
   - JoeyD.bmp claims to be: 819432 bytes
      - Considering the header is 54 bytes and ls -l says 819486, I buy it
   - Leave off debugging `vfs_bmp_to_file`

Notes: 2022-05-16
   - Just pulled?
      - make drive
      - cp <test minix3 file system> mx3.dsk
      - diskutil (or use makefile) to get a bitmapped file into bg.dsk
   - Presentation
      - virtio
      - process
      - scheduler
      - file system
      - (vfs)
      - console
      - paint
      - slider infrastructure
   - Marz: is that a nipple
   - Jantz: got no productivity from my team this semester (xd)
   - Spring break: we all got stoned - Marz recounting achievements




























