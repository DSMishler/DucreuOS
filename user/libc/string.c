#include <string.h>

int mstrlen(char *s)
{
   if(!s)
   {
      return 0;
   }
   if(*s == '\0')
   {
      return 0;
   }
   else
   {
      return (1+mstrlen(s+1));
   }
}
