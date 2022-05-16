#include <input.h>
#include <event.h>
#include <printf.h>
#include <stdint.h>

char input_key_code_to_char(uint32_t code)
{
   char c;
   switch(code)
   {
   case IE_KEY_CODE_A:
      c = 'a';
      break;
   case IE_KEY_CODE_B:
      c = 'b';
      break;
   case IE_KEY_CODE_C:
      c = 'c';
      break;
   case IE_KEY_CODE_D:
      c = 'd';
      break;
   case IE_KEY_CODE_E:
      c = 'e';
      break;
   case IE_KEY_CODE_F:
      c = 'f';
      break;
   case IE_KEY_CODE_G:
      c = 'g';
      break;
   case IE_KEY_CODE_H:
      c = 'h';
      break;
   case IE_KEY_CODE_I:
      c = 'i';
      break;
   case IE_KEY_CODE_J:
      c = 'j';
      break;
   case IE_KEY_CODE_K:
      c = 'k';
      break;
   case IE_KEY_CODE_L:
      c = 'l';
      break;
   case IE_KEY_CODE_M:
      c = 'm';
      break;
   case IE_KEY_CODE_N:
      c = 'n';
      break;
   case IE_KEY_CODE_O:
      c = 'o';
      break;
   case IE_KEY_CODE_P:
      c = 'p';
      break;
   case IE_KEY_CODE_Q:
      c = 'q';
      break;
   case IE_KEY_CODE_R:
      c = 'r';
      break;
   case IE_KEY_CODE_S:
      c = 's';
      break;
   case IE_KEY_CODE_T:
      c = 't';
      break;
   case IE_KEY_CODE_U:
      c = 'u';
      break;
   case IE_KEY_CODE_V:
      c = 'v';
      break;
   case IE_KEY_CODE_W:
      c = 'w';
      break;
   case IE_KEY_CODE_X:
      c = 'x';
      break;
   case IE_KEY_CODE_Y:
      c = 'y';
      break;
   case IE_KEY_CODE_Z:
      c = 'z';
      break;
   default:
      printf("unknown key '%d'\n", code);
      break;
   }
   return c;
}

/* same as OS's copy */
void input_print_event(input_event_t *iev)
{
   printf("input event at 0x%012lx\n", iev);
   printf("   type : 0x%04x\n", iev->type);
   printf("   code : 0x%04x\n", iev->code);
   printf("   value: 0x%08x\n", iev->value);
}
