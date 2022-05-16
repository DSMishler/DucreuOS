#include <minix3.h>
#include <block.h>
#include <utils.h>
#include <printf.h>
#include <stdint.h>
#include <kmalloc.h>


int mx3_block_dev;
int mx3_fs_exists;
uint16_t mx3_block_size;

int minix3_is_zone_occupied( int which_dev, uint32_t zone_num)
{
   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   int occupied;
   uint64_t which_byte;
   uint64_t which_bit;
   uint32_t bitmap_zone_num = zone_num-sb->first_data_zone;

   which_byte = bitmap_zone_num / 8;
   which_bit  = bitmap_zone_num % 8;

   uint64_t zone_blocks_begin = 0;
   zone_blocks_begin += MX3_BOOT_BLOCK_SIZE + mx3_block_size;
   zone_blocks_begin += sb->imap_blocks * mx3_block_size;

   uint8_t zone_byte;
   uint64_t zone_byte_addr = zone_blocks_begin + which_byte;

   block_read_arbitrary(mx3_block_dev, (char*)&zone_byte, zone_byte_addr, 1);
   // printf("zone byte: 0x%02x\n", zone_byte);

   uint8_t bitmask = 0x01 << which_bit;
   occupied  = (((zone_byte & bitmask) >> which_bit));

   if(zone_num < sb->first_data_zone)
   {
      ERRORMSG("zone invalid (too small)");
      occupied = 1;
   }
   kfree(sb);
   return occupied;
}

int minix3_is_inode_occupied(int which_dev, uint32_t inode_num)
{
   if(inode_num == 0)
   {
      ERRORMSG("inode 0 invalid");
      return 0;
   }
   uint32_t bitmap_inode_num = inode_num - 1;

   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   int occupied;
   uint64_t which_byte;
   uint64_t which_bit;

   which_byte = bitmap_inode_num / 8;
   which_bit  = bitmap_inode_num % 8;

   uint64_t imap_blocks_begin = 0;
   imap_blocks_begin += MX3_BOOT_BLOCK_SIZE + mx3_block_size;

   uint8_t imap_byte;
   uint64_t imap_byte_addr = imap_blocks_begin + which_byte;

   block_read_arbitrary(mx3_block_dev, (char*)&imap_byte, imap_byte_addr, 1);
   // printf("imap byte: 0x%02x\n", imap_byte);

   uint8_t bitmask = 0x01 << which_bit;
   occupied  = (((imap_byte & bitmask) >> which_bit));

   kfree(sb);
   return occupied;
}

int minix3_is_zone_in_range( int which_dev, uint32_t zone_num)
{
   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   int in_range = 0;
   uint32_t bitmap_zone_num = zone_num-sb->first_data_zone;

   in_range = (bitmap_zone_num >= sb->num_zones ? 0 : 1);

   if(zone_num < sb->first_data_zone)
   {
      ERRORMSG("zone invalid (too small)");
      in_range = 0;
   }
   kfree(sb);
   return in_range;
}

int minix3_is_inode_in_range(int which_dev, uint32_t inode_num)
{
   if(inode_num == 0)
   {
      ERRORMSG("inode 0 invalid");
      return 0;
   }
   uint32_t bitmap_inode_num = inode_num - 1;
   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   int in_range = 0;

   // not >= here, since inode 0 does not exist!
   in_range = (bitmap_inode_num > sb->num_inodes ? 0 : 1);

   kfree(sb);
   return in_range;
}

int minix3_set_zone_occupied(int which_dev, uint32_t zone_num)
{
   return (minix3_set_zone_as(which_dev, zone_num, 1));
}

int minix3_set_zone_unoccupied(int which_dev, uint32_t zone_num)
{
   return (minix3_set_zone_as(which_dev, zone_num, 0));
}

int minix3_set_zone_as( int which_dev, uint32_t zone_num, int as)
{
   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   if(zone_num < sb->first_data_zone)
   {
      ERRORMSG("zone too small");
      kfree(sb);
      return 5;
   }
   uint64_t which_byte;
   uint64_t which_bit;
   uint32_t bitmap_zone_num = zone_num-sb->first_data_zone;

   which_byte = bitmap_zone_num / 8;
   which_bit  = bitmap_zone_num % 8;

   uint64_t zone_blocks_begin = 0;
   zone_blocks_begin += MX3_BOOT_BLOCK_SIZE + mx3_block_size;
   zone_blocks_begin += sb->imap_blocks * mx3_block_size;

   uint8_t zone_byte;
   uint64_t zone_byte_addr = zone_blocks_begin + which_byte;

   block_read_arbitrary(mx3_block_dev, (char*)&zone_byte, zone_byte_addr, 1);

   uint8_t bitmask = 0x01 << which_bit;
   if(as) // as == 1
   {
      if(minix3_is_zone_occupied(which_dev, zone_num))
      {
         ERRORMSG("setting occupied of already occupied zone");
      }
      else if(!(minix3_is_zone_in_range(which_dev, zone_num)))
      {
         ERRORMSG("setting occupied of out of range zone");
      }
      else
      {
         zone_byte |= bitmask;
      }
   }
   else
   {
      if(!(minix3_is_zone_occupied(which_dev, zone_num)))
      {
         ERRORMSG("setting unoccupied of already unoccupied zone");
         printf("zone_num: %u\n", zone_num);
      }
      else if(!(minix3_is_zone_in_range(which_dev, zone_num)))
      {
         ERRORMSG("setting unoccupied of out of range zone");
      }
      else
      {
         zone_byte &= ~bitmask;
      }
   }

   block_write_arbitrary(mx3_block_dev, (char*)&zone_byte, zone_byte_addr, 1);

   kfree(sb);
   return 0;
}

int minix3_set_inode_occupied(int which_dev, uint32_t inode_num)
{
   return (minix3_set_inode_as(which_dev, inode_num, 1));
}

int minix3_set_inode_unoccupied(int which_dev, uint32_t inode_num)
{
   return (minix3_set_inode_as(which_dev, inode_num, 0));
}

