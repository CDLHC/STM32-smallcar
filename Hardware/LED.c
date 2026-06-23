#include "stm32f10x.h"                  // Device header
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode= GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_12 | GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode= GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_5 | GPIO_Pin_3 ;
	GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Mode= GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Speed= GPIO_Speed_50MHz;
	GPIO_Init(GPIOC,&GPIO_InitStructure);
	
	GPIO_SetBits(GPIOA,GPIO_Pin_5 | GPIO_Pin_3 | GPIO_Pin_12 | GPIO_Pin_15);
	GPIO_SetBits(GPIOC,GPIO_Pin_13);
}
void LED_Left_ON(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_5);
}
void LED_Left_OFF(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_5);
}
void LED_Left_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_5)==0)//读取某一个端口当前输出状态
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_5);
	}
	else
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_5);
	}
}
void LED_Right_ON(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_3);
}
void LED_Right_OFF(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_3);
}
void LED_Right_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_3)==0)//读取某一个端口当前输出状态
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_3);
	}
	else
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_3);
	}
}
void LED_Front_ON(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_12);
	GPIO_ResetBits(GPIOA,GPIO_Pin_15);
}
void LED_Front_OFF(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_12);
	GPIO_SetBits(GPIOA,GPIO_Pin_15);
}
void LED_Front1_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_12)==0)//读取某一个端口当前输出状态
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_12);
	}
	else
	{
		GPIO_ResetBits(GPIOA,GPIO_Pin_12);
	}
}

void LED_Front2_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_15)==0)//读取某一个端口当前输出状态
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_15);
	}
	else
	{
		GPIO_ResetBits(GPIOA,GPIO_Pin_15);
	}
}

void LED13_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOC,GPIO_Pin_13)==0)
	{
		GPIO_SetBits(GPIOC,GPIO_Pin_13);
	}
	else
	{
		GPIO_ResetBits(GPIOC,GPIO_Pin_13);
	}
}