#include "stm32f10x.h"

void MYADC_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1,ENABLE);

    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitTypeDef GPIO_InitStructrue;
    GPIO_InitStructrue.GPIO_Mode=GPIO_Mode_AIN;
    GPIO_InitStructrue.GPIO_Pin=GPIO_Pin_7;
    GPIO_InitStructrue.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&GPIO_InitStructrue);

    ADC_RegularChannelConfig(
        ADC1,
        ADC_Channel_7,
        1,
        ADC_SampleTime_239Cycles5
    );

    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_ContinuousConvMode=DISABLE;// 连续转换模式：DISABLE（单次转换），ENABLE则持续转换
	ADC_InitStructure.ADC_DataAlign=ADC_DataAlign_Right;// 数据对齐方式：右对齐（低12位有效，符合常规使用习惯）
	ADC_InitStructure.ADC_ExternalTrigConv=ADC_ExternalTrigConv_None;// 外部触发转换：None（软件触发，不用外部引脚/定时器触发）
	ADC_InitStructure.ADC_Mode=ADC_Mode_Independent;// ADC模式：独立模式（ADC1单独工作，多ADC同步时用其他模式）
	ADC_InitStructure.ADC_NbrOfChannel=1;// 转换通道数：1（只采集1个通道）
	ADC_InitStructure.ADC_ScanConvMode=DISABLE;// 扫描模式：DISABLE（单通道无需扫描），ENABLE则多通道扫描
    ADC_Init(ADC1,&ADC_InitStructure);

    ADC_Cmd(ADC1,ENABLE);//启动ADC1

    //对ADC进行校准
    ADC_ResetCalibration(ADC1);//重置ADC校准寄存器
    while(ADC_GetResetCalibrationStatus(ADC1)==SET);//等待重置校准完成，直到等到状态位为RESET
    ADC_StartCalibration(ADC1);//启动ADC校准
    while(ADC_GetCalibrationStatus(ADC1)==SET);//等待ADC校准完成，直到等到状态位为RESET
}

uint16_t ADC_GetValue(void)
{
    ADC_SoftwareStartConvCmd(ADC1,ENABLE);//软件触发ADC开始转换
    while(ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC)==RESET);//等待转化完成
    return ADC_GetConversionValue(ADC1);//读取转换结果

}