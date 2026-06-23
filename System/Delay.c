#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
// 初始化 TIM4 为 1us 计数
void Delay_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    // 72MHz / 72 = 1MHz → 1us 加1
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;

    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
    TIM_Cmd(TIM4, ENABLE);
}

// 微秒延时
void Delay_us(uint32_t xus)
{
    uint32_t start = TIM4->CNT;
    while ((TIM4->CNT - start) < xus);
}

// 毫秒延时
void Delay_ms(uint32_t xms)
{
    while (xms--)
    {
        Delay_us(1000);
    }
}

// 秒延时
void Delay_s(uint32_t xs)
{
    while (xs--)
    {
        Delay_ms(1000);
    }
}
void DelayXms(uint32_t ms)
{
    Delay_ms(ms);   // 调用你自己原来的延时函数
}

void MYDelay_ms(uint32_t ms)
{
    if(xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
    else
    {
        Delay_ms(ms);
    }
}