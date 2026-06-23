#ifndef __SERIAL_H

#define __SERIAL_H

void Serial_Init(void);

uint16_t SmallCar_GetRxData(void);

uint16_t SmallCat_GetRxFlag(void);

void USART1_IRQHandler(void);


#endif


