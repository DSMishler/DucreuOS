#pragma once
#include <stdint.h>
// for Linux stat

#define STAT_MODE_MASK      0xf000
#define STAT_MODE_SOCKET    0xc000
#define STAT_MODE_LINK      0xa000
#define STAT_MODE_REGULAR   0x8000
#define STAT_MODE_BLOCK     0x6000
#define STAT_MODE_DIRECTORY 0x4000
#define STAT_MODE_FIFO      0x1000

#define STAT_USR_R 00400
#define STAT_USR_W 00200
#define STAT_USR_X 00100

#define STAT_GRP_R 00040
#define STAT_GRP_W 00020
#define STAT_GRP_X 00010

#define STAT_OTH_R 00004
#define STAT_OTH_W 00002
#define STAT_OTH_X 00001

