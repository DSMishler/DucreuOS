#pragma once
// m: my version of standard library function that isn't here for some reason.
// s: acknowledge spaces
int msstrlen(char *a);
int mstrlen(char *a);
void mstrcpy(char *dest, char *src);
void msstrcpy(char *dest, char *src);
int mstrcmp(char *a, char *b);
int msstrcmp(char *a, char *b);
int samestr(char *a, char *b);
int samecmd(char *cmd, char *ref);
void mstrtac(char *a, char *b);

int countargs(char *cmd);
char **make_argv(char *cmd);
void free_argv(char **argv, int nargs);

unsigned long int matox(char *a);
int matoi(char *a);

unsigned int isqrt(unsigned int a);

void *memset(void *dst, char data, int size);
void *memcpy(void *dst, const void *src, int size);
int memcmp(const void *haystack, const void *needle, int size);

// these macros can be used with or without semicolons. ;; is fine
#define ERRORMSG(msg)                                                      \
              printf("   ERROR: ");                                        \
              printf("%s ", msg);                                          \
              printf("error in file [%s] at line %u.\n",__FILE__,__LINE__);
#define DEBUGMSG(msg)                                                      \
              printf("   DEBUG: [%s] at line %03u: ",__FILE__,__LINE__);   \
              printf("%s\n", msg);
// more expedient than a function to tell the user which file and where
#define MALLOC_CHECK(x) if((void *)x == (void *)0){ ERRORMSG("malloc");}
