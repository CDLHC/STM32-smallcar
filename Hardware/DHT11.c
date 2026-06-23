#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#define u8 unsigned char

void DHT11_Mode(u8 Mode)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	GPIO_InitTypeDef GPIO_InitStruct;
	if(Mode)//1
	{
		GPIO_InitStruct.GPIO_Mode=GPIO_Mode_Out_PP;
		GPIO_InitStruct.GPIO_Pin=GPIO_Pin_13;
		GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	}
	else//0
	{
		GPIO_InitStruct.GPIO_Mode=GPIO_Mode_IPU;
		GPIO_InitStruct.GPIO_Pin=GPIO_Pin_13;
		GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	}
		
	GPIO_Init(GPIOB,&GPIO_InitStruct);
}


void DHT11_Ret(void)
{
	DHT11_Mode(1);
	GPIO_ResetBits(GPIOB,GPIO_Pin_13);
	Delay_ms(20);
	GPIO_SetBits(GPIOB,GPIO_Pin_13);
	Delay_us(30);
}


u8 DHT11_check(void)
{
	u8 count=0;
	DHT11_Mode(0);
	while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13) && count<100)
	{
		count++;
		Delay_us(1);
	}
	if(count>=100)
	{
		return 1;
	}
	count=0;
	while(!GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13) && count<100)
	{
		count++;
		Delay_us(1);
	}
	if(count>=100)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}



u8 DHT11_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Pin=GPIO_Pin_13;
	GPIO_InitStruct.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOB,&GPIO_InitStruct);
	
	GPIO_SetBits(GPIOB,GPIO_Pin_13);
	
	DHT11_Ret();
	return DHT11_check();
}



u8 DHT11_Readbit(void)
{
	u8 count=0;
	//起始为高电平，等待电平变化，变化为低电平
	while(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13) && count<100)
	{
		count++;
		Delay_us(1);
	}
	count=0;
	//现在为低电平，等待电平变化，变化为高电平
	while(!GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13) && count<100)
	{
		count++;
		Delay_us(1);
	}
	Delay_us(40);
	if(GPIO_ReadInputDataBit(GPIOB,GPIO_Pin_13))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}



u8 DHT11_ReadBay(void)
{
	uint8_t i;
	u8 data=0;
	for(i=0;i<8;i++)
	{
		data<<=1;
		data |= DHT11_Readbit();
	}
	return data;
}



u8 DHT11_ReadData(u8 *tim,u8 *temp)
{
	u8 arr[5];
	uint16_t i;
	
	taskENTER_CRITICAL();
	
	DHT11_Ret();
	if(DHT11_check()==0)
	{
		for(i=0;i<5;i++)
		{
			arr[i]=DHT11_ReadBay();
		}
		
		taskEXIT_CRITICAL();
		
		if(arr[0]+arr[1]+arr[2]+arr[3] == arr[4])
		{
			*tim=arr[0];
			*temp=arr[2];
			return 0;//成功
		}
	}
	else
	{
		taskEXIT_CRITICAL();
	}
	return 1;//失败
}



