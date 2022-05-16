#include <utils.h>
#include <printf.h>
#include <stdint.h>
#include <kmalloc.h>

int msstrlen(char *a)
{
   if(*a == '\0' || *a == ' ')
   {
      return 0;
   }
   return(1 + msstrlen(a+1));
}

int mstrlen(char *a)
{
   if(*a == '\0')
   {
      return 0;
   }
   return(1 + mstrlen(a+1));
}

void mstrcpy(char *dest, char *src)
{
   if(dest == NULL || src == NULL)
   {
      printf("error: strcpy called with null pointer\n");
      return;
   }
   *dest = *src;
   if(*src == '\0')
   {
      return;
   }
   else
   {
      mstrcpy(dest+1,src+1);
   }
}

// copy over into a new string, treating spaces as terminators in 'src'
void msstrcpy(char *dest, char *src)
{
   if(dest == NULL || src == NULL)
   {
      printf("error: strcpy called with null pointer\n");
      return;
   }
   if(*src == '\0' || *src == ' ')
   {
      *dest = '\0'; // in this function, we copy over a '\0' instead of space
      return;
   }
   *dest = *src;
   msstrcpy(dest+1,src+1);
}

int mstrcmp(char *a, char *b)
{
   if(*a == *b && *a == '\0')
   {
      return 0;
   }
   else if(*a - *b != 0)
   {
      return *a - *b;
   }
   else
   {
      return mstrcmp(a+1,b+1);
   }
}

int msstrcmp(char *a, char *b)
{
   if((*a == '\0' || *a == ' ') && 
      (*b == '\0' || *b == ' '))
   {
      return 0;
   }
   else if(*a - *b != 0)
   {
      return *a - *b;
   }
   else
   {
      return msstrcmp(a+1,b+1);
   }
}

int samestr(char *a, char *b)
{
   return(!(mstrcmp(a,b)));
}

// strtac: the reverse of strcat.
// Instead of appending b to a, it prepends a to b, assuming adequate space
// is available.
void mstrtac(char *a, char *b)
{
   if(!(a) || !(b))
   {
      ERRORMSG("NULL strings");
      return;
   }
   int lena = mstrlen(a);
   int lenb = mstrlen(b);
   int i;
   // shift over to make space for string a
   // include copying over the string terminator
   for(i = lenb; i >= 0; i--)
   {
      b[i+lena] = b[i];
   }
   // copy in string a
   for(i = 0; i < lena; i++)
   {
      b[i] = a[i];
   }
   return;
}

int countargs(char *cmd)
{
   int i;
   int nargs = 0;
   enum states
   {
      STATE_IN_WORD,
      STATE_IN_SPACE,
   };
   int state = STATE_IN_SPACE;
   for(i = 0; cmd[i] != '\0'; i++)
   {
      if(state == STATE_IN_SPACE)
      {
         if(cmd[i] == ' ')
         {
            ;
         }
         else
         {
            state = STATE_IN_WORD;
            nargs++;
         }
      }
      else if(state == STATE_IN_WORD)
      {
         if(cmd[i] == ' ')
         {
            state = STATE_IN_SPACE;
         }
         else
         {
            ;
         }
      }
   }
   return nargs;
}

char **make_argv(char *cmd)
{
   char **argv = kmalloc(sizeof(char *) * countargs(cmd));
   MALLOC_CHECK(argv);
   enum states
   {
      STATE_IN_WORD,
      STATE_IN_SPACE,
   };
   int state = STATE_IN_SPACE;
   int which_arg = 0; // will go to 1 *after* we process the first word.
   int i;
   for(i = 0; cmd[i] != '\0'; i++)
   {
      if(state == STATE_IN_SPACE)
      {
         if(cmd[i] == ' ')
         {
            ;
         }
         else
         {
            state = STATE_IN_WORD;
            argv[which_arg] = kmalloc(sizeof(char)*(msstrlen(cmd+i)+1));
            MALLOC_CHECK(argv[which_arg]);
            msstrcpy(argv[which_arg], cmd+i);
            which_arg++;
            // optimize by advancing i
            // i += (msstrlen(cmd+i)-1);
         }
      }
      else if(state == STATE_IN_WORD)
      {
         if(cmd[i] == ' ')
         {
            state = STATE_IN_SPACE;
         }
         else
         {
            ;
         }
      }
   }
   if(which_arg != countargs(cmd))
   {
      ERRORMSG("programmer or input");
   }
   return argv;
}

