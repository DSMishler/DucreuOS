#include <vfs.h>
#include <minix3.h>
#include <utils.h>
#include <kmalloc.h>
#include <printf.h>

#include <bmp.h>
#include <block.h>
#include <gpu.h>


vfs_directory_t *v_dir_root;
vfs_directory_t *v_dir_pwd;
char *vfs_pwd;

void vfs_init(void)
{
   // Does not need to be contiguous.
   // why kzalloc? Just for convenience.
   v_dir_root = kzalloc(sizeof(vfs_directory_t));
   MALLOC_CHECK(v_dir_root);
   mstrcpy(v_dir_root->name, "/");
   v_dir_root->which_dev = -1;
   v_dir_root->fs_type = VFS_FS_OS;
   v_dir_root->inode_num = 0;
   v_dir_root->v_inode = NULL;
   v_dir_root->parent = v_dir_root;
   v_dir_root->children_llist = kmalloc(sizeof(vfs_directory_llist_start_t));
   MALLOC_CHECK(v_dir_root->children_llist);
   v_dir_root->children_llist->head = NULL;

   v_dir_pwd = v_dir_root;

   vfs_pwd = kmalloc(VFS_PATH_LENGTH);
   MALLOC_CHECK(vfs_pwd);
   vfs_get_path(v_dir_pwd, vfs_pwd);

   return;
}

// TODO: it would be super cool to have this take a mount *path* instead of 
//       a vfs directory pointer
int vfs_mount_minix3(vfs_directory_t *mount_to_dir)
{
   if(!(mx3_fs_exists))
   {
      ERRORMSG("no minix3 fs");
      return 1;
   }
   if(mount_to_dir->fs_type != VFS_FS_OS)
   {
      ERRORMSG("remount directory");
   }
   mount_to_dir->which_dev = mx3_block_dev;
   mount_to_dir->fs_type   = VFS_FS_MINIX3;
   mount_to_dir->inode_num = 1; // root is inode #1 on a minix3 fs
   v_inode_t *vnode = kcalloc(sizeof(v_inode_t));
   MALLOC_CHECK(vnode);
   mx3_inode_t *mx3_root = kcalloc(sizeof(mx3_inode_t));
   MALLOC_CHECK(mx3_root);
   minix3_get_inode(mount_to_dir->which_dev, mount_to_dir->inode_num, mx3_root); 
   vnode->mode = mx3_root->mode;
   vnode->uid  = mx3_root->uid;
   vnode->gid  = mx3_root->gid;
   mount_to_dir->v_inode = vnode;
   kfree(mx3_root);

   return 0;
}

void vfs_free_directory_llist_node(vfs_directory_llist_t *vfsd)
{
   if(vfsd->vdir->children_llist->head)
   {
      ERRORMSG("free of nonempty llist");
      return;
   }
   kfree(vfsd->vdir->children_llist);
   kfree(vfsd->vdir->v_inode);
   kfree(vfsd->vdir);
   kfree(vfsd);
   return;
}

int  vfs_find_in_llist(vfs_directory_llist_start_t *headptr,
                       vfs_directory_llist_t *target)
{
   vfs_directory_llist_t *p;
   p = headptr->head;
   int i = 0;
   for(; p != NULL; p = p->next)
   {
      if(p == target)
      {
         return i;
      }
   }
   return -1;
}

void vfs_remove_from_llist(vfs_directory_llist_start_t *headptr,
                           vfs_directory_llist_t *target)
{
   if(vfs_find_in_llist(headptr, target) == -1)
   {
      ERRORMSG("remove from list it is not present in");
      return;
   }
   /* first node */
   if(headptr->head == target)
   {
      headptr->head = target->next; /* which could be left at NULL */
   }
   if(target->next)
   {
      (target->next)->prev = target->prev;
   }
   /* you could be clever and put this as an else to
    * the first if statement if you wanted          */
   if(target->prev) 
   {
      (target->prev)->next = target->next;
   }

   target->next = NULL;
   target->prev = NULL;
   return;
}

void vfs_get_path(vfs_directory_t *cur_dir, char *fullname)
{
   fullname[0] = '\0';
   while(cur_dir != NULL) // NULL for safety check, but could just do while 1.
   {
      mstrtac(cur_dir->name, fullname);
      if(cur_dir->parent == cur_dir)
      {
         break;
      }
      cur_dir = cur_dir->parent;
   }
   return;
}

