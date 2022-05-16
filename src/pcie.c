#include <pcie.h>
#include <stdint.h>
#include <printf.h>
#include <kmalloc.h>
#include <virtio.h>
#include <utils.h>
#include <rng.h>
#include <block.h>
#include <gpu.h>
#include <input.h>
#include <mmu.h>
#include <utils.h>
#include <plic.h>

PCIe_driver_llist_t *global_pcie_driver_llist = NULL;
PCIe_IRQ_llist_node_t *global_pcie_irq_llist = NULL;

PCIe_bar_list_t global_pcie_bars_occupied; // in BSS, so should be zero'ed out.


ecam_header_t *pcie_get_ecam_address(int bus, int device, int function, int reg)
{
   uint64_t bus_64 = bus & 0xff; // 8 bits
   uint64_t device_64 = device & 0x1f; // 5 bits
   uint64_t function_64 = function & 0x07; // 3 bits
   uint64_t reg_64 = reg & 0x3ff; // 10 bits
   uint64_t address= ((uint64_t)ECAM_BASE) | (bus_64 << 20) | (device_64 << 15) |
                                    (function_64 << 12) | (reg_64 << 2);
   return (ecam_header_t *)address;
}

int pcie_get_bus(volatile ecam_header_t *ec)
{
   uint64_t addr = (uint64_t)ec;
   uint64_t bus = (addr & (0xff << 20)) >> 20;
   return((int)bus);
}

int pcie_get_slot(volatile ecam_header_t *ec)
{
   uint64_t addr = (uint64_t)ec;
   uint64_t slot = (addr & (0x1f << 15)) >> 15;
   return (int)slot;
}

int pcie_ecam_to_irq(volatile ecam_header_t *ec)
{
   int bus  = pcie_get_bus(ec);
   int slot = pcie_get_slot(ec);
   int irq  = PLIC_INT_CODE_IRQ0 /* base */ + (bus+slot)%4 /*offset */;
   return irq;
}

void pcie_show_cycle_all_addresses(void)
{
   int bus;
   int slot;
   ecam_header_t *ecam_address;
   for(bus = 0; bus < 256; bus++)
   {
      for(slot = 0; slot < 32; slot++)
      {
         ecam_address = pcie_get_ecam_address(bus, slot, 0, 0);
         if(ecam_address->vendor_id != 0xffff)
         {
            // then there is a real device here
            printf("ecam address of bus %03d slot %03d is 0x%012lx\n",
                        bus, slot, ecam_address);
            printf("vendor id: 0x%04x\n", ecam_address->vendor_id);
            printf("device id: 0x%04x\n", ecam_address->device_id);
            printf("header type: %d ", ecam_address->header_type);
            if(ecam_address->header_type == 1) //bridge
            {
               printf("(bridge)\n");
            }
            else // device
            {
               printf("(device)\n");
               if(pcie_find_driver(ecam_address->vendor_id,
                                   ecam_address->device_id))
               {
                  printf("I know how to drive this device\n");
               }
               else
               {
                  printf("I do not know how to drive this device\n");
               }
               int i;
               for(i = 0; i < 6; i++)
               {
                  if(ecam_address->type0.bar[i] == 0)
                  {
                     // BAR unused
                     continue;
                  }
                  if(PCIE_BAR_IS_64_BIT(ecam_address->type0.bar[i]))
                  {
                     printf("64 bit bar[%d] is used (memory at 0x%016x)\n",
                              i,
                              ((uint64_t)
                               ecam_address->type0.bar[i] & 0xFFFFFFF0) +
                              ((uint64_t)
                               (ecam_address->type0.bar[i+1] & 0xFFFFFFF0)
                                 << 32)
                              );
                        i++;
                  }
                  else
                  {
                     printf("32 bit bar[%d] is used (memory at 0x%08x)\n",
                              i, ecam_address->type0.bar[i] & 0xFFFFFFF0);
                  }
               }
            }
            // we don't show BARs here. They're already set!
            // pcie_show_capabilities(ecam_address);
         }
      }
   }
}

