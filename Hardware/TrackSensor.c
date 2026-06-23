#include "stm32f10x.h"                  // Device header

void TrackSensor_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_1 | GPIO_Pin_7 | GPIO_Pin_6 | GPIO_Pin_12;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStruct);
}
//左1
uint8_t Track_L1(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_1);
}
//左2
uint8_t Track_L2(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_7);
}
//左3
uint8_t Track_L3(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_6);
}
//左4
uint8_t Track_L4(void)
{
	return GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_12);
}