void vfs_update_pwd(void)
{
   vfs_get_path(v_dir_pwd, vfs_pwd);
}

void vfs_ls(vfs_directory_t *cur_dir)
{
   if(cur_dir == NULL)
   {
      ERRORMSG("null directory");
      return;
   }
   // printf("ls for '%s'\n", cur_dir->name);
   // TODO: have a get full path
   switch(cur_dir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      mx3_inode_t *mx3_dir_node = kcalloc(sizeof(mx3_inode_t));
      MALLOC_CHECK(mx3_dir_node);
      minix3_get_inode(cur_dir->which_dev, cur_dir->inode_num, mx3_dir_node); 
      minix3_ls(cur_dir->which_dev, mx3_dir_node);
      // TODO: also list out any directories which minix3_ls may have missed!
      // (because the vfs could have mounted other directories from here)
      break;
   case VFS_FS_OS:
      ERRORMSG("ls on uninit-ed directory");
      break;
   case VFS_FS_UNKNOWN: // fallthrough
   default:
      ERRORMSG("ls on unknown file type directory");
      break;
   }
   return;
}

int vfs_is_path_cached(vfs_directory_t *vdir, char *path)
{
   if(vdir->children_llist->head == NULL)
   {
      ; // printf("I haven't been here yet.\n");
   }
   else
   {
      vfs_directory_llist_t *vfs_dll;
      vfs_dll = vdir->children_llist->head;
      for(; vfs_dll != NULL; vfs_dll = vfs_dll->next)
      {
         if(samestr(path, vfs_dll->vdir->name))
         {
            // printf("path was cached\n");
            return 1;
         }
      }
   }
   return 0;
}

int vfs_cd_into_cached_path(char *path)
{
   if(!(vfs_is_path_cached(v_dir_pwd,path)))
   {
      ERRORMSG("reaching this message should be impossible");
      return 1;
   }
   
   vfs_directory_llist_t *vfs_dll;
   vfs_dll = v_dir_pwd->children_llist->head;
   for(; vfs_dll != NULL; vfs_dll = vfs_dll->next)
   {
      if(samestr(path, vfs_dll->vdir->name))
      {
         v_dir_pwd = vfs_dll->vdir;
         vfs_update_pwd();
         return 1;
      }
   }
   return 0;
}

int vfs_cache_path(vfs_directory_t *parent_dir, char *path)
{
   vfs_directory_t *vdir;
   switch(parent_dir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      mx3_inode_t *parent_inode = kcalloc(sizeof(mx3_inode_t));
      MALLOC_CHECK(parent_inode);
      minix3_get_inode(parent_dir->which_dev, parent_dir->inode_num,
                       parent_inode);

      char minix3_path[MX3_NAME_LEN];
      mstrcpy(minix3_path, path);
      vfs_path_to_minix3_path(minix3_path);

      mx3_inode_t *child_inode  = kcalloc(sizeof(mx3_inode_t));
      uint32_t child_inode_num;
      MALLOC_CHECK(child_inode);
      int retval;
      retval = minix3_get_inode_from_name(parent_dir->which_dev, parent_inode,
                                 minix3_path, &child_inode_num, child_inode);
      if(retval == MX3_RETURN_CODE_NOT_FOUND)
      {
         return VFS_RETURN_CODE_NOT_FOUND;
      }
      else if(retval == MX3_RETURN_CODE_NOT_DIRECTORY)
      {
         return VFS_RETURN_CODE_NOT_DIRECTORY;
      }

      vdir = kmalloc(sizeof(vfs_directory_t));
      MALLOC_CHECK(vdir);
      mstrcpy(vdir->name, path); // note: not the minix3 path, the vfs path!
      vdir->which_dev = parent_dir->which_dev;
      vdir->fs_type = parent_dir->fs_type;
      vdir->inode_num = child_inode_num;

      v_inode_t *vnode = kmalloc(sizeof(v_inode_t));
      MALLOC_CHECK(vnode);
      vnode->mode = child_inode->mode;
      vnode->uid  = child_inode->uid;
      vnode->gid  = child_inode->gid;
      vdir->v_inode = vnode;
      
      vdir->parent = parent_dir;

      vdir->children_llist = kmalloc(sizeof(vfs_directory_llist_start_t));
      MALLOC_CHECK(vdir->children_llist);
      vdir->children_llist->head = NULL;

      break;
   default:
      ERRORMSG("call from invalid directory");
      return 1;
      break;
   }




   vfs_directory_llist_t *new_node = kmalloc(sizeof(vfs_directory_llist_t));
   MALLOC_CHECK(new_node);
   new_node->vdir = vdir;
   new_node->prev = NULL;
   new_node->next = NULL;
   if(parent_dir->children_llist->head == NULL)
   {
      parent_dir->children_llist->head = new_node;
   }
   else
   {
      vfs_directory_llist_t *vfs_dll = parent_dir->children_llist->head;
      vfs_dll->prev = new_node;
      new_node->next = vfs_dll;
      parent_dir->children_llist->head = new_node;
   }

   return 0;
}

