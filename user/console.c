#include <string.h>
#include <os.h>
#include <printf.h>
#include <input.h>
#include <event.h>
#include "../src/include/utils.h"

// TODO: The size of this elf file is hard-coded in elfread in os's main.
//       Fix that!
//
typedef enum
{
   input_state_init,
   input_state_default,
   input_state_lclick,
   input_state_xpos,
   input_state_ypos,
   input_state_expect_sync,
} input_state_states_t;

void paint_default_regions(void)
{
   uint32_t screen_width;
   uint32_t screen_height;

   screen_width  = os_get_screen_x();
   screen_height = os_get_screen_y();
   gpu_rectangle_t test_rect;
   pixel_t pixel_red   = {255, 0, 0, 255};
   pixel_t pixel_green = {0, 255, 0, 255};
   pixel_t pixel_blue  = {0, 0, 255, 255};
   test_rect.x = 0;
   test_rect.y = 0;
   test_rect.width  = screen_width  * 10 / 100;
   test_rect.height = screen_height * 10 / 100;
   os_draw_rectangle(&test_rect, &pixel_red);
   os_add_region(0, 10, 0, 10, 1);
   test_rect.x = screen_width * 10/ 100;;
   test_rect.y = 0;
   test_rect.width  = screen_width  * 10 / 100;
   test_rect.height = screen_height * 10 / 100;
   os_draw_rectangle(&test_rect, &pixel_green);
   os_add_region(10, 20, 0, 10, 2);
   test_rect.x = screen_width * 20/ 100;;
   test_rect.y = 0;
   test_rect.width  = screen_width  * 10 / 100;
   test_rect.height = screen_height * 10 / 100;
   os_draw_rectangle(&test_rect, &pixel_blue);
   os_add_region(20, 30, 0, 10, 3);
}


int main(void)
{
   printf("I'm the user process - hello world!\n");
   uint32_t screen_width;
   uint32_t screen_height;

   screen_width  = os_get_screen_x();
   screen_height = os_get_screen_y();

   paint_default_regions();

   pixel_t pixel_red   = {255, 0, 0, 255};
   pixel_t pixel_green = {0, 255, 0, 255};
   pixel_t pixel_blue  = {0, 0, 255, 255};

   gpu_circle_t test_circ;
   test_circ.x = 205;
   test_circ.y = 190;
   test_circ.radius = 22;

   os_draw_circle(&test_circ, &pixel_red);


   int input_parse_size = 50;
   input_event_t i_events[input_parse_size];
   input_state_states_t state = input_state_default;
   input_cursor_position_t icp;
   pixel_t current_color = pixel_red;
   int lmouse_pressed = 0;
   int region;
   icp.x = icp.y = 0;
   while(1)
   {
      input_event_t i_event;
      int number_to_draw = input_parse_size;
      int i;
      os_get_input(i_events, number_to_draw);
      for(i = 0; i < number_to_draw; i++)
      {
         i_event = i_events[i];
         switch(state)
         {
         case input_state_init:
         case input_state_default:
            if(i_event.type == IE_TYPE_SYNC)
            {
               ;
            }
            else if(i_event.type == IE_TYPE_TABLET)
            {
               if(i_event.code == IE_TABLET_CODE_X)
               {
                  icp.x = i_event.value;
                  state = input_state_xpos;
               }
               else
               {
                  DEBUGMSG("skipping unexpected code");
                  input_print_event(&i_event);
               }
            }
            else if(i_event.type == IE_TYPE_KEY)
            {
               if(i_event.code == IE_KEY_CODE_BUTTON_LMOUSE)
               {
                  if(i_event.value == IE_KEY_VALUE_PRESS)
                  {
                     lmouse_pressed = 1;
                     region = os_get_region(icp.x, icp.y);
                     if(region == 1)
                     {
                        current_color = pixel_red;
                     }
                     else if(region == 2)
                     {
                        current_color = pixel_green;
                     }
                     else if(region == 3)
                     {
                        current_color = pixel_blue;
                     }
                     else if(region == 0)
                     {
                        test_circ.x = icp.x*screen_width/0x8000;
                        test_circ.y = icp.y*screen_height/0x8000;
                        test_circ.radius = 5;
                        os_draw_circle(&test_circ, &current_color);
                     }
                     // gpu_circle_t c;
                     // c.x = x_pos;
                     // c.y = y_pos;
                     // c.radius = 5;
                     // pixel_t first_c = {rand8(),rand8(),rand8(),255};
                     // gpu_fill_circle(&(gpu_device.fb), &c, first_c);
                     // gpu_rectangle_t r;
                     // gpu_circle_to_rectangle(&r, &c);
                     // gpu_transfer_and_flush_some(&r);
                     // int region_value;
                     // printf("region value: %d\n", region_value);
                     // state = input_state_lclick;
                     ;
                  }
                  else
                  {
                     lmouse_pressed = 0;
                  }
               }
               else
               {
                  if(i_event.value == IE_KEY_VALUE_PRESS)
                  {
                     os_putchar(input_key_code_to_char(i_event.code));
                  }
                  else
                  {
                     ;
                     // DEBUGMSG("release");
                  }
               }
            }
            else
            {
               DEBUGMSG("unexpected event");
            }
            break;
         case input_state_xpos:
            if(i_event.type == IE_TYPE_TABLET)
            {
               if(i_event.code == IE_TABLET_CODE_Y)
               {
                  icp.y = i_event.value;
                  state = input_state_ypos;
                  if(lmouse_pressed)
                  {
                     region = os_get_region(icp.x, icp.y);
                     if(region == 0)
                     {
                        test_circ.x = icp.x*screen_width/0x8000;
                        test_circ.y = icp.y*screen_height/0x8000;
                        test_circ.radius = 5;
                        os_draw_circle(&test_circ, &current_color);
                     }
                  }
               }
               else
               {
                  DEBUGMSG("skipping unexpected code");
               }
            }
            else
            {
               DEBUGMSG("unexpected event");
            }
            break;
         case input_state_ypos:
         case input_state_lclick:
         case input_state_expect_sync:
            if(i_event.type == IE_TYPE_SYNC)
            {
               state = input_state_default;
            }
            else
            {
               DEBUGMSG("unexpected event (expected sync)");
            }
            break;
         default:
            ERRORMSG("unknown state");
            break;
         }
      }
      os_sleep(1);
   }
   // int i;
   // for(i = 0; i < 10000; i+=10)
   // {
      // printf("sleeping for %d quantums\n", i);
      // os_sleep(i);
   // }


   // printf("exiting back to OS console\n");
   // os_console();
   # if 0
   input_event_t input[8];
   printf("getting input events\n");
   os_get_input(input, 8);
   int i;
   for(i = 0; i < 8; i++)
   {
      printf("input event %d: 0x%016lx\n", i, input[i]);
   }
   #endif 
   // asm volatile("j .");
   return 0;
}

