#pragma once
#include<stdint.h>

#define MX3_BOOT_BLOCK_SIZE 1024
#define MX3_MAGIC 0x4d5a /*'Z''M' in little endian order */

extern int mx3_block_dev;
extern int mx3_fs_exists;
extern uint16_t mx3_block_size;

// structure taken from the lecture notes
typedef struct mx3_superblock
{
   uint32_t num_inodes;
   uint16_t padding0;
   uint16_t imap_blocks;
   uint16_t zone_blocks;
   uint16_t first_data_zone;
   uint16_t log_zone_size;
   uint16_t padding1;
   uint32_t max_file_size;
   uint32_t num_zones;
   uint16_t magic;
   uint16_t padding2;
   uint16_t block_size;        // 1024 << log_zone_size
   uint8_t  disk_version;      // usually unused
} mx3_superblock_t;


#define MX3_NZONES   10
// first seven zones: direct poiters
#define MX3_IDZONE   7
#define MX3_IIDZONE  8
#define MX3_IIIDZONE 9
// structure also taken from the lecture notes
typedef struct mx3_inode
{
   uint16_t mode;              // file type
   uint16_t nlinks;            // how many links to the file still exist?
   uint16_t uid;               // user id
   uint16_t gid;               // group id
   uint32_t size;              // filesize
   uint32_t atime;             // last accessed
   uint32_t ctime;             // created
   uint32_t mtime;             // modified
   uint32_t zones[MX3_NZONES]; // where to find the data of the file
} mx3_inode_t;

#define MX3_NAME_LEN 60
typedef struct mx3_dir_entry
{
   uint32_t inode_num;
   char name[MX3_NAME_LEN];
} mx3_dir_entry_t;

// for the VFS
#define MX3_RETURN_CODE_NOT_FOUND      101
#define MX3_RETURN_CODE_NOT_DIRECTORY  102
#define MX3_RETURN_CODE_IS_DIRECTORY   103
#define MX3_RETURN_CODE_NOT_EMPTY      104
#define MX3_RETURN_CODE_UNKNOWN_ERROR  105

// for file types
#define MX3_MODE_MASK      0xf000
#define MX3_MODE_SOCKET    0xc000
#define MX3_MODE_LINK      0xa000
#define MX3_MODE_REGULAR   0x8000
#define MX3_MODE_BLOCK     0x6000
#define MX3_MODE_DIRECTORY 0x4000
#define MX3_MODE_FIFO      0x1000

#define MX3_USR_R 00400
#define MX3_USR_W 00200
#define MX3_USR_X 00100

#define MX3_GRP_R 00040
#define MX3_GRP_W 00020
#define MX3_GRP_X 00010

#define MX3_OTH_R 00004
#define MX3_OTH_W 00002
#define MX3_OTH_X 00001

#define MX3_777   00777

// for GID, UID
#define MX3_GID_MISHLER 0xd000
#define MX3_UID_MISHLER 0xd000


int minix3_is_zone_occupied( int which_dev, uint32_t zone_num);
int minix3_is_inode_occupied(int which_dev, uint32_t inode_num);
int minix3_is_zone_in_range( int which_dev, uint32_t zone_num);
int minix3_is_inode_in_range(int which_dev, uint32_t inode_num);

int minix3_set_zone_occupied( int which_dev, uint32_t zone_num);
int minix3_set_zone_unoccupied( int which_dev, uint32_t zone_num);
int minix3_set_zone_as( int which_dev, uint32_t zone_num, int as);

int minix3_set_inode_occupied(int which_dev, uint32_t inode_num);
int minix3_set_inode_unoccupied(int which_dev, uint32_t inode_num);
int minix3_set_inode_as(int which_dev, uint32_t inode_num, int as);

void minix3_set_zone_to_zero(int which_dev, uint32_t which_zone);
uint32_t minix3_get_available_zone( int which_dev);
uint32_t minix3_get_available_inode(int which_dev);

/*
 * the following series of functions enables a the abstraction of thinking
 * of the file as a series of contiguous zones, even though almost all of the
 * zones belong to the triply indirect field
 */
uint32_t minix3_get_file_max_zones(uint16_t fs_block_size);
uint32_t minix3_get_last_file_zone(int which_dev, mx3_inode_t *f_inode);
uint64_t minix3_append_zone(int which_dev, mx3_inode_t *f_inode);
uint64_t minix3_gzbars(int which_dev,
                       mx3_inode_t *f_inode,
                       uint32_t zone_index,
                       uint32_t *remaining_size);
uint64_t minix3_get_zone_baddr(int which_dev,
                               mx3_inode_t *f_inode,
                               uint32_t zone_index);
uint32_t minix3_get_zone(int which_dev,
                         mx3_inode_t *f_inode,
                         uint32_t zone_index);
uint32_t minix3_set_zone_to(int which_dev, mx3_inode_t *f_inode,
                            uint32_t zone_index, uint32_t target);
void     minix3_clear_zone(int which_dev, mx3_inode_t *f_inode,
                           uint32_t zone_index);
uint32_t minix3_zone_walker(int which_dev,
                            mx3_inode_t *f_inode,
                            uint32_t zone_index,
                            uint32_t *fill_remaining_size,
                            int do_set_zone,
                            uint32_t set_zone_to);
void minix3_clear_all_zones(int which_dev, mx3_inode_t *f_inode);

void minix3_init(void);
mx3_superblock_t *minix3_get_superblock(int which_dev);

int minix3_get_inode(int which_dev, uint32_t inode_num, mx3_inode_t *fill);
int minix3_put_inode(int which_dev, uint32_t inode_num, mx3_inode_t *inode);
int minix3_inode_transfer_common(int which_dev, uint32_t inode_num,
                                 mx3_inode_t *buffer, int write);
int minix3_get_inode_from_name(int which_dev, mx3_inode_t *dir_node,
                           char *name, uint32_t *inode_num, mx3_inode_t *fill);

/* TODO: we could make it so that directories will coalesce after getting *
 * something rm'ed from them                                              */

/* regular files */
int minix3_touch_reg(int which_dev, uint32_t parent_inode_num, char *fname);
int minix3_touch_dir(int which_dev, uint32_t parent_inode_num, char *fname);
int minix3_touch_common(int which_dev, uint32_t parent_inode_num, char *fname,
                        uint16_t f_mode, uint16_t f_uid, uint16_t f_gid);
int minix3_rm(int which_dev, uint32_t parent_inode_num, char *fname);

/* directories */
void minix3_ls(int which_dev, mx3_inode_t *dir);
int  minix3_mkdir(int which_dev, uint32_t parent_inode_num, char *dirname);
int  minix3_rmdir(int which_dev, uint32_t parent_inode_num, char *dirname);
int  minix3_rmdir_check(int which_dev, mx3_inode_t *dir_node);


/* more utilities */
int  minix3_append_to_file(int which_dev, uint32_t file_inode_num,
                           void* buffer,  uint32_t size);
int  minix3_read_from_zone(int which_dev, mx3_inode_t *f_inode,
                           uint32_t zone_index, char *fill);
char* minix3_read_from_file(int which_dev, mx3_inode_t *f_inode,
                            uint32_t start_byte, uint32_t size);
char* minix3_read_whole_file(int which_dev, mx3_inode_t *f_inode);



void minix3_print_superblock(mx3_superblock_t *mx3_sb);
void minix3_print_inode(mx3_inode_t *inode);