void vfs_process_path(char *path)
{
   int pathlen = mstrlen(path);
   if(pathlen < 1)
   {
      ERRORMSG("path too short");
      return;
   }
   // add the '/' if necessary.
   if(path[pathlen-1] != '/')
   {
      path[pathlen] = '/';
      path[pathlen+1] = '\0';
   }
   // TODO: remove all instances of double slash
   return;
}

void vfs_path_to_minix3_path(char *path)
{
   int pathlen = mstrlen(path);
   if(pathlen < 1)
   {
      ERRORMSG("path too short");
      return;
   }
   if(path[pathlen-1] == '/')
   {
      path[pathlen-1] = '\0';
   }
   pathlen = mstrlen(path);
   if(pathlen + 1 > MX3_NAME_LEN)
   {
      ERRORMSG("path too long for minix3");
      return;
   }
}

// TODO: you currently can't cd through more than one directory at a time.
// Fix that!
void vfs_cd(char *path)
{
   if(samestr(path, "."))
   {
      return;
   }
   else if(samestr(path, ""))
   {
      v_dir_pwd = v_dir_root;
      vfs_update_pwd();
      return;
   }
   else if(samestr(path, ".."))
   {
      v_dir_pwd = v_dir_pwd->parent; // which might do nothing if you're at '/'
      vfs_update_pwd();
      return;
   }
   // else it's a real path (we only do relative for now
   char processed_path[VFS_PATH_LENGTH];
   mstrcpy(processed_path, path);
   vfs_process_path(processed_path);
   // printf("cd from %s to %s\n", vfs_pwd, processed_path);
   /* first, look in the current directory llist for it */
   if(!(vfs_is_path_cached(v_dir_pwd, processed_path)))
   {
      int retval = vfs_cache_path(v_dir_pwd, processed_path);
      if(retval == VFS_RETURN_CODE_NOT_FOUND)
      {
         printf("vfs: failed to find directory '%s'.\n", path);
         return;
      }
      else if(retval == VFS_RETURN_CODE_NOT_DIRECTORY)
      {
         printf("vfs: flie '%s' is not a directory.\n", path);
         return;
      }
      else if(retval)
      {
         ERRORMSG("unknown error");
         return;
      }
   }
   vfs_cd_into_cached_path(processed_path);
}

void vfs_touch(vfs_directory_t *vdir, char *fname)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_fname[MX3_NAME_LEN];
      mstrcpy(mx3_fname, fname);
      vfs_path_to_minix3_path(mx3_fname);
      retval = minix3_touch_reg(vdir->which_dev, vdir->inode_num, mx3_fname);
      if(retval)
      {
         ERRORMSG("touch error");
      }
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}

void vfs_rm(vfs_directory_t *vdir, char *dirname)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_dirname[MX3_NAME_LEN];
      mstrcpy(mx3_dirname, dirname);
      vfs_path_to_minix3_path(mx3_dirname);
      retval = minix3_rm(vdir->which_dev, vdir->inode_num, mx3_dirname);
      if(retval == MX3_RETURN_CODE_NOT_FOUND)
      {
         printf("directory '%s' does not exist here\n", dirname);
      }
      else if(retval == MX3_RETURN_CODE_IS_DIRECTORY)
      {
         printf("'%s' is a directory\n", dirname);
      }
      else if(retval == MX3_RETURN_CODE_UNKNOWN_ERROR)
      {
         ERRORMSG("known unknown");
      }
      else if(retval != 0)
      {
         ERRORMSG("unknown unknown");
      }
      else /* retval == 0 */
      {
         ;
      }
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}

