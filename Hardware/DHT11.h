#ifndef __DHT11_H

#define __DHT11_H

#include "stm32f10x.h"                  // Device header
#define u8 unsigned char

u8 DHT11_Mode(u8 Mode);

void DHT11_Ret(void);

u8 DHT11_check(void);

u8 DHT11_Init(void);


u8 DHT11_Readbit(void);


u8 DHT11_ReadBay(void);

u8 DHT11_ReadData(u8 *tim,u8 *temp);


#endif