int minix3_set_inode_as( int which_dev, uint32_t inode_num, int as)
{
   if(inode_num == 0)
   {
      ERRORMSG("inode 0 invalid");
      return 5;
   }
   uint32_t bitmap_inode_num = inode_num - 1;
   // the inode num in the bitmap is different than the inode num we're trying
   // to set

   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   uint64_t which_byte;
   uint64_t which_bit;

   which_byte = bitmap_inode_num / 8;
   which_bit  = bitmap_inode_num % 8;

   uint64_t inode_blocks_begin = 0;
   inode_blocks_begin += MX3_BOOT_BLOCK_SIZE + mx3_block_size;

   uint8_t inode_byte;
   uint64_t inode_byte_addr = inode_blocks_begin + which_byte;

   block_read_arbitrary(mx3_block_dev, (char*)&inode_byte, inode_byte_addr, 1);

   uint8_t bitmask = 0x01 << which_bit;
   if(as) // as == 1
   {
      if(minix3_is_inode_occupied(which_dev, inode_num))
      {
         ERRORMSG("setting occupied of already occupied inode");
      }
      else if(!(minix3_is_inode_in_range(which_dev, inode_num)))
      {
         ERRORMSG("setting occupied of out of rnage inode");
      }
      else
      {
         inode_byte |= bitmask;
      }
   }
   else
   {
      if(!(minix3_is_inode_occupied(which_dev, inode_num)))
      {
         ERRORMSG("setting unoccupied of already unoccupied inode");
      }
      else if(!(minix3_is_inode_in_range(which_dev, inode_num)))
      {
         ERRORMSG("setting unoccupied of out of rnage inode");
      }
      else
      {
         inode_byte &= ~bitmask;
      }
   }

   block_write_arbitrary(mx3_block_dev, (char*)&inode_byte, inode_byte_addr, 1);

   kfree(sb);
   return 0;
}

void minix3_set_zone_to_zero(int which_dev, uint32_t which_zone)
{
   uint64_t zone_baddr = mx3_block_size * which_zone;
   char *buffer = kcalloc(mx3_block_size); /* kcalloc zeroes for us */
   block_write_arbitrary(which_dev, buffer, zone_baddr, mx3_block_size);
   kfree(buffer);
}


uint32_t minix3_get_available_zone(int which_dev)
{
   static uint32_t test_available_zone_num = 0;
   // TODO: this seems a little junky. Can we fix it?
   if(test_available_zone_num == 0)
   {
      mx3_superblock_t *sb = minix3_get_superblock(which_dev);
      test_available_zone_num = sb->first_data_zone;
      kfree(sb);
   }
   while(1)
   {
      if(!(minix3_is_zone_in_range(which_dev,test_available_zone_num)))
      {
         // TODO: add in something to make sure loop this doesn't happen twice
         // (full file system)
         DEBUGMSG("very rare event");
         test_available_zone_num = 0;
      }
      if(!(minix3_is_zone_occupied(which_dev, test_available_zone_num)))
      {
         minix3_set_zone_to_zero(which_dev, test_available_zone_num);
         return test_available_zone_num;
      }
      test_available_zone_num += 1;
   }
}

uint32_t minix3_get_available_inode(int which_dev)
{
   static uint32_t test_available_inode_num = 1;
   while(1)
   {
      if(!(minix3_is_inode_in_range(which_dev,test_available_inode_num)))
      {
         DEBUGMSG("very rare event");
         test_available_inode_num = 1;
      }
      if(!(minix3_is_inode_occupied(which_dev, test_available_inode_num)))
      {
         return test_available_inode_num;
      }
      test_available_inode_num += 1;
   }
}

uint32_t minix3_get_file_max_zones(uint16_t fs_block_size)
{
   uint32_t max_zones;
   uint32_t ppz = fs_block_size / 4; /* pointers per zone */
   /* minix3 has 7 direct, 1 indirect, 2 doubly indirect, 3 triply indirect */
   max_zones = 7 + ppz + ppz*ppz + ppz*ppz*ppz; 
   return max_zones;
}

uint32_t minix3_get_last_file_zone(int which_dev, mx3_inode_t *f_inode)
{
   uint32_t zone_num;
   if(f_inode->size == 0)
   {
      return 0;
   }
   zone_num = (f_inode->size - 1) / mx3_block_size; // first guess
   uint32_t remaining_size;
   minix3_gzbars(which_dev, f_inode, zone_num, &remaining_size);
   /* maybe we undershot. Double check */
   while(remaining_size > mx3_block_size)
   {
      DEBUGMSG("undershot");
      printf("remaining_size: %u\n", remaining_size);
      zone_num ++;
      minix3_gzbars(which_dev, f_inode, zone_num, &remaining_size);
   }
   return zone_num;
}

uint64_t minix3_append_zone(int which_dev, mx3_inode_t *f_inode)
{
   uint32_t next_zone;
   if(f_inode->size == 0)
   {
      next_zone = 0;
   }
   else
   {
      next_zone = minix3_get_last_file_zone(which_dev, f_inode) + 1;
   }
   uint32_t new_zone = minix3_get_available_zone(which_dev);
   minix3_set_zone_occupied(which_dev, new_zone);
   uint64_t new_baddr;
   new_baddr = mx3_block_size * 
               minix3_set_zone_to(which_dev, f_inode, next_zone, new_zone);
   return new_baddr;
}

/* gzbars: get zone baddr and remaining size */
uint64_t minix3_gzbars(int which_dev,
                       mx3_inode_t *f_inode,
                       uint32_t zone_index,
                       uint32_t *remaining_size)
{
   return ((uint64_t) mx3_block_size * 
           minix3_zone_walker(which_dev,f_inode,zone_index,remaining_size,0,0));
          
}

uint64_t minix3_get_zone_baddr(int which_dev,
                               mx3_inode_t *f_inode,
                               uint32_t zone_index)
{
   uint32_t padding;
   return ((uint64_t) mx3_block_size *
            minix3_zone_walker(which_dev,f_inode,zone_index,&padding,0,0));
}

uint32_t minix3_get_zone(int which_dev,
                         mx3_inode_t *f_inode,
                         uint32_t zone_index)
{
   uint32_t padding;
   return (minix3_zone_walker(which_dev,f_inode,zone_index,&padding,0,0));
}

/* returns the zone */
uint32_t minix3_set_zone_to(int which_dev, mx3_inode_t *f_inode,
                            uint32_t zone_index, uint32_t target)
{
   uint32_t padding;
   return (minix3_zone_walker(which_dev, f_inode, zone_index,
                              &padding, 1, target));
}


/* clear_zone also takes care of the zone bitmap */
void minix3_clear_zone(int which_dev, mx3_inode_t *f_inode, uint32_t zone_index)
{
   uint32_t cleared_zone;
   cleared_zone = minix3_get_zone(which_dev, f_inode, zone_index);
   minix3_set_zone_unoccupied(which_dev, cleared_zone);
   /* now set cleared zone to what the zone will be after clearing */
   cleared_zone = minix3_set_zone_to(which_dev, f_inode, zone_index, 0);
   if(cleared_zone != 0)
   {
      ERRORMSG("failed to clear zone"); /* address returned was nonzero */
   }
   return;
}