void pcie_scan_and_init(void)
{
   int bus;
   int slot;
   ecam_header_t *ecam_address;

   pcie_init_drivers();

   for(bus = 0; bus < 256; bus++)
   {
      for(slot = 0; slot < 32; slot++)
      {
         ecam_address = pcie_get_ecam_address(bus, slot, 0, 0);
         if(ecam_address->vendor_id != 0xffff)
         {
            // then there is a real device here
            if(ecam_address->header_type == 1) //bridge
            {
               // printf("bridge on bus %03d slot %03d ec 0x%012lx\n",
               //         bus, slot, ecam_address);
               pcie_init_bridge(ecam_address, bus);
            }
            else // device
            {
               // check for driver
               if( pcie_find_driver(ecam_address->vendor_id,
                                   ecam_address->device_id))
               {
                  ;
                  // TODO: possibly move the driver and init-ing of BARS here?
               }
               else
               {
                  // printf("device on bus %03d slot %03d ec 0x%012lx\n",
                  //             bus, slot, ecam_address);
                  // printf("I do not know how to drive a connected device\n");
                  // printf("vendor id: 0x%04x\n", ecam_address->vendor_id);
                  // printf("device id: 0x%04x\n", ecam_address->device_id);
                  ;
               }

               // set BARS
               int i;
               for(i = 0; i < 6; i++)
               {
                  ecam_address->type0.bar[i] = -1;
                  if(ecam_address->type0.bar[i] == 0)
                  {
                     // BAR unused
                     continue;
                  }
                  // else BAR is used
                  uint64_t required_alignment;
                  if(PCIE_BAR_IS_64_BIT(ecam_address->type0.bar[i]))
                  {
                     if(i == 5) // should never happen. That means there
                                 // would have to be a bar[6]!
                     {
                        printf("Chief, something went wrong. Head on over");
                        printf("To %s, line %u to check it out\n",
                                 __FILE__, __LINE__);
                     }
                     ecam_address->type0.bar[i+1] = -1;
                     uint32_t upper_bits = ecam_address->type0.bar[i+1];
                     uint64_t bar_address;
                     bar_address = ecam_address->type0.bar[i];
                     bar_address |= ((uint64_t)upper_bits) << 32;
                     required_alignment = -(bar_address & ~0xFUL);
                     i++; // next BAR won't be used either, then
                  }
                  else
                  {
                     uint32_t bar_address;
                     bar_address = ecam_address->type0.bar[i];
                     required_alignment = (uint64_t) -(bar_address & ~0xFUL);
                  }
                  // RIP old method of checking. Why didn't this work? I'm
                  // keeping it here so ye might remember the mystery
                  // for yourself
                  /*
                  printf("\tdevice asked for 0x%08x\n",required_alignment);
                  printf("\t0x100000 mod required_aligment (0x%08x) = 0x%08x\n",
                              required_alignment, 0x100000%required_alignment);
                  if(0x100000 % required_alignment)
                  {
                     printf("error! This OS does not support required");
                     printf(" alignments beyond 0x100000!\n");
                     printf("\t(device asked for 0x%08x)\n",required_alignment);
                     printf("\t(0x100000 mod required_alignment = %08x)\n",
                                 0x100000%required_alignment);
                  
                  }*/
                  // error check
                  uint64_t bar_mask = -required_alignment;
                  if((0x100000 & bar_mask) != 0x100000)
                  {
                     printf("error! This OS does not support required");
                     printf(" alignments beyond 0x100000!\n");
                     printf("\t(device asked for 0x%08x)\n",required_alignment);
                     printf("\t(0x100000 mod required_alignment = %08x)\n",
                                 0x100000%required_alignment);
                  }
               }
               ecam_address->command_reg |= ECAM_COMMAND_MEMORY_SPACE_BIT;
               pcie_init_bars(ecam_address);
               // now run the driver if needed
               PCIe_driver_t * pcie_driver = 
                  pcie_find_driver(ecam_address->vendor_id,
                                    ecam_address->device_id);
               if(pcie_driver)
               {
                  pcie_driver->driver_func(pcie_driver);
               }
            }
         }
      }
   }
}

