#ifndef LOCK_TEST_NAND_H
#define LOCK_TEST_NAND_H
#include "gd32f30x_conf.h"
#define NAND_BLOCK_COUNT 1024
#define NAND_BLOCK_SIZE 64
#define NAND_SPARE_AREA_SIZE 64
#define NAND_OK 0
#define NAND_FAIL 1
u32 InitNandFlash(void);
u32 NandReadID(void);
u32 NandReadSpare(u32 block, u32 page, u32 column, u8 *buf, u32 len);
u32 NandReadPage(u32 block, u32 page, u32 column, u8 *buf, u32 len);
u32 NandWritePage(u32 block, u32 page, u32 column, u8 *buf, u32 len);
u32 NandCheckPage(u32 block, u32 page, u32 value, u32 mode);
#endif
