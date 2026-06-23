#include "stm32f10x.h"
#include "Delay.h"

uint16_t Num;

void HCSR04_Init(void)
{
	// 先延时，让模块上电稳定
	Delay_ms(100);
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStruct;
	
	// Trig — PB14
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	// Echo — PB15
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_15;
	GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	//先关闭定时器，清空所有状态
	TIM_Cmd(TIM1, DISABLE);       
	TIM1->CNT = 0;               
	TIM1->SR = 0;                
	
	//配置时基单元
	TIM_InternalClockConfig(TIM1);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;
	TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStruct.TIM_Period = 65535;
	TIM_TimeBaseInitStruct.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStruct);
	
	//强制清空计数器
	TIM1->CNT = 0;
	
	// 开启定时器
	TIM_Cmd(TIM1, ENABLE);
	
	//强制拉低 Trig，绝对避免上电乱触发
	GPIO_ResetBits(GPIOB, GPIO_Pin_14);
	Delay_ms(10);
}

float HCSR04_Distance(void)
{
	// 每次测距前都复位 Trig + 清空定时器，彻底根治卡死
	GPIO_ResetBits(GPIOB, GPIO_Pin_14);
	Delay_us(10);
	TIM1->CNT = 0;
	GPIO_SetBits(GPIOB, GPIO_Pin_14);
	Delay_us(20);
	GPIO_ResetBits(GPIOB, GPIO_Pin_14);
	// 等待Echo上升沿，带超时，永不卡死
	uint32_t timeout = 0;
	while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 0)
	{
		timeout++;
		if(timeout > 80000) return 0;//超时保护
	}

	TIM1->CNT = 0;

	// 等待Echo下降沿
	timeout = 0;
	while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15) == 1)
	{
		timeout++;
		if(timeout > 80000) return 0;//超时保护
	}

	Num = TIM_GetCounter(TIM1);
	float Distance = Num / 58.0f;

	return Distance;
}