void pcie_init_bridge(volatile ecam_header_t *ec, int bus)
{
   // global, unset, but increments across function calls. Cool!
   static int pcie_bridge_bus_number = 1;

   ec->command_reg |= ECAM_COMMAND_MEMORY_SPACE_BIT;
   ec->command_reg |= ECAM_COMMAND_BUS_MASTER_BIT;

   ec->type1.memory_base  = 0x4000; // 0x4000_0000
   ec->type1.memory_limit = 0x4FFF; // ceiling of 0x4FFF_(....) (0x4FFF_FFFF)
   ec->type1.prefetch_memory_base  = 0x4000;
   ec->type1.prefetch_memory_limit = 0x4FFF;

   ec->type1.primary_bus_no = bus; // which our code sort-of assumes is zero
   ec->type1.secondary_bus_no = pcie_bridge_bus_number;
   ec->type1.subordinate_bus_no = pcie_bridge_bus_number;

   // what happens if we cross busses? As we learned in Office Hours, this
   // will cause a conflict. Imagine two wires hooked up to the same port.
   // Unpredictable behavior. But we are far from the need to deal with
   // this issue. Just add one to the bus number we use and move on!

   pcie_bridge_bus_number += 1;
}

// find the virtio specific capabilities for this device and add them in.
// similar to the SHOW function
// TODO: you coud modify this function to, instead of allocating several
//    allocations, to just go through each BAR and say: you're active,
//    who's on you? and go from there
void pcie_init_bars(volatile ecam_header_t *ec)
{
   if(ec->status_reg & ECAM_STATUS_CAPABILITIES_BIT)
   {
      // array of the offset corresponding to each config type.
      // index 0 should remain untouched.
      uint32_t config_types_offsets[6];
      uint32_t config_types_bars[6];
      PCIe_bar_allocation_t *bar_allocations;
      bar_allocations = kmalloc(sizeof(PCIe_bar_allocation_t) * 6);
      int i,j;
      for(i = 0; i < 6; i++)
      {
         config_types_offsets[i] = 0xFFFFFFFF;
         config_types_bars[i] = 0xFFFFFFFF;
         bar_allocations[i].which_bar = 0xFFFFFFFF;
         for(j = 0; j < GLOBAL_VIRTIO_BARS_OFFSETS_LEN; j++)
         {
            bar_allocations[i].virtio_register_offsets[j] = 0xFFFFFFFF;
         }
         bar_allocations[i].notes[0] = '\0';
      }

      unsigned long capabilities_base = (unsigned long)ec;
      ecam_capability_t *cp = (ecam_capability_t*)
                               (capabilities_base + ec->common.capes_pointer);
      while(cp != (ecam_capability_t*) capabilities_base)
      {
         cp = (ecam_capability_t *) (capabilities_base + cp->next_offset);
         // 0x09 is Vendor Specific (often VIRTIO)
         if(cp->id == 0x09)
         {
            virtio_pci_cap_t *vcp = (virtio_pci_cap_t*)cp;
            int index = vcp->config_type;
            uint32_t bar = vcp->bar;
            int offset = vcp->offset;
            config_types_offsets[index] = offset;
            config_types_bars[index] = bar;
         }
         // 0x10 is-PCI express
      }

      // found the capabilities!
      for(i = 0; i < 6; i++)
      {
         if (i == VIRTIO_CAP_CFG_PCI)
         {
            continue; // we don't care about this config for now.
         }
         uint32_t bar = config_types_bars[i];
         if (bar != 0xFFFFFFFF)
         {
            bar_allocations[bar].which_bar = bar;
            bar_allocations[bar].ec = ec;
            bar_allocations[bar].virtio_register_offsets[i] =
                                          config_types_offsets[i];
            sprintf(bar_allocations[bar].notes, "your code is messy, bud");
         }
      }
      for(i = 0; i < 6; i++)
      {
         pcie_add_to_bar_list(bar_allocations+i);
      }
      kfree(bar_allocations);
   }
   else
   {
      if(ec->vendor_id == 0x1b36 && ec->device_id == 0x0008)
      {
         // its just the root driver.
         ;
      }
      else
      {
         printf("there are NO capabilities for this ECAM (error!)\n");
      }
   }
}