// one design alternative to the argv structure is to have it NULL-terminated,
// but matching up with the linux/C standard for argv seems like the better bet
void free_argv(char **argv, int nargs)
{
   int i;
   for(i = 0; i < nargs; i++)
   {
      kfree(argv[i]);
   }
   kfree(argv);
}


int samecmd(char *cmd, char *ref)
{
   return(!(msstrcmp(cmd,ref)));
}

unsigned long int matox(char *a)
{
   int i;
   unsigned long int result = 0;
   unsigned long int power = 1;
   int len = 0;
   unsigned long int curnum;

   // ignore the hex indicator 0x in front of a hex number
   // check this way first to avoid 1-length strings ruining the day
   if(*a == '0')
   {
      if(*(a+1) == 'x' || *(a+1) == 'X')
      {
         a += 2;
      }
   }

   while((a[len] >= '0' && a[len] <= '9') ||
         (a[len] >= 'a' && a[len] <= 'f') ||
         (a[len] >= 'A' && a[len] <= 'F'))
   {
      len++;
   }
   if(len != mstrlen(a))
   {
      ERRORMSG("argument");
      return 0;
   }

   for(i = len - 1; i >= 0; i--)
   {
      if(a[i] >= '0' && a[i] <= '9')
      {
         curnum = a[i] - '0';
      }
      else if(a[i] >= 'a' && a[i] <= 'f')
      {
         curnum = a[i] - 'a' + 10;
      }
      else // if(a[i] >= 'A' && a[i] <= 'F')
      {
         curnum = a[i] - 'A' + 10;
      }
      result += power * curnum;
      power *= 16;
   }

   return result;
}

int matoi(char *a)
{
   int i;
   int result = 0;
   int power = 1;
   int len = 0;
   int negate = 0;
   int curnum;

   if(a[0] == '-')
   {
      negate = 1;
      a++;
   }

   while((a[len] >= '0' && a[len] <= '9'))
   {
      len++;
   }
   if(len != mstrlen(a))
   {
      ERRORMSG("argument");
      return 0;
   }

   for(i = len - 1; i >= 0; i--)
   {
      if(a[i] >= '0' && a[i] <= '9')
      {
         curnum = a[i] - '0';
      }
      result += power * curnum;
      power *= 10;
   }

   if(negate)
   {
      result = -result;
   }
   return result;
}

unsigned int isqrt(unsigned int a)
{
   unsigned int i;
   for(i = 1; i < a; i++)
   {
      if(i*i > a)
      {
         break;
      }
   }
   return i-1;
}

// TODO: visit and possibly refactor

void *memset(void *dst, char data, int size)
{
   int64_t i;
   long *ldst = (long *)dst;
   char *cdst;
   char l[] = {data, data, data, data, data, data, data, data};

   int num_8_byte_copies = size / 8;
   int num_1_byte_copies = size % 8;

   for (i = 0; i < num_8_byte_copies; i++)
   {
      *ldst++ = *((long*)l);
   }

   cdst = (char *)ldst;

   for (i = 0;i < num_1_byte_copies;i++) {
      *cdst++ = l[0];
   }

   return dst;
}

void *memcpy(void *dst, const void *src, int size)
{
   int i;
   char *cdst;
   const char *csrc;
   long *ldst = (long *)dst;
   const long *lsrc = (long *)src;

   int num_8_byte_copies = size / 8;
   int num_1_byte_copies = size % 8;

   for (i = 0; i < num_8_byte_copies; i++) {
      *ldst++ = *lsrc++;
   }

   cdst = (char *)ldst;
   csrc = (char *)lsrc;

   for (i = 0;i < num_1_byte_copies;i++) {
      *cdst++ = *csrc++;
   }

   return dst;
}

int memcmp(const void *haystack, const void *needle, int size)
{
   const char *hay = (char *)haystack;
   const char *need = (char *)needle;
   int64_t i;

   for (i = 0; i < size; i++)
   {
      if (hay[i] != need[i])
      {
         return 0;
      }
   }

   return 1;
}