/* note: this is not the zone number, but rather the number of zones */
/* returns the zone pointed to at the index. Might be zero */
/* set_zone_to: if needed, set the zone pointer to this zone */
uint32_t minix3_zone_walker(int which_dev,
                            mx3_inode_t *f_inode,
                            uint32_t zone_index,
                            uint32_t *fill_remaining_size,
                            int do_set_zone,
                            uint32_t set_zone_to)
{
   uint32_t remaining_size = f_inode->size;
   // TODO: remove this hard-coding to allow for multiple mx3 devices
   uint16_t fs_block_size = mx3_block_size;
   uint32_t ppz = fs_block_size / 4; /* pointers per zone */
   uint32_t max_zones = minix3_get_file_max_zones(fs_block_size);
   uint32_t i, j, k, l; /* i will be the file's zone index */
   char *buffer_I, *buffer_II, *buffer_III;
   /* baddr: block address */
   uint64_t I_zone_baddr, II_zone_baddr, III_zone_baddr;
   uint32_t return_zone;
   uint32_t next_zone;
   uint32_t *zones, *Izones, *IIzones;
   char *zeroes;
   zeroes = kcalloc(fs_block_size);
   buffer_I = buffer_II = buffer_III = NULL;
   for(i = 0; i < MX3_IDZONE; i++)
   {
      if(i >= zone_index)
      {
         if(do_set_zone)
         {
            if(f_inode->zones[i])
            {
               if(set_zone_to == 0)
               {
                  ;
               }
               else
               {
                  ERRORMSG("overwriting zone");
               }
            }
            f_inode->zones[i] = set_zone_to;
         }
         return_zone = f_inode->zones[i];
         goto freeall;
      }
      else if(f_inode->zones[i])
      {
         remaining_size -= (fs_block_size < remaining_size ?
                            fs_block_size : remaining_size);
      }
   }
   /* indirect */
   if(i == zone_index && do_set_zone)
   {
      if(f_inode->zones[MX3_IDZONE])
      {
         if(set_zone_to == 0)
         {
            ;
         }
         else
         {
            ERRORMSG("overwriting zone");
         }
      }
      next_zone = minix3_get_available_zone(which_dev);
      minix3_set_zone_occupied(which_dev, next_zone);
      block_write_arbitrary(which_dev, zeroes,
                            next_zone * fs_block_size, fs_block_size);

      f_inode->zones[MX3_IDZONE] = next_zone;
   }
   buffer_I = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IDZONE])
   {
      I_zone_baddr = f_inode->zones[MX3_IDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_I, I_zone_baddr, fs_block_size);
      zones = (uint32_t *)buffer_I;
      for(j = 0; j < ppz; j++,i++)
      {
         if(i >= zone_index)
         {
            if(do_set_zone)
            {
               if(i != zone_index)
               {
                  ERRORMSG("managed to pass the zone you wanted");
               }
               if(zones[j])
               {
                  if(set_zone_to == 0)
                  {
                     ;
                  }
                  else
                  {
                     ERRORMSG("overwriting zone");
                     printf("indirect zone %d overwritten from %u to %u\n",
                             j, zones[j], set_zone_to);
                     printf("(the indirect zone you were using is %u)\n",
                             f_inode->zones[MX3_IDZONE]);
                  }
               }
               zones[j] = set_zone_to;
               block_write_arbitrary(which_dev, buffer_I,
                                     I_zone_baddr, fs_block_size);
            }
            return_zone = zones[j];
            goto freeall;
         }
         else if(zones[j])
         {
            remaining_size -= (fs_block_size < remaining_size ?
                               fs_block_size : remaining_size);
         }
      }
   }
   else
   {
      i += ppz;
   }
   /* doubly indirect */
   if(i == zone_index && do_set_zone)
   {
      if(f_inode->zones[MX3_IIDZONE])
      {
         if(set_zone_to == 0)
         {
            ;
         }
         else
         {
            ERRORMSG("overwriting zone");
         }
      }
      next_zone = minix3_get_available_zone(which_dev);
      minix3_set_zone_occupied(which_dev, next_zone);
      block_write_arbitrary(which_dev, zeroes,
                            next_zone * fs_block_size, fs_block_size);
      f_inode->zones[MX3_IIDZONE] = next_zone;
   }
   buffer_II = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IIDZONE])
   {
      II_zone_baddr = f_inode->zones[MX3_IIDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_II,
                           II_zone_baddr, fs_block_size);
      Izones = (uint32_t *)buffer_II;
      for(k = 0; k < ppz; k++)
      {
         if(i == zone_index && do_set_zone)
         {
            if(Izones[k])
            {
               if(set_zone_to == 0)
               {
                  ;
               }
               else
               {
                  ERRORMSG("overwriting zone");
               }
            }
            next_zone = minix3_get_available_zone(which_dev);
            minix3_set_zone_occupied(which_dev, next_zone);
            block_write_arbitrary(which_dev, zeroes,
                                  next_zone * fs_block_size, fs_block_size);
            Izones[k] = next_zone;
            block_write_arbitrary(which_dev, buffer_II,
                                  II_zone_baddr, fs_block_size);
         }
         if(Izones[k])
         {
            I_zone_baddr = Izones[k]*fs_block_size;
            block_read_arbitrary(which_dev, buffer_I,
                                 I_zone_baddr, fs_block_size);
            zones = (uint32_t *)buffer_I;
            for(j = 0; j < ppz; j++,i++)
            {
               if(i >= zone_index)
               {
                  if(do_set_zone)
                  {
                     if(i != zone_index)
                     {
                        ERRORMSG("managed to pass the zone you wanted");
                     }
                     if(zones[j])
                     {
                        if(set_zone_to == 0)
                        {
                           ;
                        }
                        else
                        {
                           ERRORMSG("overwriting zone");
                        }
                     }
                     zones[j] = set_zone_to;
                     block_write_arbitrary(which_dev, buffer_I,
                                           I_zone_baddr, fs_block_size);
                  }
                  return_zone = zones[j];
                  goto freeall;
               }
               if(zones[j])
               {
                  remaining_size -= (fs_block_size < remaining_size ?
                                     fs_block_size : remaining_size);
               }
            }
         }
         else
         {
            i += ppz;
         }
      }
   }
   else
   {
      i += ppz*ppz;
   }
   /* triply indirect */
   if(i == zone_index && do_set_zone)
   {
      if(f_inode->zones[MX3_IIIDZONE])
      {
         if(set_zone_to == 0)
         {
            ;
         }
         else
         {
            ERRORMSG("overwriting zone");
         }
      }
      next_zone = minix3_get_available_zone(which_dev);
      minix3_set_zone_occupied(which_dev, next_zone);
      block_write_arbitrary(which_dev, zeroes,
                            next_zone * fs_block_size, fs_block_size);
      f_inode->zones[MX3_IIIDZONE] = next_zone;
   }
   buffer_III = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IIIDZONE])
   {
      III_zone_baddr = f_inode->zones[MX3_IIIDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_III,
                           III_zone_baddr, fs_block_size);
      IIzones = (uint32_t *)buffer_III;
      for(l = 0; l < ppz; l++)
      {
         if(i == zone_index && do_set_zone)
         {
            if(IIzones[l])
            {
               if(set_zone_to == 0)
               {
                  ;
               }
               else
               {
                  ERRORMSG("overwriting zone");
               }
            }
            next_zone = minix3_get_available_zone(which_dev);
            minix3_set_zone_occupied(which_dev, next_zone);
            block_write_arbitrary(which_dev, zeroes,
                                  next_zone * fs_block_size, fs_block_size);
            IIzones[l] = next_zone;
            block_write_arbitrary(which_dev, buffer_III,
                                  III_zone_baddr, fs_block_size);
         }
         if(IIzones[l])
         {
            II_zone_baddr = f_inode->zones[MX3_IIDZONE]*fs_block_size;
            block_read_arbitrary(which_dev, buffer_II,
                                 II_zone_baddr, fs_block_size);
            Izones = (uint32_t *)buffer_II;
            for(k = 0; k < ppz; k++)
            {
               if(i == zone_index && do_set_zone)
               {
                  if(Izones[k])
                  {
                     if(set_zone_to == 0)
                     {
                        ;
                     }
                     else
                     {
                        ERRORMSG("overwriting zone");
                     }
                  }
                  next_zone = minix3_get_available_zone(which_dev);
                  minix3_set_zone_occupied(which_dev, next_zone);
                  block_write_arbitrary(which_dev, zeroes,
                                     next_zone * fs_block_size, fs_block_size);
                  Izones[k] = next_zone;
                  block_write_arbitrary(which_dev, buffer_II,
                                        II_zone_baddr, fs_block_size);
               }
               if(Izones[k])
               {
                  I_zone_baddr = Izones[k]*fs_block_size;
                  block_read_arbitrary(which_dev, buffer_I,
                                       I_zone_baddr, fs_block_size);
                  zones = (uint32_t *)buffer_I;
                  for(j = 0; j < ppz; j++,i++)
                  {
                     if(i >= zone_index)
                     {
                        if(do_set_zone)
                        {
                           if(i != zone_index)
                           {
                              ERRORMSG("managed to pass the zone you wanted");
                           }
                           if(zones[j])
                           {
                              if(set_zone_to == 0)
                              {
                                 ;
                              }
                              else
                              {
                                 ERRORMSG("overwriting zone");
                              }
                           }
                           zones[j] = set_zone_to;
                           block_write_arbitrary(which_dev, buffer_I,
                                                 I_zone_baddr, fs_block_size);
                        }
                        return_zone = zones[j];
                        goto freeall;
                     }
                     if(zones[j])
                     {
                        remaining_size -= (fs_block_size < remaining_size ?
                                           fs_block_size : remaining_size);
                     }
                  }
               }
               else
               {
                  i += ppz;
               }
            }
         }
         else
         {
            i += ppz*ppz;
         }
      }
   }
   else
   {
      i += ppz*ppz*ppz;
   }

   freeall:
   if(i > zone_index)
   {
      ERRORMSG("zone not available");
      printf("i = %u\tzone index = %u\n", i, zone_index);
   }
   if(i >= max_zones)
   {
      ERRORMSG("file full");
   }
   *fill_remaining_size = remaining_size;
   kfree(zeroes);
   kfree(buffer_I);
   kfree(buffer_II);
   kfree(buffer_III);
   return return_zone;
}