void pcie_add_to_bar_list(PCIe_bar_allocation_t *bar_allocation)
{
   int i, j;
   uint32_t which_bar = bar_allocation->which_bar;
   volatile ecam_header_t *ec = bar_allocation->ec;
   if(which_bar == 0xFFFFFFFF)
   {
      return;
   }
   for(i = 0; i < 16; i++)
   {
      if(global_pcie_bars_occupied.vendorIDs[i] == 0)
      {
         break;
      }
   }
   global_pcie_bars_occupied.vendorIDs[i] = ec->vendor_id;
   global_pcie_bars_occupied.deviceIDs[i] = ec->device_id;
   global_pcie_bars_occupied.which_bar[i] = which_bar;
   global_pcie_bars_occupied.which_irq[i] = pcie_ecam_to_irq(ec);
   for(j = 0; j < GLOBAL_VIRTIO_BARS_OFFSETS_LEN; j++)
   {
      global_pcie_bars_occupied.virtio_register_offsets[i][j] = 
                              bar_allocation->virtio_register_offsets[j];
   }
   mstrcpy(global_pcie_bars_occupied.notes[i], bar_allocation->notes);


   // consider making a macro for this
   if(PCIE_BAR_IS_64_BIT(ec->type0.bar[which_bar]))
   {
      ec->type0.bar[which_bar+1] = 0;
   }
   ec->type0.bar[which_bar] = 0x40000000+0x00100000*i;
   // printf("setting bar %d to 0x%08x\n",which_bar,ec->type0.bar[which_bar]);
}

void pcie_show_bar_list()
{
   int i, j;
   for(i = 0; i < 16; i++)
   {
      // possible TODO: add a used field instead of checking here.
      if(global_pcie_bars_occupied.vendorIDs[i] == 0)
      {
         continue;
      }
      printf("memory address 0x%08x:\n", 0x40000000+0x00100000*i);
      printf("   vendor id: 0x%04x ", global_pcie_bars_occupied.vendorIDs[i]);
      if(global_pcie_bars_occupied.vendorIDs[i] == 0x1af4)
      {
         printf("(virtio)");
      }
      else
      {
         printf("(unknown)");
      }
      printf("\n");
      printf("   device id: 0x%04x ", global_pcie_bars_occupied.deviceIDs[i]);
      if(global_pcie_bars_occupied.deviceIDs[i] == 0x1050)
      {
         printf("(gpu)");
      }
      else if(global_pcie_bars_occupied.deviceIDs[i] == 0x1052)
      {
         printf("(input)");
      }
      else if(global_pcie_bars_occupied.deviceIDs[i] == 0x1044)
      {
         printf("(rng)");
      }
      else if(global_pcie_bars_occupied.deviceIDs[i] == 0x1042)
      {
         printf("(block)");
      }
      else
      {
         printf("(unknown)");
      }
      printf("\n");
      printf("   which bar: 0x%04x\n", global_pcie_bars_occupied.which_bar[i]);
      printf("   which irq:   %4d\n",  global_pcie_bars_occupied.which_irq[i]);
      printf("   offsets:\n");
      for(j = 0; j < GLOBAL_VIRTIO_BARS_OFFSETS_LEN; j++)
      {
         uint32_t offset;
         char *cape_name;
         offset = global_pcie_bars_occupied.virtio_register_offsets[i][j];
         if(offset == 0xFFFFFFFF)
         {
            continue;
         }
         GET_CAP_NAME(cape_name,j);
         printf("      %s: 0x%08x\n",cape_name,offset);
      }
      if (global_pcie_bars_occupied.notes[i][0])
      {
         printf("   notes: '%s'\n", global_pcie_bars_occupied.notes[i]);
      }
      printf("\n");
   }
}