void vfs_mkdir(vfs_directory_t *vdir, char *dirname)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_dirname[MX3_NAME_LEN];
      mstrcpy(mx3_dirname, dirname);
      vfs_path_to_minix3_path(mx3_dirname);
      retval = minix3_mkdir(vdir->which_dev, vdir->inode_num, mx3_dirname);
      if(retval)
      {
         ERRORMSG("mkdir error");
      }
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   /* go ahead and cache the newly made path */
   char vfs_dirname[MX3_NAME_LEN];
   mstrcpy(vfs_dirname, dirname);
   vfs_process_path(vfs_dirname);
   vfs_cache_path(vdir, vfs_dirname);
   return;
}

void vfs_rmdir(vfs_directory_t *vdir, char *dirname)
{
   int remove_from_cached_tree = 0;
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_dirname[MX3_NAME_LEN];
      mstrcpy(mx3_dirname, dirname);
      vfs_path_to_minix3_path(mx3_dirname);
      retval = minix3_rmdir(vdir->which_dev, vdir->inode_num, mx3_dirname);
      if(retval == MX3_RETURN_CODE_NOT_FOUND)
      {
         printf("directory '%s' does not exist here\n", dirname);
      }
      else if(retval == MX3_RETURN_CODE_NOT_DIRECTORY)
      {
         printf("'%s' is not a directory\n", dirname);
      }
      else if(retval == MX3_RETURN_CODE_NOT_EMPTY)
      {
         printf("cannot remove '%s'. It is not empty\n", dirname);
      }
      else if(retval == MX3_RETURN_CODE_UNKNOWN_ERROR)
      {
         ERRORMSG("known unknown error");
      }
      else if(retval != 0)
      {
         ERRORMSG("unknown unknown error");
      }
      else /* retval == 0 */
      {
         remove_from_cached_tree = 1;
      }
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   if(remove_from_cached_tree)
   {
      char vfs_dirname[VFS_FNAME_LENGTH];
      mstrcpy(vfs_dirname, dirname);
      vfs_process_path(vfs_dirname);
      vfs_directory_llist_t *child_vfs_node;
      if(!vdir->children_llist)
      {
         ERRORMSG("minix3 missed something");
      }
      child_vfs_node = vdir->children_llist->head;
      for(; child_vfs_node != NULL; child_vfs_node = child_vfs_node->next)
      {
         if(samestr(child_vfs_node->vdir->name, vfs_dirname))
         {
            break;
         }
      }
      if(child_vfs_node == NULL)
      {
         DEBUGMSG("node not in tree");
      }
      else
      {
         vfs_remove_from_llist(vdir->children_llist, child_vfs_node);
         vfs_free_directory_llist_node(child_vfs_node);
      }
   }
   return;
}

void vfs_cat(vfs_directory_t *vdir, char *fname)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_fname[MX3_NAME_LEN];
      uint32_t inode_num;
      mstrcpy(mx3_fname, fname);
      vfs_path_to_minix3_path(mx3_fname);
      mx3_inode_t *dir, *target_inode;
      dir = kcalloc(sizeof(mx3_inode_t));
      target_inode = kcalloc(sizeof(mx3_inode_t));
      retval = minix3_get_inode(vdir->which_dev, vdir->inode_num, dir);
      if(retval)
      {
         ERRORMSG("cat error");
      }
      retval = minix3_get_inode_from_name(vdir->which_dev, dir, mx3_fname,
                                          &inode_num, target_inode);
      if(retval != MX3_RETURN_CODE_NOT_DIRECTORY && retval != 0)
      {
         ERRORMSG("cat error");
      }
      char *file_contents;
      file_contents = minix3_read_whole_file(vdir->which_dev, target_inode);
      if(file_contents == NULL)
      {
         ERRORMSG("cat error");
      }

      printf("cat-ing '%s'\n", fname);
      printf("%s\n", file_contents);

      kfree(file_contents);
      kfree(target_inode);
      kfree(dir);
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}

