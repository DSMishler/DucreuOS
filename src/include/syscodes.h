#pragma once
// main important thing is we want exit to be zero
// In the event that we'd like to support linux-based applications, copy all
// of the linux system calls!
#define SYSCALL_EXIT       0
#define SYSCALL_PUTCHAR    1
#define SYSCALL_GETCHAR    2
#define SYSCALL_CONSOLE    3
#define SYSCALL_SLEEP      4



#define SYSCALL_GET_INPUT  14
#define SYSCALL_GET_REGION 15
#define SYSCALL_ADD_REGION 16

#define SYSCALL_DRAW_RECTANGLE   20
#define SYSCALL_DRAW_CIRCLE      21
#define SYSCALL_GET_SCREEN_X     22
#define SYSCALL_GET_SCREEN_Y     23
