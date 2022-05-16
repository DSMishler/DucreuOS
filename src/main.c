#include <printf.h>
#include <sbi.h>
#include <lock.h>
#include <hart.h>
#include <pog.h>
#include <joeyD.h>
#include <page.h>
#include <mmu.h>
#include <symbols.h>
#include <csr.h>
#include <utils.h>
#include <bmp.h>
#include <kmalloc.h>
#include <pcie.h>
#include <plic.h>
#include <rng.h>
#include <block.h>
#include <gpu.h>
#include <regions.h>
#include <input.h>
#include <ring.h>
#include <process.h>
#include <elf.h>
#include <main.h>
#include <sched.h>
#include <minix3.h>
#include <vfs.h>

extern page_table_t *kernel_mmu_table;
uint64_t OS_GPREGS[MAX_SUPPORTED_HARTS][32];

int main(int hart)
{
   printf("OS booted on hart %d\n", hart);

   // provide trap frames
   CSR_WRITE("sscratch", &(OS_GPREGS[hart][0]));

   page_init();
   page_table_t *pt = page_zalloc();
   kernel_mmu_table = pt;

   mmu_map_multiple(pt, sym_start(text), sym_start(text),
                    sym_end(text)-sym_start(text),
                    PTEB_READ | PTEB_EXECUTE);
   mmu_map_multiple(pt, sym_start(data), sym_start(data),
                    sym_end(data)-sym_start(data),
                    PTEB_READ | PTEB_WRITE);
   mmu_map_multiple(pt, sym_start(rodata), sym_start(rodata),
                    sym_end(rodata)-sym_start(rodata),
                    PTEB_READ);
   mmu_map_multiple(pt, sym_start(bss), sym_start(bss),
                    sym_end(bss)-sym_start(bss),
                    PTEB_READ | PTEB_WRITE);
   mmu_map_multiple(pt, sym_start(stack), sym_start(stack),
                    sym_end(stack)-sym_start(stack),
                    PTEB_READ | PTEB_WRITE);
   mmu_map_multiple(pt, sym_start(heap), sym_start(heap),
                    sym_end(heap)-sym_start(heap),
                    PTEB_READ | PTEB_WRITE);
   // PLIC
   // see sbi/src/include/plic.h for these digits
   mmu_map_multiple(pt, 0x0c000000, 0x0c000000,
                    0x0c2FFFFF - 0x0c000000,
                    PTEB_READ | PTEB_WRITE);

   // ECAM
   // TODO: add in macros for these
   mmu_map_multiple(pt, 0x30000000, 0x30000000,
                    0x3FFFFFFF - 0x30000000,
                    PTEB_READ | PTEB_WRITE);
   // VIRTIO BARs
   mmu_map_multiple(pt, 0x40000000, 0x40000000,
                    0x4FFFFFFF - 0x40000000,
                    PTEB_READ | PTEB_WRITE);

   CSR_WRITE("satp", SATP_MODE_ON | ASID_TO_SATP(KERNEL_ASID) | PPN_TO_SATP(pt));
   // because the machine will just go ahead and start speculating
   asm volatile("sfence.vma");
   init_kernel_heap();

   // init PCI
   pcie_scan_and_init();


   // no need to init gpu: it will be initialized when it loads and gives
   // and interrupt to the OS
   regions_init();

   // start listening for interrupts
   plic_init(hart);

   // file systems
   minix3_init();

   vfs_init();
   vfs_mount_minix3(v_dir_root);

   // prepare for process spawning
   process_init_trap_stacks();

   // scheduler
   sched_init();

   joeyD();

   block_write_arbitrary(0, "hello world", 8, 8);

   console();
   // console does not return, but just for completeness:
   return 0;
}