void pcie_show_capabilities(volatile ecam_header_t *ec)
{
   if(ec->status_reg & ECAM_STATUS_CAPABILITIES_BIT)
   {
      printf("there are capabilities for this ECAM\n");
      unsigned long capabilities_base = (unsigned long)ec;
      printf("capabilities start at 0x%012lx\n", capabilities_base);
      ecam_capability_t *cp = (ecam_capability_t*)
                               (capabilities_base + ec->common.capes_pointer);
      printf("first capability at 0x%012lx\n", (unsigned long)cp);
      while(cp != (ecam_capability_t*) capabilities_base)
      {
         printf("capability of id 0x%02x (next at offset 0x%02x)\n",
                 cp->id, cp->next_offset);
         cp = (ecam_capability_t *) (capabilities_base + cp->next_offset);
         // 0x09 is Vendor Specific (often VIRTIO)
         if(cp->id == 0x09)
         {
            virtio_show_vendor_specific_cap((virtio_pci_cap_t *)cp);
         }
         // 0x10 is-PCI express
      }
   }
   else
   {
      printf("there are NO capabilities for this ECAM\n");
   }
   printf("\n");
}

void pcie_register_driver(uint16_t vendor_id, uint16_t device_id,
                          int (*driver_func)(PCIe_driver_t *device))
{
   if(pcie_find_driver(vendor_id,device_id))
   {
      // then do not add to the driver list
      printf("[%s (fucntion pcie_register_driver)]:\n",  __FILE__);
      printf("    I have already registered a driver");
      printf("with vendor 0x%04x, driver 0x%04x\n", vendor_id, device_id);
      return;
   }
   // else
   PCIe_driver_llist_t *pl;
   pl = kmalloc(sizeof(PCIe_driver_llist_t));
   MALLOC_CHECK(pl);
   // populate driver fields
   (pl->driver).vendor_id = vendor_id;
   (pl->driver).device_id = device_id;
   (pl->driver).driver_func = driver_func;
   // and add the element to the start of the linked list (easier this way)
   pl->next = global_pcie_driver_llist;
   global_pcie_driver_llist = pl;
}

PCIe_driver_t * pcie_find_driver(uint16_t vendor_id, uint16_t device_id)
{
   PCIe_driver_llist_t *pl;
   pl = global_pcie_driver_llist;
   for( ; pl != NULL; pl = pl->next)
   {
      if((pl->driver).vendor_id == vendor_id &&
         (pl->driver).device_id == device_id)
      {
         return &(pl->driver); // which is probably the same as just pl
      }
   }
   // else driver not found
   return NULL;
}

// 1af4: virtIO vendor ID
void pcie_init_drivers()
{
   pcie_register_driver(0x1af4, 0x1044, virtio_pcie_rng_driver);
   pcie_register_driver(0x1af4, 0x1042, virtio_pcie_block_driver);
   pcie_register_driver(0x1af4, 0x1050, virtio_pcie_gpu_driver);
   pcie_register_driver(0x1af4, 0x1052, virtio_pcie_input_driver);
}

void pcie_show_irq_llist()
{
   printf("PCIE IRQ list\n");
   if(global_pcie_irq_llist == NULL)
   {
      printf("No nodes yet!\n");
      return;
   }
   PCIe_IRQ_llist_node_t *p;
   printf("   IRQ, function address (note)\n");
   for(p = global_pcie_irq_llist; p != NULL; p = p->next)
   {
      printf("   %02d , %016lx (%s)\n",p->which_irq, p->irq_handler, p->note);
   }
}

void pcie_register_irq(int which_irq, void (*irq_function)(), char *note)
{
   PCIe_IRQ_llist_node_t *new_irq = kmalloc(sizeof(PCIe_IRQ_llist_node_t));
   MALLOC_CHECK(new_irq);
   // no need to free new_irq, we don't hot-plug.
   // It'll live for the whole OS lifetime.

   new_irq->which_irq = which_irq;
   new_irq->irq_handler = irq_function;
   mstrcpy(new_irq->note,note);
   new_irq->next = NULL;
   // now add the node to the list
   if(global_pcie_irq_llist == NULL)
   {
      global_pcie_irq_llist = new_irq;
   }
   else
   {
      PCIe_IRQ_llist_node_t *p = global_pcie_irq_llist;
      for(; p->next != NULL; p = p->next)
      {
         ;
      }
      p->next = new_irq;
   }
   return;
}

void pcie_handle_irq(int which_irq)
{
   PCIe_IRQ_llist_node_t *p;
   for(p = global_pcie_irq_llist; p != NULL; p = p->next)
   {
      if(p->which_irq == which_irq)
      {
         p->irq_handler();
      }
   }
   return;
}
