/**
  ************************************************************
  * 文件名称    esp8266.c
  *
  * 作者        张继勇
  *
  * 日期        2017-05-08
  *
  * 版本        V1.0
  *
  * 说明        ESP8266驱动文件
  *
  * 修改记录
  ************************************************************
**/

// 单片机头文件
#include "stm32f10x.h"

// 外部设备驱动
#include "esp8266.h"

// 硬件驱动
#include "delay.h"
#include "usart.h"

// C库
#include <string.h>
#include <stdio.h>


#define ESP8266_WIFI_INFO		"AT+CWJAP=\"LFD\",\"LFD2005323801\"\r\n"


unsigned char esp8266_buf[512];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;


//==========================================================
//  函数名称    ESP8266_Clear
//
//  函数功能    清空接收缓冲区
//
//  入口参数    无
//
//  返回参数    无
//
//  说明
//==========================================================
void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;

}

//==========================================================
//  函数名称    ESP8266_WaitRecive
//
//  函数功能    等待接收完成
//
//  入口参数    无
//
//  返回参数    REV_OK-接收完成		REV_WAIT-接收超时未完成
//
//  说明        循环调用检测是否接收完成
//==========================================================
_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							// 接收计数器为0，说明没有正在接收的数据，直接返回等待
		return REV_WAIT;

	if(esp8266_cnt == esp8266_cntPre)				// 与上一次的值相同，说明接收完成
	{
		esp8266_cnt = 0;							// 清空接收计数器

		return REV_OK;								// 返回接收完成标志
	}

	esp8266_cntPre = esp8266_cnt;					// 保存当前值

	return REV_WAIT;								// 返回接收未完成标志

}

//==========================================================
//  函数名称    ESP8266_SendCmd
//
//  函数功能    发送AT指令
//
//  入口参数    cmd-命令
//              res-需要等待的返回关键字
//
//  返回参数    0-成功  1-失败
//
//  说明
//==========================================================
_Bool ESP8266_SendCmd(char *cmd, char *res)
{

	unsigned char timeOut = 200;

	Usart_SendString(USART3, (unsigned char *)cmd, strlen((const char *)cmd));

	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)							// 如果接收到数据
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)		// 判断是否包含返回关键字
			{
				ESP8266_Clear();									// 清空缓冲区

				return 0;
			}
		}

		MYDelay_ms(10);
	}

	return 1;

}

//==========================================================
//  函数名称    ESP8266_SendData
//
//  函数功能    发送数据
//
//  入口参数    data-数据
//              len-长度
//
//  返回参数    无
//
//  说明
//==========================================================
_Bool ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[32];

	ESP8266_Clear();								// 清空接收缓冲区
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		// 发送AT指令
	if(!ESP8266_SendCmd(cmdBuf, ">"))				// 收到">"时才可以发送数据
	{
		Usart_SendString(USART3, data, len);		// 向串口发送数据
		return 0;									// 发送成功
	}
	return 1;										// 发送失败

}

//==========================================================
//  函数名称    ESP8266_GetIPD
//
//  函数功能    获取平台返回的数据
//
//  入口参数    等待超时时间(单位10ms)
//
//  返回参数    平台返回的原始数据
//
//  说明        不同平台设备返回的格式不同，需要区别处理
//              AT ESP8266的返回格式为  "+IPD,x:yyy"  x为数据长度，yyy为返回数据
//==========================================================
unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;

	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								// 接收完成
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				// 查找IPD头
			if(ptrIPD == NULL)											// 如果没有找到合法的IPD头则延时，否则等待未完成会超出设定的超时时间
			{
				//UsartPrintf(USART_DEBUG, "\"IPD\" not found\r\n");
			}
			else
			{
				ptrIPD = strchr(ptrIPD, ':');							// 找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;

			}
		}
		MYDelay_ms(5);													// 延时等待
	} while(timeOut--);
	return NULL;														// 超时未找到，返回空指针

}

//==========================================================
//  函数名称    ESP8266_Init
//
//  函数功能    初始化ESP8266
//
//  入口参数    无
//
//  返回参数    无
//
//  说明
//==========================================================
 void ESP8266_Init(void)
  {
      ESP8266_Clear();
      uint16_t count;

      // 1. AT检测，最多10次重试
      OLED_ShowString(0,0, "1.AT...",8);
      count = 0;
      while(ESP8266_SendCmd("AT\r\n", "OK"))
      {
          if(++count >= 10)
          {
              UsartPrintf(USART2, "ESP8266 AT检测失败, 即将进入离线模式\r\n");
              OLED_ShowString(0,0, "AT FAIL",8);
              return;     // 直接退出，跳过后续初始化
          }
          MYDelay_ms(500);
      }

      // 2. CWMODE
      OLED_ShowString(0,2, "2.CWMODE...",8);
      count = 0;
      while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
      {
          if(++count >= 10)
          {
              UsartPrintf(USART2, "ESP8266 CWMODE失败\r\n");
              OLED_ShowString(0,2, "CWMODE FAIL",8);
              return;
          }
          MYDelay_ms(500);
      }

      // 3. DHCP
      OLED_ShowString(0,4, "3.AT+CWDHCP...",8);
      count = 0;
      while(ESP8266_SendCmd("AT+CWDHCP=1,1\r\n", "OK"))
      {
          if(++count >= 10)
          {
              UsartPrintf(USART2, "ESP8266 DHCP失败\r\n");
              OLED_ShowString(0,4, "DHCP FAIL",8);
              return;
          }
          MYDelay_ms(500);
      }

      // 4. 连WiFi，最多30次重试（WiFi连接比较慢）
      OLED_ShowString(0,6, "4.CWJAP...",8);
      count = 0;
      while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
      {
          if(++count >= 30)
          {
              UsartPrintf(USART2, "ESP8266 WiFi连接失败, 即将进入离线模式\r\n");
              OLED_ShowString(0,6, "WiFi FAIL",8);
              return;
          }
          MYDelay_ms(500);
      }

      // 5. 初始化成功
      OLED_Clear();
      OLED_ShowString(0,8, "5.ESP8266 Init OK",8);
      MYDelay_ms(500);
  }

//==========================================================
//  函数名称    ESP8266_ReconnectWiFi
//
//  函数功能    重新连接WiFi
//
//  入口参数    无
//
//  返回参数    无
//
//  说明
//==========================================================
void ESP8266_ReconnectWiFi(void)
{
	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		MYDelay_ms(500);
}

//==========================================================
void USART3_IRQHandler(void)
{

	if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET) // 接收中断
	{
		if(esp8266_cnt >= sizeof(esp8266_buf))	esp8266_cnt = 0; // 防止溢出被刷写
		esp8266_buf[esp8266_cnt++] = USART3->DR;

		USART_ClearFlag(USART3, USART_FLAG_RXNE);
	}
}