void console(void)
{
   printf("DucreuOS console: %s> ", vfs_pwd);
   char c;
   char command[COMMAND_MAX_LEN];
   char prev_command[COMMAND_MAX_LEN];
   prev_command[0] = 0;
   int index = 0;

   do
   {
      while((c = sbi_getchar()) == 0xff)
      {
         ;
      }
      if(c == '\r')
      {
         sbi_putchar('\n');
         command[index] = '\0';
         mstrcpy(prev_command, command);
         execute_command(command);
         printf("\n");
         printf("DucreuOS console: %s> ", vfs_pwd);
         index = 0;
      }
      // good idea - supporting backspace is cool
      else if(c == '\b' || c == 127)
      {
         if(index > 0)
         {
            command[index--] = '\0';
            printf("\b \b");
         }
      }
      // finally going to ignore arrow keys
      else if(c == '\x1b') // escape sequence
      {
         char c1 = sbi_getchar();
         char c2 = sbi_getchar();
         if(c1 == '\x5b')
         {
            switch(c2)
            {
            case '\x41':
               for(; index != 0; index--)
               {
                  printf("\b \b");
               }
               mstrcpy(command, prev_command);
               index = mstrlen(command);
               printf(command);
               break;
            case '\x42':
               printf("DOWN(ignored)");
               break;
            case '\x43':
               printf("RIGHT(ignored)");
               break;
            case '\x44':
               printf("LEFT(ignored)");
               break;
            default:
               sbi_putchar('?');
            }
         }
      }
      else
      {
         sbi_putchar(c);
         command[index++] = c;
      }
   }
   while(1);
}