/* the zone walker got too big. We did this walk separately */
void minix3_clear_all_zones(int which_dev, mx3_inode_t *f_inode)
{
   uint32_t remaining_size = f_inode->size;
   // TODO: remove this hard-coding to allow for multiple mx3 devices
   uint16_t fs_block_size = mx3_block_size;
   uint32_t ppz = fs_block_size / 4; /* pointers per zone */
   uint32_t i, j, k, l; /* i will be the file's zone index */
   char *buffer_I, *buffer_II, *buffer_III;
   /* baddr: block address */
   uint64_t I_zone_baddr, II_zone_baddr, III_zone_baddr;
   uint32_t *zones, *Izones, *IIzones;
   buffer_I = buffer_II = buffer_III = NULL;
   for(i = 0; i < MX3_IDZONE; i++)
   {
      if(f_inode->zones[i])
      {
         remaining_size -= (fs_block_size < remaining_size ?
                            fs_block_size : remaining_size);
         minix3_set_zone_unoccupied(which_dev, f_inode->zones[i]);
         f_inode->zones[i] = 0;
      }
   }
   if(remaining_size == 0)
   {
      goto clear_all_end;
   }
   /* indirect */
   buffer_I = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IDZONE])
   {
      I_zone_baddr = f_inode->zones[MX3_IDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_I, I_zone_baddr, fs_block_size);
      zones = (uint32_t *)buffer_I;
      for(j = 0; j < ppz; j++,i++)
      {
         if(zones[j])
         {
            remaining_size -= (fs_block_size < remaining_size ?
                               fs_block_size : remaining_size);
            minix3_set_zone_unoccupied(which_dev, zones[j]);
            zones[j] = 0;
         }
      }
      minix3_set_zone_unoccupied(which_dev, f_inode->zones[MX3_IDZONE]);
      f_inode->zones[MX3_IDZONE] = 0;
   }
   else
   {
      i += ppz;
   }
   /* doubly indirect */
   if(remaining_size == 0)
   {
      goto clear_all_end;
   }
   buffer_II = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IIDZONE])
   {
      II_zone_baddr = f_inode->zones[MX3_IIDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_II,
                           II_zone_baddr, fs_block_size);
      Izones = (uint32_t *)buffer_II;
      for(k = 0; k < ppz; k++)
      {
         if(Izones[k])
         {
            I_zone_baddr = Izones[k]*fs_block_size;
            block_read_arbitrary(which_dev, buffer_I,
                                 I_zone_baddr, fs_block_size);
            zones = (uint32_t *)buffer_I;
            for(j = 0; j < ppz; j++,i++)
            {
               if(zones[j])
               {
                  remaining_size -= (fs_block_size < remaining_size ?
                                     fs_block_size : remaining_size);
                  minix3_set_zone_unoccupied(which_dev, zones[j]);
                  zones[j] = 0;
               }
            }
            minix3_set_zone_unoccupied(which_dev, zones[j]);
            zones[j] = 0;
         }
         else
         {
            i += ppz;
         }
      }
      minix3_set_zone_unoccupied(which_dev, f_inode->zones[MX3_IIDZONE]);
      f_inode->zones[MX3_IIDZONE] = 0;
   }
   else
   {
      i += ppz*ppz;
   }
   /* triply indirect */
   if(remaining_size == 0)
   {
      goto clear_all_end;
   }
   buffer_III = kcalloc(fs_block_size);
   if(f_inode->zones[MX3_IIIDZONE])
   {
      III_zone_baddr = f_inode->zones[MX3_IIIDZONE]*fs_block_size;
      block_read_arbitrary(which_dev, buffer_III,
                           III_zone_baddr, fs_block_size);
      IIzones = (uint32_t *)buffer_III;
      for(l = 0; l < ppz; l++)
      {
         if(IIzones[l])
         {
            II_zone_baddr = f_inode->zones[MX3_IIDZONE]*fs_block_size;
            block_read_arbitrary(which_dev, buffer_II,
                                 II_zone_baddr, fs_block_size);
            Izones = (uint32_t *)buffer_II;
            for(k = 0; k < ppz; k++)
            {
               if(Izones[k])
               {
                  I_zone_baddr = Izones[k]*fs_block_size;
                  block_read_arbitrary(which_dev, buffer_I,
                                       I_zone_baddr, fs_block_size);
                  zones = (uint32_t *)buffer_I;
                  for(j = 0; j < ppz; j++,i++)
                  {
                     if(zones[j])
                     {
                        remaining_size -= (fs_block_size < remaining_size ?
                                           fs_block_size : remaining_size);
                        minix3_set_zone_unoccupied(which_dev, zones[j]);
                        zones[j] = 0;
                     }
                  }
                  minix3_set_zone_unoccupied(which_dev, zones[k]);
                  zones[k] = 0;
               }
               else
               {
                  i += ppz;
               }
            }
            minix3_set_zone_unoccupied(which_dev, zones[l]);
            zones[l] = 0;
         }
         else
         {
            i += ppz*ppz;
         }
      }
      minix3_set_zone_unoccupied(which_dev, f_inode->zones[MX3_IIIDZONE]);
      f_inode->zones[MX3_IIIDZONE] = 0;
   }
   else
   {
      i += ppz*ppz*ppz;
   }

   clear_all_end:
   if(remaining_size != 0)
   {
      ERRORMSG("not cleared?");
   }
   kfree(buffer_I);
   kfree(buffer_II);
   kfree(buffer_III);
   return;
}