void vfs_append(vfs_directory_t *vdir, char *fname, char *buffer, uint32_t size)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      int retval;
      char mx3_fname[MX3_NAME_LEN];
      uint32_t inode_num;
      mstrcpy(mx3_fname, fname);
      vfs_path_to_minix3_path(mx3_fname);
      mx3_inode_t *dir, *target_inode;
      dir = kcalloc(sizeof(mx3_inode_t));
      target_inode = kcalloc(sizeof(mx3_inode_t));
      retval = minix3_get_inode(vdir->which_dev, vdir->inode_num, dir);
      if(retval)
      {
         ERRORMSG("append error");
      }
      retval = minix3_get_inode_from_name(vdir->which_dev, dir, mx3_fname,
                                          &inode_num, target_inode);
      if(retval != MX3_RETURN_CODE_NOT_DIRECTORY)
      {
         ERRORMSG("append error");
      }
      retval = minix3_append_to_file(vdir->which_dev, inode_num,
                                     (void*)buffer, size);
      if(retval)
      {
         ERRORMSG("append error");
      }

      kfree(target_inode);
      kfree(dir);
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}

/*
 * read in the data from a block device, assume it to be a bitmapped file,
 * and write it into the filesystem as a new file named fname
 */
void vfs_bmp_to_file(vfs_directory_t *vdir, char *fname, int bmp_dev)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      /* touch the file */
      vfs_touch(vdir, fname);
      char *buffer;
      int bsz; // bsz: block size
      bmp_img_header_t *bh;
      bmp_img_info_header_t *bmp_info;
      bsz = block_devices[bmp_dev].block_specific->block_size;
      buffer = kcalloc(bsz);
      MALLOC_CHECK(buffer);
      block_read_poll(bmp_dev, buffer,0, bsz);
      bh = (bmp_img_header_t *) buffer;
      bmp_info = ((bmp_img_info_header_t *) (buffer+sizeof(bmp_img_header_t)));

      if(bh->identifier != BMP_HEADER_MAGIC)
      {
         ERRORMSG("not a BMP file on this device");
      }
      // int bmp_width  = bmp_info->image_width;
      // int bmp_height = bmp_info->image_height;
      uint64_t image_size = bmp_info->image_size_uc;
      uint64_t offset_to_pixels = 54;
      uint64_t file_size = image_size + offset_to_pixels;
      // TODO: un-hardcode this
      //
      kfree(buffer);
      buffer = kcalloc(file_size);
      block_read_arbitrary(bmp_dev, buffer, 0, file_size);
      vfs_append(vdir, fname, buffer, file_size);
      kfree(buffer);
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}

/*
 * Read in a bitmap file and write it to a portion of the screen
 * dictated by the rectangle
 */
void vfs_bmp_file_to_gpu(vfs_directory_t *vdir, char *fname,
                         gpu_rectangle_t *r)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;


      char mx3_fname[MX3_NAME_LEN];
      uint32_t inode_num;
      int retval;
      mstrcpy(mx3_fname, fname);
      vfs_path_to_minix3_path(mx3_fname);
      mx3_inode_t *dir, *target_inode;
      dir = kcalloc(sizeof(mx3_inode_t));
      target_inode = kcalloc(sizeof(mx3_inode_t));
      retval = minix3_get_inode(vdir->which_dev, vdir->inode_num, dir);
      if(retval)
      {
         ERRORMSG("display error");
      }
      retval = minix3_get_inode_from_name(vdir->which_dev, dir, mx3_fname,
                                          &inode_num, target_inode);
      if(retval != MX3_RETURN_CODE_NOT_DIRECTORY && retval != 0)
      {
         ERRORMSG("display error");
      }
      char *file_contents;
      file_contents = minix3_read_whole_file(vdir->which_dev, target_inode);
      if(file_contents == NULL)
      {
         ERRORMSG("display error");
      }

      kfree(target_inode);
      kfree(dir);

      /* now write the file to screen */
      bmp_img_header_t *bh;
      bmp_img_info_header_t *bmp_info;
      bh = (bmp_img_header_t *) file_contents;
      bmp_info = ((bmp_img_info_header_t *) 
                  (file_contents+sizeof(bmp_img_header_t)));

      if(bh->identifier != BMP_HEADER_MAGIC)
      {
         ERRORMSG("not a BMP file on this device");
      }
      int bmp_width  = bmp_info->image_width;
      int bmp_height = bmp_info->image_height;
      uint64_t offset_to_pixels = 54;
      // uint64_t image_size = bmp_info->image_size_uc;
      // uint64_t file_size = image_size + offset_to_pixels;

      uint64_t buffer_index, which_pixel, max_pixel;
      pixel_t pixel_to_gpu;
      uint32_t pixel_row, pixel_col;
      uint64_t gpu_fb_offset;
      uint64_t *gpu_fb_offsets;

      max_pixel = bmp_width*bmp_height;

      for(which_pixel = 0; which_pixel < max_pixel; which_pixel++)
      {
         buffer_index = offset_to_pixels + which_pixel * 3;
         /* b */
         pixel_to_gpu.b = file_contents[buffer_index];
         /* g */
         pixel_to_gpu.g = file_contents[buffer_index + 1];
         /* r */
         pixel_to_gpu.r = file_contents[buffer_index + 2];
         /* a */
         pixel_to_gpu.a = 0xff;

         pixel_row = (max_pixel-which_pixel-1) / bmp_width;
         pixel_col = (which_pixel) % bmp_width;
         gpu_fb_offsets= gpu_get_fb_offset_into_rectangle(pixel_row, bmp_height,
                                                          pixel_col, bmp_width,
                                                          r);
         int i;
         for(i = 0; gpu_fb_offsets[i] != 0xffffffffffffffff; i++)
         {
            gpu_fb_offset = gpu_fb_offsets[i];
            gpu_device.fb.pixels[gpu_fb_offset] = pixel_to_gpu;
            if(i > 0)
            {
               break;
            }
         }
         kfree(gpu_fb_offsets);
      }

      kfree(file_contents);
      gpu_transfer_and_flush_some(r);
      break;
   default:
      ERRORMSG("unsupported file system");
      break;
   }
   return;
}



