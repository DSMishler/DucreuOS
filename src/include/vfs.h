#pragma once
#include <stdint.h>


#include <gpu.h>


// virtual inode, only *some* of the information from the other file systems
// (just what the OS needs to know)
typedef struct v_inode
{
   uint16_t mode;              // file type
   uint16_t uid;               // user id
   uint16_t gid;               // group id
} v_inode_t;

#define VFS_FNAME_LENGTH 64
#define VFS_PATH_LENGTH  256 // TODO: enforce me

#define VFS_FS_UNKNOWN  -1
#define VFS_FS_MINIX3   1
#define VFS_FS_OS       2

typedef struct vfs_directory
{
   char name[VFS_FNAME_LENGTH];
   int which_dev;      // which block device to find this inode on
   int fs_type;        // Minix3, ext4, etc.
   uint32_t inode_num; // the inode number of the inode on the block device
   v_inode_t *v_inode;
   struct vfs_directory *parent;
   struct vfs_directory_llist_start *children_llist;
                     // office hours moment: does the structure get re-def'ed?
                     // opting to leave it as a pointer for now so that remaking
                     // it a void* will be easy if failures happen
} vfs_directory_t;

typedef struct vfs_directory_llist
{
   vfs_directory_t *vdir;
   struct vfs_directory_llist *next;
   struct vfs_directory_llist *prev;
} vfs_directory_llist_t;

typedef struct vfs_directory_llist_start
{
   vfs_directory_llist_t *head;
} vfs_directory_llist_start_t;

/* return codes */
#define VFS_RETURN_CODE_NOT_FOUND     0x44
#define VFS_RETURN_CODE_NOT_DIRECTORY 0x45

extern vfs_directory_t *v_dir_root;
extern vfs_directory_t *v_dir_pwd;
extern char *vfs_pwd;

void vfs_init(void);
int vfs_mount_minix3(vfs_directory_t *mount_to_dir);

void vfs_free_directory_llist_node(vfs_directory_llist_t *vfsd);
int  vfs_find_in_llist(vfs_directory_llist_start_t *headptr,
                       vfs_directory_llist_t *target);
void vfs_remove_from_llist(vfs_directory_llist_start_t *headptr,
                           vfs_directory_llist_t *target);

void vfs_get_path(vfs_directory_t *cur_dir, char *fullname);
void vfs_update_pwd(void);
void vfs_ls(vfs_directory_t *cur_dir);
int  vfs_is_path_cached(vfs_directory_t *vdir, char *path);
int  vfs_cd_into_cached_path(char *path);
int  vfs_cache_path(vfs_directory_t *parent_dir, char *path);
void vfs_process_path(char *path);
void vfs_path_to_minix3_path(char *path);
void vfs_cd(char *path);

/* regular files */
void vfs_touch(vfs_directory_t *vdir, char *fname);
void vfs_rm(vfs_directory_t *vdir, char *fname);

/* directories */
void vfs_mkdir(vfs_directory_t *vdir, char *dirname);
void vfs_rmdir(vfs_directory_t *vdir, char *dirname);

/* other utilities */
void vfs_cat(vfs_directory_t *vdir, char *fname);
void vfs_append(vfs_directory_t *vdir, char *fname, char *buffer,uint32_t size);


/* basically just here for the project */
void vfs_bmp_to_file(vfs_directory_t *vdir, char *fname, int bmp_dev);
void vfs_bmp_file_to_gpu(vfs_directory_t *vdir, char *fname,
                         gpu_rectangle_t *r);

void vfs_show_directory_as_inode(vfs_directory_t *vdir);
void vfs_print_inode(vfs_directory_t *vdir, char *fname);
void vfs_print_cached_tree(vfs_directory_t *base);
void vfs_print_cached_tree_rec(vfs_directory_t *base, int level);
