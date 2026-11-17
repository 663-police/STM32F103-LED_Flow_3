#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include <stdint.h>

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowString(uint8_t x, uint8_t page, const char *str);
void OLED_ShowNum(uint8_t x, uint8_t page, uint32_t num, uint8_t len);

#endif