void execute_command(char *command)
{
   int command_nargs = countargs(command);
   char **command_argv = make_argv(command);
   if(samestr(command, "help"))
   {
      // possible TODO: Make the OS help function sassy with a sleep function
      printf("available commands:\n");
      printf("   - help: display this message\n");
      printf("   - stat: display hart status\n");
      printf("   - shutdown, quit: exit OS without the need to Ctrl A + X\n");
      printf("   - probe <addr> <size>: probe memory at addr (in hex) \n");
      printf("   - set <addr> <val>: set addr (in hex) to val (in hex) \n");
      printf("   - start <hart> <priv mode> <address>\n");
      printf("   - stop <hart>\n");
      printf("   - pt: see the kernel's page table high level view\n");
      printf("   - page: see which pages the kernel has allocated\n");
      printf("   - heap: print out the heap free list\n");
      printf("   - malloc <bytes>: kernel mallocs some number of bytes\n");
      printf("   - free <address>: kernel frees memory that you malloc'ed\n");
      printf("   - pog: celebrate a little!\n");
      printf("   - joeyD: display OS logo\n");
      printf("   - pcie: Mishler pcie testing function\n");
      printf("   - bars: Mishler bars struct\n");
      printf("   - rng <longs>: Generate <longs> random longs and print\n");
      printf("   - nargs <...>: show how many args you just typed\n");
      printf("   - argv  <...>: print out the arguments you just typed\n");
      printf("   - virtq <dev>: print out the virtqueue of the device <dev>\n");
      printf("   - irqs: show the pcie IRQ linked list\n");
      printf("   - elfread\n");
      printf("   - blockread\n");
      printf("   - blockwrite\n");
      printf("   - clinttime\n");
      printf("   - clintadd\n");
      printf("   - clintset\n");
      printf("   - clintcmp\n");
      printf("   - schedinvoke\n");
      printf("   - top\n");
      printf("   - vfs\n");
      printf("   - minix3\n");
      printf("   - ls\n");
      printf("   - cd\n");
      printf("   - mkdir\n");
      printf("   - rmdir\n");
      printf("   - cat\n");
      printf("   - bmp2file\n");
      printf("   - file2screen\n");
      printf("   - inputring <op>: do something with the input ring\n");
      printf("   - gpu <op>\n");
      printf("   - region <op>\n");
      printf("   - minix3 <op>\n");
      printf("   - vfs <op>\n");
   }
   else if(samecmd(command, "stat"))
   {
      printf("\n");
      printf("hart status\n");
      printf("-----------\n");
      int i, status;
      for(i = 0; i < MAX_SUPPORTED_HARTS; i++)
      {
         status = sbi_get_hart_status(i);
         if (status == HS_INVALID)
         {
            continue;
         }
         printf("hart %d: ", i);
         switch(status)
         {
         case HS_STARTED:
            printf("started");
            break;
         case HS_STARTING:
            printf("starting");
            break;
         case HS_STOPPED:
            printf("stopped");
            break;
         case HS_STOPPING:
            printf("stopping");
            break;
         }
         printf("\n");
      }
   }
   else if(samecmd(command, "shutdown") ||samecmd(command, "quit"))
   {
      printf("Shutting down...\n");
      sbi_poweroff();
   }
   else if(samecmd(command, "probe"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `probe <vaddress> <size>`\n");
         printf("   assumes size is `long` (8 byte) if not specified.\n");
      }
      else
      {
         unsigned long addr;
         addr = matox(command_argv[1]);
         if(command_nargs == 2)
         {
            addr = addr & (~0x7);
            printf("probing memory address 0x%012lx\n", addr);
            unsigned long memory;
            memory = *((unsigned long *)addr);
            printf("0x%012lx : 0x%016lx\n", addr, memory);
         }
         else
         {
            if(samestr(command_argv[2], "half"))
            {
               addr = addr & (~0x3);
               printf("probing memory address 0x%012lx\n", addr);
               unsigned int memory;
               memory = *((unsigned int *)addr);
               printf("0x%012lx : 0x%08x\n", addr, memory);
            }
            else if(samestr(command_argv[2], "long"))
            {
               addr = addr & (~0x7);
               printf("probing memory address 0x%012lx\n", addr);
               unsigned long memory;
               memory = *((unsigned long *)addr);
               printf("0x%012lx : 0x%016lx\n", addr, memory);
            }
            else
            {
               printf("error: size parameter may only be `half` or `long`\n");
            }
         }
      }
   }
   else if(samecmd(command, "set"))
   {
      if(command_nargs != 3)
      {
         printf("error: usage is `set <vaddress> <64-bit value>`\n");
      }
      else
      {
         unsigned long addr, val;
         addr = matox(command_argv[1]);
         val  = matox(command_argv[2]);
         addr = addr & (~0x7);
         printf("setting memory address 0x%012lx to 0x%016lx\n", addr, val);
         *((unsigned long *)addr) = val;
      }
   }
   else if(samecmd(command, "start"))
   {
      if(command_nargs != 4)
      {
         printf("error: usage is `start <which_hart> <address> <scratch>`\n");
      }
      else
      {
         printf("ERROR: function deprecated since hart start has changed\n");
         // TODO: remove this whole function... you can't really have
         // a call like this without properly setting up a process first.
         if(0)
         {
            unsigned int hart;
            uint64_t addr;
            uint64_t scratch;
            hart = matox(command_argv[1]);
            addr = matox(command_argv[2]);
            scratch = matox(command_argv[3]);
            sbi_hart_start(hart, addr, scratch);
         }
      }
   }
   else if(samecmd(command, "stop"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `stop <which_hart>`\n");
      }
      else
      {
         unsigned int hart;
         hart = matox(command_argv[1]);
         sbi_hart_stop(hart);
      }
   }
   
   else if(samecmd(command, "page"))
   {
      print_pages();
   }

   else if(samecmd(command, "pt"))
   {
      mmu_show_page_table(kernel_mmu_table, kernel_mmu_table, 2, 0);
   }

   else if(samecmd(command, "heap"))
   {
      heap_print_free_list();
   }

   else if(samecmd(command, "malloc"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `malloc <num_bytes>`\n");
      }
      else
      {
         unsigned int bytes;
         bytes = matoi(command_argv[1]);
         char *my_memory = kmalloc(bytes);
         printf("contratulations, you have just malloc'ed address 0x%012lx\n",
                 my_memory);
      }
   }
   else if(samecmd(command, "free"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `free <address>`\n");
      }
      else
      {
         char *address;
         address = (char *)matox(command_argv[1]);
         kfree(address);
         printf("memory at 0x%012lx freed\n", address);
      }
   }
   else if(samecmd(command, "pcie"))
   {
      printf("enumerating all PCIE memory (brute force)\n");
      pcie_show_cycle_all_addresses();
   }
   else if(samecmd(command, "bars"))
   {
      printf("displaying the BARS struct to you...\n\n");
      pcie_show_bar_list();
   }
   else if(samecmd(command, "rng"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `rng <num_longs>`\n");
      }
      else
      {
         int numlongs = matoi(command_argv[1]);
         int numbytes = numlongs * 8;
         void *buffer = kcalloc(numbytes); // must be contiguous!
         printf("filling in memory at address 0x%012lx with %d random bytes!\n",
                buffer, numbytes);
         rng_fill_poll(buffer, numbytes);
         int i;
         for(i = 0; i < numlongs; i++)
         {
            printf("random number #%02d: 0x%016lx\n",
                                  i, *(((uint64_t*)buffer)+i));
         }
         kfree(buffer);
      }
   }
   else if(samecmd(command, "elfread"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `elfread` <dev>\n");
      }
      else
      {
         int dev   = matoi(command_argv[1]);
         int start = 0;
         int size  = 512*400; // TODO: get elf file size
         char *buffer = kcalloc(size);
         MALLOC_CHECK(buffer);

         process_t *p;
         p = process_init_new(PS_PRIV_USER);

         printf("reading block device %d as an elf file...\n", dev);
         block_read_poll(dev, buffer, start, size+start);
         int retval;
         retval = elf_load(p, buffer);
         if(retval)
         {
            printf("elfread error: won't load process\n");
         }
         else
         {
            printf("scheduling process now...");
            sched_assign_pid(p);
            sched_add(p);
         }
         kfree(buffer);
      }
   }
   else if(samecmd(command, "poffsets"))
   {
      process_print_frame_offsets();
   }
   else if(samecmd(command, "top"))
   {
      process_top();
   }
   else if(samecmd(command, "schedinvoke"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `schedinvoke` <hart>>\n");
      }
      else
      {
         int hart = matoi(command_argv[1]);
         sched_invoke(hart);
      }
   }
   else if(samecmd(command, "clinttime"))
   {
      uint64_t time = sbi_get_clint_time();
      printf("clint time: 0x%016x (%d)\n", time, time);
   }
   else if(samecmd(command, "clintset"))
   {
      if(command_nargs != 3)
      {
         printf("error: usage is `clintset` <hart> <value>\n");
      }
      else
      {
         uint64_t hart= matoi(command_argv[1]);
         uint64_t set = matoi(command_argv[2]);
         printf("setting clint time: %d\n", set);
         sbi_set_clint_timer(hart,set);
      }
   }
   else if(samecmd(command, "clintadd"))
   {
      if(command_nargs != 3)
      {
         printf("error: usage is `clintadd` <hart> <value>\n");
      }
      else
      {
         uint64_t hart= matoi(command_argv[1]);
         uint64_t add = matoi(command_argv[2]);
         printf("adding clint time: %d\n", add);
         sbi_add_clint_timer(hart,add);
      }
   }
   else if(samecmd(command, "clintcmp"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `clintcmp` <hart>\n");
      }
      else
      {
         uint64_t hart= matoi(command_argv[1]);
         sbi_show_clint_timer(hart);
      }
   }
   else if(samecmd(command, "blockread"))
   {
      if(command_nargs < 4)
      {
         printf("error: usage is `blockread` <dev> <start> <size>\n");
      }
      else
      {
         char *as;
         if(command_nargs == 5)
         {
            as = command_argv[4];
         }
         else
         {
            as = "char";
         }
         int dev   = matoi(command_argv[1]);
         int start = matoi(command_argv[2]);
         int size  = matoi(command_argv[3]);
         char *buffer = kcalloc(size);

         printf("checking read functionality of block driver...\n");
         block_read_arbitrary(dev, buffer, start, size);
         int i;
         if(samestr(as, "char"))
         {
            for(i = 0; i < size; i++)
            {
               printf("buffer[%3d (0x%03x)]: 0x%02x (%c)\n",
                         i, i, buffer[i], buffer[i]);
            }
         }
         else
         {
            printf("sorry, I didn't understand your argument '%s'\n", as);
            printf("acceptable prints:\n");
            printf("   char");
         }
         kfree(buffer);
      }
   }
   else if(samecmd(command, "blockwrite"))
   {
      if(command_nargs < 4 || command_nargs > 5)
      {
         printf("error: usage is `blockread` <dev> <start> <size> (<what>)\n");
         printf("<what> might be 'ones', 'zeros', or 'up' (default)\n");
      }
      else
      {
         char *what;
         if(command_nargs == 5)
         {
            what = command_argv[4];
         }
         else
         {
            what = "up";
         }
         int dev   = matoi(command_argv[1]);
         int start = matoi(command_argv[2]);
         int size  = matoi(command_argv[3]);
         char *buffer = kcalloc(size);
         MALLOC_CHECK(buffer);
         printf("checking write functionality of block driver...\n");
         int i;
         for(i = start; i < start+size; i++)
         {
            if(samestr(what, "up"))
            {
               buffer[i] = i/2;
            }
            else if(samestr(what, "zeros"))
            {
               buffer[i] = 0x00;
            }
            else if(samestr(what, "ones"))
            {
               buffer[i] = 0xff;
            }
            else // zeros
            {
               buffer[i] = 0x00;
            }
         }
         block_write(dev, buffer, start, start+size);
         asm volatile("wfi"); // will continue after PLIC interrupts us
         kfree(buffer);
         printf("write successful");
      }
   }
   else if(samecmd(command, "regions"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is 'region' <op>\n");
      }
      else
      {
         if(samestr(command_argv[1], "print"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `print` (no args)\n");
            }
            else
            {
               regions_print_list(global_regions_start);
            }
         }
         else if(samestr(command_argv[1], "default"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `default` (no args)\n");
            }
            else
            {
               execute_command("regions add 00 10 00 10 1");
               execute_command("regions add 10 20 10 20 2");
               execute_command("regions add 20 30 20 30 3");
               execute_command("regions add 30 40 30 40 4");
               execute_command("regions add 40 50 40 50 5");
               execute_command("regions add 50 60 50 60 6");
               execute_command("regions add 60 70 60 70 7");
               execute_command("regions add 70 80 70 80 8");
               execute_command("regions add 80 90 80 90 9");
               execute_command("regions add 90 100 90 100 10");
            }
         }
         else if(samestr(command_argv[1], "add"))
         {
            if(command_nargs != 7)
            {
               printf("error: usage is `add` <x_start> <x_end>");
               printf(" <y_start> <y_end> <value>\n");
            }
            else
            {
               uint32_t x_start;
               uint32_t x_end;
               uint32_t y_start;
               uint32_t y_end;
               int value;
               x_start = matoi(command_argv[2]);
               x_end   = matoi(command_argv[3]);
               y_start = matoi(command_argv[4]);
               y_end   = matoi(command_argv[5]);
               value   = matoi(command_argv[6]);
               region_llist_t *rl;
               rl = regions_new(x_start, x_end, y_start, y_end, value);
               if(regions_append(global_regions_start, rl))
               {
                  ERRORMSG("");
               }
               gpu_rectangle_t gr;
               uint32_t x_start_pixel;
               uint32_t x_end_pixel;
               uint32_t y_start_pixel;
               uint32_t y_end_pixel;
               x_start_pixel = gpu_percent_to_x_pixel(x_start);
               x_end_pixel   = gpu_percent_to_x_pixel(x_end);
               y_start_pixel = gpu_percent_to_y_pixel(y_start);
               y_end_pixel   = gpu_percent_to_y_pixel(y_end);
               gr.x = x_start_pixel;
               gr.y = y_start_pixel;
               gr.width  = x_end_pixel - x_start_pixel;
               gr.height = y_end_pixel - y_start_pixel;
               pixel_t new_col = {rand8(), rand8(), rand8(), 255};
               gpu_fill_rectangle(&(gpu_device.fb), &gr, new_col);
               gpu_transfer_and_flush_some(&gr);
            }
         }
         else
         {
            printf("I couldn't understand the arguments you specified\n");
            printf("possbile ops are:\n");
            printf("   - print\n");
            printf("   - add\n");
         }
      }
   }
   else if(samecmd(command, "gpu"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is 'gpu' <op>\n");
      }
      else
      {
         if(samestr(command_argv[1], "gdi"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `gdi` (no args)\n");
            }
            else
            {
               gpu_get_display_info_response_t* gpu_info;
               gpu_info = kcalloc(sizeof(gpu_get_display_info_response_t));
               MALLOC_CHECK(gpu_info);
      
               if(gpu_get_display_info(gpu_info))
               {
                  ERRORMSG("tester fail");
               }
               gpu_print_gdi_response(gpu_info);
               kfree(gpu_info);
            }
         }
         else if(samestr(command_argv[1], "init"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `init` (no args)\n");
            }
            else
            {
               gpu_init();
            }
         }
         else if(samestr(command_argv[1], "fill"))
         {
            if(command_nargs < 3)
            {
               printf("error: usage is `fill` <op>\n");
            }
            else
            {
               if(samestr(command_argv[2], "bmp"))
               {
                  gpu_fill_with_bmp_and_flush();
               }
               if(samestr(command_argv[2], "white"))
               {
                  gpu_fill_white(&(gpu_device.fb));
               }
               else
               {
                  printf("possible ops are:\n");
                  printf("  bmp\n");
                  printf("  white\n");
               }
            }
         }
         else if(samestr(command_argv[1], "flush"))
         {
            if(command_nargs < 3)
            {
               printf("error: usage is `flush` <op>\n");
            }
            else
            {
               if(samestr(command_argv[2], "all"))
               {
                  gpu_transfer_and_flush();
               }
               else if(samestr(command_argv[2], "some"))
               {
                  gpu_rectangle_t r;
                  r.x = 100;
                  r.y = 100;
                  r.width  = 100;
                  r.height = 100;
                  gpu_transfer_and_flush_some(&r);
               }
               else
               {
                  printf("possible ops are:\n");
                  printf("  all\n");
                  printf("  some\n");
               }
            }
         }
         else if(samestr(command_argv[1], "regions"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `regions` (no args)\n");
            }
            else
            {
               gpu_regions_test();
            }
         }
         else if(samestr(command_argv[1], "cursor"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `cursor` (no args)\n");
            }
            else
            {
               gpu_draw_from_cursor();
            }
         }
         else if(samestr(command_argv[1], "draw"))
         {
            if(command_nargs != 2)
            {
               printf("error: usage is `draw` (no args)\n");
            }
            else
            {
               gpu_draw_endless();
            }
         }
         else
         {
            printf("I couldn't understand the arguments you specified\n");
            printf("possbile ops are:\n");
            printf("   - gdi\n");
            printf("   - init\n");
            printf("   - fill\n");
            printf("   - flush\n");
            printf("   - cursor\n");
            printf("   - draw\n");
         }
      }
   }
   else if(samecmd(command, "minix3"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `minix3` <op>\n");
      }
      else
      {
         if(samestr(command_argv[1], "print"))
         {
            mx3_superblock_t *sb = minix3_get_superblock(mx3_block_dev);
            heap_size_of_allocation(sb);
            minix3_print_superblock(sb);
            kfree(sb);
         }
         else if(samestr(command_argv[1], "/"))
         {
            mx3_inode_t inode;
            minix3_get_inode(mx3_block_dev, 1, &inode);
            minix3_print_inode(&inode);
         }
         else if(samestr(command_argv[1], "ls/"))
         {
            mx3_inode_t inode;
            minix3_get_inode(mx3_block_dev, 1, &inode);
            minix3_ls(mx3_block_dev, &inode);
         }
         else if(samestr(command_argv[1], "zone"))
         {
            if(command_nargs < 3)
            {
               printf("error: which zone? `minix3 zone` <which_zone>\n");
            }
            else
            {
               uint32_t zone;
               zone = matoi(command_argv[2]);
               int occupied, in_range;
               occupied = minix3_is_zone_occupied(mx3_block_dev, zone);
               printf("zone %d occupied? %d\n", zone, occupied);
               in_range = minix3_is_zone_in_range(mx3_block_dev, zone);
               printf("zone %d in range? %d\n", zone, in_range);
            }
         }
         else if(samestr(command_argv[1], "inode"))
         {
            if(command_nargs < 3)
            {
               printf("error: which inode? `minix3 inode` <which_inode>\n");
            }
            else
            {
               uint32_t inode;
               inode = matoi(command_argv[2]);
               int occupied, in_range;
               occupied = minix3_is_inode_occupied(mx3_block_dev, inode);
               printf("inode %d occupied? %d\n", inode, occupied);
               in_range = minix3_is_inode_in_range(mx3_block_dev, inode);
               printf("inode %d in range? %d\n", inode, in_range);
            }
         }
         else if(samestr(command_argv[1], "avail"))
         {
            if(command_nargs < 3)
            {
               printf("error: which avail? `minix3 avail` <inode/zone>\n");
            }
            else
            {
               if(samestr(command_argv[2], "inode"))
               {
                  printf("available inode: %u\n",
                          minix3_get_available_inode(mx3_block_dev));
               }
               else if(samestr(command_argv[2], "zone"))
               {
                  printf("available zone: %u\n",
                          minix3_get_available_zone(mx3_block_dev));
               }
               else
               {
                  printf("only accepted arguments are:\n");
                  printf("   - inode\n");
                  printf("   - zone\n");
               }
            }
         }
         else if(samestr(command_argv[1], "fzones"))
         {
            printf("how many zones per minix3 file given block size of %u?",
                    mx3_block_size);
            printf(" %u.\n", minix3_get_file_max_zones(mx3_block_size));
            uint64_t zone_max;
            zone_max = (uint64_t)minix3_get_file_max_zones(mx3_block_size)*
                       mx3_block_size;
            printf("file size limit for zones?        %lu.\n", zone_max);
            printf("file size limit for size pointer? %u.\n", 0xffffffff);
         }
         else if(samestr(command_argv[1], "walker"))
         {
            if(command_nargs < 4)
            {
               printf("error: which avail? `minix3 walker` <inum> <target>\n");
            }
            else
            {
               uint32_t inum = matoi(command_argv[2]);
               uint32_t target = matoi(command_argv[3]);
               uint32_t size_remaining;
               mx3_inode_t inode;
               minix3_get_inode(mx3_block_dev, inum, &inode);
               uint64_t zone_addr;
               zone_addr = minix3_gzbars(mx3_block_dev, &inode, target,
                                         &size_remaining);
               printf("minix3 inode %u:\n", inum);
               printf("   zone %u is at %lu on device.\n", target, zone_addr);
               printf("   there is %u remaining size at start of zone\n",
                                  size_remaining);
            }
         }
         else
         {
            printf("I couldn't understand the arguments you specified\n");
            printf("possbile ops are:\n");
            printf("   print\n");
            printf("   /\n");
            printf("   ls\n");
            printf("   zone\n");
            printf("   inode\n");
            printf("   avail\n");
            printf("   fzones\n");
            printf("   walker\n");
         }
      }
   }
   else if(samecmd(command, "vfs"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is vfs <op>\n");
      }
      else
      {
         if(samestr(command_argv[1], "tree"))
         {
            vfs_print_cached_tree(v_dir_pwd);
         }
         else
         {
            printf("I couldn't understand the arguments you specified\n");
            printf("possible ops are:\n");
            printf("   tree");
         }
      }
   }
   else if(samecmd(command, "ls"))
   {
      vfs_ls(v_dir_pwd);
   }
   else if(samecmd(command, "inode"))
   {
      if(command_nargs < 2)
      {
         vfs_print_inode(v_dir_pwd, "");
      }
      else
      {
         vfs_print_inode(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "cd"))
   {
      if(command_nargs < 2)
      {
         // printf("error: usage is `cd` <target>\n");
         vfs_cd("");
      }
      else
      {
         vfs_cd(command_argv[1]);
      }
   }
   else if(samecmd(command, "touch"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `touch` <target>\n");
      }
      else
      {
         vfs_touch(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "rm"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `rm` <target>\n");
      }
      else
      {
         vfs_rm(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "mkdir"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `mkdir` <target>\n");
      }
      else
      {
         vfs_mkdir(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "rmdir"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `rmdir` <target>\n");
      }
      else
      {
         vfs_rmdir(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "cat"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `cat` <target>\n");
      }
      else
      {
         vfs_cat(v_dir_pwd, command_argv[1]);
      }
   }
   else if(samecmd(command, "append"))
   {
      if(command_nargs < 3)
      {
         printf("error: usage is `append` <string> <target>\n");
      }
      else
      {
         vfs_append(v_dir_pwd, command_argv[2],
                    command_argv[1], mstrlen(command_argv[1]));
      }
   }
   else if(samecmd(command, "bmp2file"))
   {
      if(command_nargs < 3)
      {
         printf("error: usage is `bmp2file` <which_dev> <target>\n");
      }
      else
      {
         int bmp_dev = matoi(command_argv[1]);
         vfs_bmp_to_file(v_dir_pwd, command_argv[2], bmp_dev);
      }
   }
   else if(samecmd(command, "file2screen"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `file2screen` <target>\n");
      }
      else
      {
         gpu_rectangle_t r;
         r.x = 0;
         r.y = 0;
         r.width = gpu_device.fb.width;
         r.height = gpu_device.fb.height;
         vfs_bmp_file_to_gpu(v_dir_pwd, command_argv[1], &r);
      }
   }
   else if(samecmd(command, "inputring"))
   {
      if(command_nargs < 2)
      {
         printf("error: usage is `inputring` <op>\n");
      }
      else
      {
         if(samestr(command_argv[1], "print"))
         {
            ie_ring_buffer_print(global_input_events_ring_buffer);
         }
         else if(samestr(command_argv[1], "taken"))
         {
            printf("occupied: %u\n", global_input_events_ring_buffer->occupied);
         }
         else if(samestr(command_argv[1], "pop"))
         {
            if(command_nargs < 3)
            {
               printf("error: how many to pop? Pass an extra argument\n");
            }
            int how_many = matoi(command_argv[2]);
            int i;
            input_event_t ev;
            for(i = 0; i < how_many; i++)
            {
               ev = ie_ring_buffer_pop(global_input_events_ring_buffer);
               input_print_event(&ev);
            }
         }
         else
         {
            printf("I couldn't understand the arguments you specified\n");
            printf("possbile ops are:\n");
            printf("   print\n");
            printf("   taken\n");
            printf("   pop\n");
         }
      }
   }
   else if(samecmd(command, "nargs"))
   {
      printf("Your command has %d args\n", command_nargs);
   }
   else if(samecmd(command, "argv"))
   {
      printf("Your command's arguments:\n");
      int i;
      for(i = 0; i < command_nargs; i++)
      {
         // doing it this way in case some goober puts %'s in their args
         printf("%s\n", command_argv[i]);
      }
   }
   else if(samecmd(command, "virtq"))
   {
      if(command_nargs != 2)
      {
         printf("error: usage is `virtq <device_code>`\n");
      }
      else
      {
         char *device = command_argv[1];
         if(samestr(device, "rng"))
         {
            rng_print_virtq();
         }
         else if(samestr(device, "block"))
         {
            block_print_virtq();
         }
         else if(samestr(device, "gpu"))
         {
            gpu_print_virtq();
         }
         else if(samestr(device, "input"))
         {
            input_print_virtq();
         }
         else
         {
            printf("sorry, I didn't understand your request.\n");
            printf("available devices are:\n");
            printf("   - rng\n");
            printf("   - block\n");
            printf("   - gpu\n");
            printf("   - input\n");
         }
      }
   }
   else if(samecmd(command, "irqs"))
   {
      pcie_show_irq_llist();
   }
   else if(samecmd(command, "pog"))
   {
      pog(); // !
   }
   else if(samecmd(command, "joeyD"))
   {
      joeyD();
   }
   else
   {
      printf("unrecognized command '%s'\n", command);
   }
   free_argv(command_argv, command_nargs);
}