void minix3_init(void)
{
   int i;
   for(i = 0; i < num_block_devices; i++)
   {
      mx3_superblock_t *mx3_sb;
      mx3_sb = minix3_get_superblock(i);
      uint16_t block_magic = mx3_sb->magic;
      uint16_t block_size = mx3_sb->block_size;
      kfree(mx3_sb);
      if(block_magic == MX3_MAGIC)
      {
         mx3_block_dev = i;
         mx3_block_size = block_size;
         break;
      }
   }
   if(i == num_block_devices)
   {
      printf("failed to discover a minix3 block device\n");
      mx3_block_dev = -1;
      return;
   }
   printf("minix3 file system discovered on hard drive %d\n", mx3_block_dev);
   mx3_fs_exists = 1;
   return;
}

mx3_superblock_t *minix3_get_superblock(int which_dev)
{
   // posible TODO: cache the superblock and have this device return it that
   // way, which will be much faster
   uint32_t block_dev_size;
   block_dev_size = block_devices[which_dev].block_specific->block_size;
   if(block_dev_size < sizeof(mx3_superblock_t))
   {
      ERRORMSG("block device size too small");
      return NULL;
   }

   uint64_t block_read_start = MX3_BOOT_BLOCK_SIZE;
   uint64_t block_read_end = MX3_BOOT_BLOCK_SIZE + block_dev_size;

   if((block_read_start % block_dev_size) || (block_read_end % block_dev_size))
   {
      ERRORMSG("block device block size");
      return NULL;
   }

   char *buffer = kcalloc(block_dev_size);
   MALLOC_CHECK(buffer);
   block_read_poll(which_dev, buffer, block_read_start, block_read_end);

   mx3_superblock_t *mx3_sb = (mx3_superblock_t *)buffer;
   return mx3_sb;
}

int minix3_inode_transfer_common(int which_dev, uint32_t inode_num,
                                mx3_inode_t *buffer, int write)
{
   uint64_t inode_offset;
   mx3_superblock_t *sb = minix3_get_superblock(which_dev);
   inode_offset = 0;
   inode_offset += MX3_BOOT_BLOCK_SIZE + mx3_block_size;
   inode_offset += sb->imap_blocks * mx3_block_size;
   inode_offset += sb->zone_blocks * mx3_block_size;
   inode_offset += (inode_num-1)*sizeof(mx3_inode_t);
   // printf("inode offset: %u\n", inode_offset);

   if(write)
   {
      block_write_arbitrary(which_dev, (char*)buffer,
                            inode_offset, sizeof(mx3_inode_t));
   }
   else
   {
      block_read_arbitrary(which_dev, (char*)buffer,
                           inode_offset, sizeof(mx3_inode_t));
   }

   return 0;
}

int minix3_get_inode(int which_dev, uint32_t inode_num, mx3_inode_t *fill)
{
   return minix3_inode_transfer_common(which_dev, inode_num, fill, 0);
}

int minix3_put_inode(int which_dev, uint32_t inode_num, mx3_inode_t *inode)
{
   return minix3_inode_transfer_common(which_dev, inode_num, inode, 1);
}

