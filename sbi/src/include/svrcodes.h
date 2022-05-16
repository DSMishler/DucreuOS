#pragma once
// Anything special about the numbering scheme? I think we just get to
//  choose everything. I'll copy Dr. Marz' numbering scheme, but 215 for
//  poweroff seems strange.
#define SVRCALL_PUTCHAR  10
#define SVRCALL_GETCHAR  11
#define SVRCALL_POWEROFF 215

#define SVRCALL_GET_HART_STATUS  1
#define SVRCALL_HART_START       2
#define SVRCALL_HART_STOP        3
#define SVRCALL_WHO_AM_I         4

#define SVRCALL_GET_CLINT_TIME     15
#define SVRCALL_CLINT_SET_TIME     16
#define SVRCALL_CLINT_ADD_TIME     17
#define SVRCALL_CLINT_ACKNOWLEDGE  18
#define SVRCALL_CLINT_SHOW_CMP     19