void vfs_show_directory_as_inode(vfs_directory_t *vdir)
{
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      mx3_inode_t *mx3_dir = kmalloc(sizeof(mx3_inode_t));
      
      minix3_get_inode(vdir->which_dev, vdir->inode_num, mx3_dir);
      minix3_print_inode(mx3_dir);

      kfree(mx3_dir);
      break;
   default:
      ERRORMSG("unsupported file system");
   }
}

void vfs_print_inode(vfs_directory_t *vdir, char *fname)
{
   if(fname == NULL || samestr(fname, ""))
   {
      vfs_show_directory_as_inode(vdir);
      return;
   }
   /* else */
   switch(vdir->fs_type)
   {
   case VFS_FS_MINIX3:
      ;
      mx3_inode_t *mx3_dir = kmalloc(sizeof(mx3_inode_t));
      mx3_inode_t *mx3_target = kmalloc(sizeof(mx3_inode_t));
      minix3_get_inode(vdir->which_dev, vdir->inode_num, mx3_dir);
      uint32_t target_inode_num;
      int retval;
      retval = minix3_get_inode_from_name(vdir->which_dev, mx3_dir,
                                          fname, &target_inode_num, mx3_target);
      if(retval == MX3_RETURN_CODE_NOT_FOUND)
      {
         printf("file '%s' not found\n", fname);
      }
      else if(retval == 0 || retval == MX3_RETURN_CODE_NOT_DIRECTORY)
      {
         minix3_print_inode(mx3_target);
      }
      else
      {
         ERRORMSG("retval");
      }

      kfree(mx3_dir);
      kfree(mx3_target);
      break;
   default:
      ERRORMSG("unsupported file system");
   }
}

void vfs_print_cached_tree(vfs_directory_t *base)
{
   printf("*cached* vfs directory tree\n");
   vfs_print_cached_tree_rec(base, 0);
}

void vfs_print_cached_tree_rec(vfs_directory_t *base, int level)
{
   int i;
   for(i = 0; i < level; i++)
   {
      if(i == level-1)
      {
         printf("|->");
      }
      else
      {
         printf("|  ");
      }
   }
   printf("%-30s", base->name);
   printf(" | ");
   printf("dev %d", base->which_dev);
   printf(" | ");
   switch(base->fs_type)
   {
   case VFS_FS_MINIX3:
      printf("minix3 ");
      break;
   case VFS_FS_OS:
      printf("vfs    ");
      break;
   default:
      printf("unknown");
      break;
   }
   printf(" | ");
   printf("inode %04d", base->inode_num);
   printf("\n");
   /* now print all children */
   if(!(base->children_llist))
   {
      return;
   }
   /* else */
   vfs_directory_llist_t *child_dir_node = base->children_llist->head;
   for(; child_dir_node != NULL; child_dir_node = child_dir_node->next)
   {
      vfs_print_cached_tree_rec(child_dir_node->vdir, level+1);
   }
}