int minix3_get_inode_from_name(int which_dev, mx3_inode_t *dir,
                           char *name, uint32_t *inode_num, mx3_inode_t *fill)
{
   if((dir->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      ERRORMSG("get node on non-directory\n");
      return 1;
   }
   int found = 0;
   char *zone = kcalloc(mx3_block_size);
   MALLOC_CHECK(zone);
   uint32_t remaining_size;
   uint64_t address_on_block;
   uint32_t i;
   uint32_t max_zones = minix3_get_file_max_zones(mx3_block_size);
   for(i = 0; i < max_zones; i++)
   {
      address_on_block = minix3_gzbars(which_dev, dir, i, &remaining_size);
      if(remaining_size == 0)
      {
         break;
      }
      if(address_on_block == 0)
      {
         continue;
      }
      block_read_arbitrary(which_dev, zone, address_on_block, mx3_block_size);
      mx3_dir_entry_t *dirs = (mx3_dir_entry_t*)zone;
      int j;
      uint16_t dirs_per_block = mx3_block_size/sizeof(mx3_dir_entry_t);
      for(j = 0; j < dirs_per_block; j++)
      {
         if(dirs[j].inode_num == 0)
         {
            // DEBUGMSG("empty inode");
            continue;
         }
         else
         {
            // printf("inode %d: %s\n", dirs[j].inode_num, dirs[j].name);
            if(samestr(dirs[j].name, name))
            {
               // printf("found the directory\n");
               minix3_get_inode(which_dev, dirs[j].inode_num, fill);
               *inode_num = dirs[j].inode_num;
               found = 1;
               break;
            }
            else
            {
               ; // printf("this was not the directory");
            }
         }
      }
   }
   kfree(zone);
   if(found == 0)
   {
      // DEBUGMSG("failed to find directory");
      return MX3_RETURN_CODE_NOT_FOUND;
   }
   if((fill->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      // ERRORMSG("you found yourself a non-directory\n");
      return MX3_RETURN_CODE_NOT_DIRECTORY;
   }
   return 0;
}



int minix3_touch_reg(int which_dev, uint32_t parent_inode_num, char *fname)
{
   uint16_t f_mode = MX3_MODE_REGULAR | MX3_777;
   uint16_t f_uid  = MX3_UID_MISHLER;
   uint16_t f_gid  = MX3_GID_MISHLER;
   return minix3_touch_common(which_dev, parent_inode_num, fname,
                              f_mode, f_uid, f_gid);
}

int minix3_touch_dir(int which_dev, uint32_t parent_inode_num, char *fname)
{
   uint16_t f_mode = MX3_MODE_DIRECTORY | MX3_777;
   uint16_t f_uid  = MX3_UID_MISHLER;
   uint16_t f_gid  = MX3_GID_MISHLER;
   return minix3_touch_common(which_dev, parent_inode_num, fname,
                              f_mode, f_uid, f_gid);
}

int minix3_touch_common(int which_dev, uint32_t parent_inode_num, char *fname,
                        uint16_t f_mode, uint16_t f_uid, uint16_t f_gid)
{
   mx3_inode_t *dir = kcalloc(sizeof(mx3_inode_t));
   uint32_t retval;
   retval = minix3_get_inode(which_dev, parent_inode_num, dir);
   if(retval)
   {
      kfree(dir);
      return retval;
   }
   if((dir->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      ERRORMSG("touch with a non-directory parent\n");
      kfree(dir);
      return 2;
   }
   /* make sure a file with that name doesn't already exist */
   uint32_t padding;
   mx3_inode_t *pn = kcalloc(sizeof(mx3_inode_t)); /* pn: padding node */
   retval = minix3_get_inode_from_name(which_dev, dir, fname, &padding, pn);
   kfree(pn);
   if(retval != MX3_RETURN_CODE_NOT_FOUND)
   {
      ERRORMSG("a file with that name already exists");
      return 4;
   }
   /* now create the new inode */
   uint32_t new_inode_num = minix3_get_available_inode(which_dev);
   minix3_set_inode_occupied(which_dev, new_inode_num);
   // TODO: make RTC tell us when the inode was made
   mx3_inode_t *new_inode = kcalloc(sizeof(mx3_inode_t));
   new_inode->mode = f_mode;
   new_inode->size = 0;
   new_inode->gid = f_gid;
   new_inode->uid = f_uid;
   new_inode->atime = 0xaaaaaaaa;
   new_inode->ctime = 0xcccccccc;
   new_inode->mtime = 0x22222222;


   mx3_dir_entry_t *new_dir_entry;
   new_dir_entry     = kcalloc(sizeof(mx3_dir_entry_t));
   new_dir_entry->inode_num = new_inode_num;
   mstrcpy(new_dir_entry->name, fname);

   new_inode->nlinks = 1;
   minix3_put_inode(which_dev, new_inode_num, new_inode);

   
   minix3_append_to_file(which_dev, parent_inode_num,
                         new_dir_entry, sizeof(mx3_dir_entry_t));

   kfree(dir);
   kfree(new_inode);
   return 0;
}

int minix3_rm(int which_dev, uint32_t parent_inode_num, char *fname)
{
   mx3_inode_t *dir = kcalloc(sizeof(mx3_inode_t));
   uint32_t retval;
   retval = minix3_get_inode(which_dev, parent_inode_num, dir);
   if(retval)
   {
      kfree(dir);
      return retval;
   }
   if((dir->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      ERRORMSG("rmdir with a non-directory parent\n");
      kfree(dir);
      return 2;
   }
   uint32_t child_inode_num;
   mx3_inode_t *child_inode = kcalloc(sizeof(mx3_inode_t));
   retval = minix3_get_inode_from_name(which_dev, dir, fname,
                                       &child_inode_num, child_inode);
   if(retval != MX3_RETURN_CODE_NOT_DIRECTORY)
   {
      kfree(dir);
      kfree(child_inode);
      return MX3_RETURN_CODE_IS_DIRECTORY;
   }
   if(child_inode->size > 0)
   {
      minix3_clear_all_zones(which_dev, child_inode);
   }
   minix3_set_inode_unoccupied(which_dev, child_inode_num);
   kfree(child_inode);

   uint32_t i;
   uint32_t max_zones = minix3_get_file_max_zones(mx3_block_size);
   uint64_t zone_baddr;
   uint32_t remaining_size;
   mx3_dir_entry_t *dirs;
   int j;
   char *zone;
   zone = kcalloc(mx3_block_size);
   for(i = 0; i < max_zones; i++)
   {
      zone_baddr = minix3_gzbars(which_dev, dir, i, &remaining_size);
      if(remaining_size == 0)
      {
         break;
      }
      if(zone_baddr == 0)
      {
         continue;
      }
      block_read_arbitrary(which_dev, zone, zone_baddr, mx3_block_size);
      dirs = (mx3_dir_entry_t *)zone;
      uint16_t dirs_per_block = mx3_block_size/sizeof(mx3_dir_entry_t);
      for(j = 0; j < dirs_per_block; j++)
      {
         if(dirs[j].inode_num == child_inode_num)
         {
            dirs[j].inode_num = 0;
            uint64_t dir_j_start;
            dir_j_start = zone_baddr + (uint64_t)(dirs+j)-(uint64_t)(dirs);
            block_write_arbitrary(which_dev, (char*)(dirs+j),
                                  dir_j_start, sizeof(mx3_dir_entry_t));
            break;
         }
      }
   }
   kfree(zone);
   if(remaining_size != 0)
   {
      ERRORMSG("did not find inode");
   }
   kfree(dir);
   return 0;
}



void minix3_ls(int which_dev, mx3_inode_t *dir)
{
   if((dir->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      ERRORMSG("ls on non-directory\n");
      return;
   }
   char *zone = kcalloc(mx3_block_size);
   MALLOC_CHECK(zone);
   uint32_t remaining_size;
   uint64_t address_on_block;
   uint32_t i;
   uint32_t max_zones = minix3_get_file_max_zones(mx3_block_size);
   for(i = 0; i < max_zones; i++)
   {
      address_on_block = minix3_gzbars(which_dev, dir, i, &remaining_size);
      // printf("remaining size: %u\n", remaining_size);
      if(remaining_size == 0)
      {
         break;
      }
      if(address_on_block == 0)
      {
         continue;
      }
      block_read_arbitrary(which_dev, zone, address_on_block, mx3_block_size);
      mx3_dir_entry_t *dirs = (mx3_dir_entry_t*)zone;
      int j;
      uint16_t dirs_per_block = mx3_block_size/sizeof(mx3_dir_entry_t);
      for(j = 0; j < dirs_per_block; j++)
      {
         if(dirs[j].inode_num == 0)
         {
            // DEBUGMSG("empty inode");
            continue;
         }
         else
         {
            printf("inode %d: %s\n", dirs[j].inode_num, dirs[j].name);
            remaining_size -= sizeof(mx3_dir_entry_t);
            if(remaining_size == 0)
            {
               break;
            }
         }
      }
   }
   kfree(zone);
   return;
}

int minix3_mkdir(int which_dev, uint32_t parent_inode_num, char *dirname)
{
   uint32_t retval;
   uint32_t new_inode_num;
   retval = minix3_touch_dir(which_dev, parent_inode_num, dirname);
   if(retval)
   {
      return retval;
   }


   /* increase the nlinks from dot and dot_dot */
   mx3_inode_t *dir       = kcalloc(sizeof(mx3_inode_t));
   mx3_inode_t *new_inode = kcalloc(sizeof(mx3_inode_t));

   minix3_get_inode(which_dev, parent_inode_num, dir);
   minix3_get_inode_from_name(which_dev, dir, dirname,
                              &new_inode_num, new_inode);

   new_inode->nlinks += 1;
   dir->nlinks += 1;

   minix3_put_inode(which_dev, parent_inode_num, dir);
   minix3_put_inode(which_dev, new_inode_num, new_inode);

   kfree(dir);
   kfree(new_inode);

   /* now add dot and dot_dot to the new directory */
   mx3_dir_entry_t *new_dot, *new_dot_dot;
   new_dot     = kcalloc(sizeof(mx3_dir_entry_t));
   new_dot_dot = kcalloc(sizeof(mx3_dir_entry_t));
   new_dot->inode_num = new_inode_num;
   new_dot_dot->inode_num = parent_inode_num;
   mstrcpy(new_dot->name,     ".");
   mstrcpy(new_dot_dot->name, "..");
   
   minix3_append_to_file(which_dev, new_inode_num,
                         new_dot, sizeof(mx3_dir_entry_t));
   minix3_append_to_file(which_dev, new_inode_num,
                         new_dot_dot, sizeof(mx3_dir_entry_t));
   kfree(new_dot);
   kfree(new_dot_dot);

   return 0;
}

int minix3_rmdir(int which_dev, uint32_t parent_inode_num, char *dirname)
{
   mx3_inode_t *dir = kcalloc(sizeof(mx3_inode_t));
   uint32_t retval;
   retval = minix3_get_inode(which_dev, parent_inode_num, dir);
   if(retval)
   {
      kfree(dir);
      return retval;
   }
   if((dir->mode & MX3_MODE_MASK) != MX3_MODE_DIRECTORY)
   {
      ERRORMSG("rmdir with a non-directory parent\n");
      kfree(dir);
      return 2;
   }
   uint32_t child_inode_num;
   mx3_inode_t *child_inode = kcalloc(sizeof(mx3_inode_t));
   retval = minix3_get_inode_from_name(which_dev, dir, dirname,
                                       &child_inode_num, child_inode);
   if(retval != 0)
   {
      kfree(dir);
      kfree(child_inode);
      return retval;
   }
   /* make sure the directory is empty and contains only . and .. */
   retval = minix3_rmdir_check(which_dev, child_inode);
   if(retval != 0)
   {
      kfree(dir);
      kfree(child_inode);
      return retval;
   }
   if(child_inode_num == 1)
   {
      ERRORMSG("bruh, don't remove /");
      kfree(dir);
      kfree(child_inode);
      return MX3_RETURN_CODE_UNKNOWN_ERROR;
   }
   uint32_t child_data_zone;
   child_data_zone = minix3_get_last_file_zone(which_dev, child_inode);
   if(child_data_zone != 0)
   {
      return MX3_RETURN_CODE_UNKNOWN_ERROR;
   }
   minix3_clear_zone(which_dev, child_inode, child_data_zone);
   minix3_set_inode_unoccupied(which_dev, child_inode_num);
   kfree(child_inode);

   uint32_t i;
   uint32_t max_zones = minix3_get_file_max_zones(mx3_block_size);
   uint64_t zone_baddr;
   uint32_t remaining_size;
   mx3_dir_entry_t *dirs;
   int j;
   char *zone;
   zone = kcalloc(mx3_block_size);
   for(i = 0; i < max_zones; i++)
   {
      zone_baddr = minix3_gzbars(which_dev, dir, i, &remaining_size);
      if(remaining_size == 0)
      {
         break;
      }
      if(zone_baddr == 0)
      {
         continue;
      }
      block_read_arbitrary(which_dev, zone, zone_baddr, mx3_block_size);
      dirs = (mx3_dir_entry_t *)zone;
      uint16_t dirs_per_block = mx3_block_size/sizeof(mx3_dir_entry_t);
      for(j = 0; j < dirs_per_block; j++)
      {
         if(dirs[j].inode_num == child_inode_num)
         {
            dirs[j].inode_num = 0;
            uint64_t dir_j_start;
            dir_j_start = zone_baddr + (uint64_t)(dirs+j)-(uint64_t)(dirs);
            block_write_arbitrary(which_dev, (char*)(dirs+j),
                                  dir_j_start, sizeof(mx3_dir_entry_t));
            break;
         }
      }
   }
   kfree(zone);
   if(remaining_size != 0)
   {
      ERRORMSG("did not find inode");
   }
   dir->nlinks -= 1;
   minix3_put_inode(which_dev, parent_inode_num, dir);
   kfree(dir);
   return 0;
}

/* see if we'd have any issues removing this file */
int minix3_rmdir_check(int which_dev, mx3_inode_t *dir_node)
{
   if(dir_node->size != 2*sizeof(mx3_dir_entry_t))
   {
      DEBUGMSG("directory not empty, but you could be missing coalesce code");
      return MX3_RETURN_CODE_NOT_EMPTY;
   }
   int retval_dot, retval_dot_dot;
   mx3_inode_t *pn;
   uint32_t padding;
   pn = kcalloc(sizeof(mx3_inode_t));
   retval_dot = minix3_get_inode_from_name(which_dev, dir_node, ".",
                                           &padding, pn);
   retval_dot_dot = minix3_get_inode_from_name(which_dev, dir_node, ".",
                                               &padding, pn);
   kfree(pn);
   if(retval_dot != 0 || retval_dot_dot != 0)
   {
      return MX3_RETURN_CODE_UNKNOWN_ERROR;
   }
   return 0;
}


// TODO: swap out void* with char*?
int minix3_append_to_file(int which_dev, uint32_t file_inode_num,
                          void* buffer,  uint32_t size)
{
   mx3_inode_t *f_inode = kcalloc(sizeof(mx3_inode_t));
   uint32_t retval;
   retval = minix3_get_inode(which_dev, file_inode_num, f_inode);
   if(retval)
   {
      kfree(f_inode);
      DEBUGMSG("repeating debug");
      return retval;
   }
   uint32_t remaining_file_size;
   uint32_t remaining_block_size;
   uint32_t i;
   char *zone = kcalloc(mx3_block_size);
   uint64_t address_on_block;
   uint32_t write_size;

   /* write to the rest of this block */
   i = minix3_get_last_file_zone(which_dev, f_inode);
   address_on_block = minix3_gzbars(which_dev, f_inode,
                                        i, &remaining_file_size);
   if(address_on_block)
   {
      remaining_block_size = mx3_block_size - remaining_file_size;
      address_on_block += remaining_file_size;
      write_size = remaining_block_size < size ? remaining_block_size : size;
      block_write_arbitrary(which_dev, buffer, address_on_block, write_size);
      size -= write_size;
      buffer = (void*) ((uint64_t)buffer+write_size);
      f_inode->size += write_size;
   }

   /* add more blocks as needed */
   while(size)
   {
      address_on_block = minix3_append_zone(which_dev, f_inode);
      write_size = mx3_block_size < size ? mx3_block_size : size;
      
      block_write_arbitrary(which_dev, buffer, address_on_block, write_size);

      size -= write_size;
      buffer = (void*) ((uint64_t)buffer+write_size);
      f_inode->size += write_size;
      printf("size left: %u\n", size);
   }
   kfree(zone);

   minix3_put_inode(which_dev, file_inode_num, f_inode);
   kfree(f_inode);
   return 0;
}

int  minix3_read_from_zone(int which_dev, mx3_inode_t *f_inode,
                           uint32_t zone_index, char *fill)
{
   uint64_t zone_baddr;
   uint32_t remaining_size;
   zone_baddr = minix3_gzbars(which_dev, f_inode, zone_index, &remaining_size);
   if(zone_baddr == 0)
   {
      return 1;
   }
   /* else */
   uint32_t read_size;
   read_size = (remaining_size < mx3_block_size ?
                remaining_size : mx3_block_size);
   block_read_arbitrary(which_dev, fill, zone_baddr, read_size);
   return 0;
}

char* minix3_read_from_file(int which_dev, mx3_inode_t *f_inode,
                            uint32_t start_byte, uint32_t size)
{
   // TODO: this calculation is not safe. There could be zones that are
   //       NULL before the end of the file. Fix that!
   if(start_byte > f_inode->size)
   {
      ERRORMSG("not in file");
      return NULL;
   }
   if(start_byte + size > f_inode->size)
   {
      DEBUGMSG("flowing over file");
      return NULL;
   }
   if(start_byte % mx3_block_size)
   {
      ERRORMSG("we don't support non-block-sized start bytes (yet)");
      return NULL;
   }
   char *file_buffer;
   file_buffer = kcalloc(size);
   char *target;
   uint32_t target_zone;
   uint32_t file_buffer_offset = 0;
   while(size)
   {
      // TODO: this calculation is not safe. There could be zones that are
      //       NULL before the end of the file. Fix that!
      target_zone = (start_byte + file_buffer_offset) / mx3_block_size;
      target = file_buffer + file_buffer_offset;
      if(minix3_read_from_zone(which_dev, f_inode, target_zone, target))
      {
         ERRORMSG("");
      }
      if(f_inode->size > file_buffer_offset + start_byte + mx3_block_size)
      {
         file_buffer_offset += mx3_block_size;
         size -= mx3_block_size;
      }
      else
      {
         size -= f_inode->size - file_buffer_offset - start_byte;
         if(size != 0)
         {
            ERRORMSG("");
         }
      }
   }
   return file_buffer;
}

char* minix3_read_whole_file(int which_dev, mx3_inode_t *f_inode)
{
   return minix3_read_from_file(which_dev, f_inode, 0, f_inode->size);
}

void minix3_print_superblock(mx3_superblock_t *mx3_sb)
{
   printf("minix 3 superblock:\n");
   printf("size of a minix 3 superblock: %d\n", sizeof(mx3_superblock_t));

   printf(" num_inodes:      0x%08x\n", mx3_sb->num_inodes);
   printf(" imap_blocks:     0x%04x\n", mx3_sb->imap_blocks);
   printf(" zone_blocks:     0x%04x\n", mx3_sb->zone_blocks);
   printf(" first_data_zone: 0x%08x\n", mx3_sb->first_data_zone);
   printf(" log_zone_size:   0x%04x\n", mx3_sb->log_zone_size);
   printf(" max_file_size:   0x%08x\n", mx3_sb->max_file_size);
   printf(" num_zones:       0x%08x\n", mx3_sb->num_zones);
   printf(" magic:           0x%04x\n", mx3_sb->magic);
   printf(" block_size:      0x%04x\n", mx3_sb->block_size);
   printf(" disk_version:    0x%02x\n", mx3_sb->disk_version);

   kfree(mx3_sb);
   return;
}

void minix3_print_inode(mx3_inode_t *inode)
{
   printf("minix3 inode:\n");
   printf("size of a minix 3 inode: %d\n", sizeof(mx3_inode_t));

   printf(" mode:            0x%04x\n", inode->mode);
   printf(" nlinks:          0x%04x\n", inode->nlinks);
   printf(" uid:             0x%04x\n", inode->uid);
   printf(" gid:             0x%04x\n", inode->gid);
   printf(" size:            0x%08x\n", inode->size);
   printf(" atime:           0x%08x\n", inode->atime);
   printf(" ctime:           0x%08x\n", inode->ctime);
   printf(" mtime:           0x%08x\n", inode->mtime);
   int i;
   for(i = 0; i < MX3_NZONES; i++)
   {
      if( i == MX3_IDZONE)
      {
         printf("  indirect zone pointer:        ");
      }
      else if( i == MX3_IIDZONE)
      {
         printf("  doubly indirect zone pointer: ");
      }
      else if( i == MX3_IIIDZONE)
      {
         printf("  triply indirect zone pointer: ");
      }
      else
      {
         printf("  direct zone pointer:          ");
      }
      printf("0x%08x\n", inode->zones[i]);
   }
   return;
}
