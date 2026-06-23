#include "stm32f10x.h"

//网络协议层
#include "onenet.h"
#include "mqttkit.h"

//网络设备
#include "esp8266.h"

//FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"


#include "Delay.h"
#include "LED.h"
#include "oled.h"
#include "Motor.h"
#include "Serial.h"
#include "HCSR04.h"
#include "Servo.h"
#include "DHT11.h"
#include "TrackSensor.h"
#include "PWM.h"
#include "Hummer.h"
#include "ADC.h"
#include "usart.h"
#include "stm32f10x_iwdg.h"

#include "common.h"

#define ESP8266_ONENET_INFO		"AT+CIPSTART=\"TCP\",\"mqtts.heclouds.com\",1883\r\n"

#define PROID			"4447G2Igb6"

#define ACCESS_KEY		"Mm1XbE9nallUOVNTdWZ2SXV0Z1VGN25lZWY0Qm54MGY="

#define DEVICE_NAME		"car"

TaskHandle_t ADC_Handler;//ADC采集
TaskHandle_t OLED_Handler;//OLED
TaskHandle_t LED_Hummer_Handler;//LED
TaskHandle_t HCSR04_Handler;//超声波测距
TaskHandle_t HCSR04_Mode_Handler;//自动避障
TaskHandle_t DHT11_Handler;//温湿度采集
TaskHandle_t BlueTooth_Handler;//接收蓝牙
TaskHandle_t Tracking_Handler;//自动循迹
TaskHandle_t Ctrl_Handler;//控制
TaskHandle_t HeartBeat_Handler;//心跳任务
TaskHandle_t Car_Receive_Handler;//onenet平台下发命令任务
TimerHandle_t Timer_Handler;//软件定时器
SemaphoreHandle_t OLED_Mutex;//OLED互斥锁
SemaphoreHandle_t esp8266_Mutex;//esp8266互斥锁
SemaphoreHandle_t HCSR04_Mutex;//超声波测距互斥锁
QueueHandle_t BlueToothQueue;
QueueHandle_t MQTT_Queue;
SemaphoreHandle_t BluetoothSem;

//结构体定义
Mode_Status Mode;
LED1_Status LED1;
LED2_Status LED2;
MQTT_QueueItem_t item;
Hummer_Status Hummer;

uint8_t RxData;
int16_t  Distance;
int16_t count = 0;
float ADC_raw;
float battery_voltage;
volatile uint32_t dht_timer = 0;
volatile uint32_t led_timer = 0;
volatile uint32_t hcsr04_timer = 0;
volatile uint32_t heartbeat_led_timer=0;
volatile uint32_t heartbeat_check_timer=0; 
uint8_t temp = 0, humi = 0;  // 初始化
volatile uint8_t TrackingMode = 0;    // 循迹模式标志位
volatile uint8_t HCSR04Mode=0;        //超声波模式标志位
volatile uint8_t near_flag=0;         //超声波测距喇叭标志位
volatile uint8_t hummer_flag=0;       //蓝牙控制喇叭标志位
volatile uint8_t battery_voltage_flag=0;//低电压喇叭标准位
volatile uint8_t tcp_alive = 1;              //ESP8266 TCP连接状态: 1=正常, 0=断开
uint8_t turn_state = 0;      // 0:无转向, 1:左转, 2:右转

void Hardware_Init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

	Delay_Init();
	Usart2_Init(9600);
	MYDelay_ms(500);
	Usart3_Init(115200);
	LED_Init();
	Motor_Init();
	Serial_Init();
	HCSR04_Init();
	Servo_Init();
	DHT11_Init();
	TrackSensor_Init();
	Hummer_Init();
	MYADC_Init();
	OLED_Init();
	
	Servo_SetAngle(90);

    LED_Left_ON();
	LED_Right_ON();
	LED1.LED1_Status = 0;
	LED2.LED2_Status = 1;
	UsartPrintf(USART2, " 小车正在准备.. \r\n");
}

//FreeRTOS软件定时器
void Timer_Callback(TimerHandle_t xTimer)
{
	led_timer++;
	dht_timer++;
	hcsr04_timer++;
	heartbeat_led_timer++;
	heartbeat_check_timer++;
}

//ADC采集
void ADC_Task(void *arg)
{
	while(1)
	{
		uint32_t sum = 0;
		for(int i = 0; i < 10; i++)
		{
			sum += ADC_GetValue();
		}
		ADC_raw = sum / 10;
        UsartPrintf(USART2, "ADC_raw=%d\r\n", sum / 10);
		float ADC_v = ADC_raw * 3.3f / 4095.0f;
        battery_voltage = ADC_v * 4.0f; // 分压比3倍
		if(battery_voltage<=11)
		{
			battery_voltage_flag=1;
			Hummer_ON();
		}
		else
		{
			battery_voltage_flag=0;
			Hummer_OFF();
		}
		
		IWDG_ReloadCounter();    //喂狗！证明主逻辑还活着
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

//OLED显示数据
void OLED_Task(void *ar)
{
	while(1)
	{
		// UsartPrintf(USART2, "Distance=%d, temp=%d, humi=%d\r\n", Distance, temp, humi);
		xSemaphoreTake(OLED_Mutex,portMAX_DELAY);
		OLED_ShowNum(43,0,Distance,3,16);
		OLED_ShowString(70,0,"cm",16);
		OLED_ShowNum(45,4,temp,2,16);
		OLED_ShowCHinese(60,4,9);
		OLED_ShowNum(45,6,humi,2,16);
		OLED_ShowString(65,6,"%",16);
		
		//显示电池电压
		OLED_ShowNum(43, 2, (int)battery_voltage, 2, 16);                    // 整数: 43, 51
		OLED_ShowString(59, 2, ".", 16);                                      // 小数点: 59
		OLED_ShowNum(63, 2, (int)(battery_voltage*10) % 10, 1, 16);         // 十分位: 63
		OLED_ShowNum(71, 2, (int)(battery_voltage*100) % 10, 1, 16);        // 百分位: 71
		OLED_ShowString(79, 2, "V", 16);
	
		xSemaphoreGive(OLED_Mutex);

		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

//超声波测距任务
void HCSR04_Task(void *arg)
{
	while(1)
	{
		if(hcsr04_timer >= 10)
		{
			hcsr04_timer = 0;

			xSemaphoreTake(HCSR04_Mutex,portMAX_DELAY);
			Distance = HCSR04_Distance();
			xSemaphoreGive(HCSR04_Mutex);
	
			if(Distance>0 && Distance<=15)
			{
				near_flag = 1;
			}
			else 
			{
			    near_flag = 0;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}


//温湿度采集任务
void DHT11_Task(void *arg)
{
	while(1)
	{
		//读取温湿度模块
		if(dht_timer >= 200)
		{
			dht_timer = 0;
			DHT11_ReadData(&humi,&temp);
			
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}


//自动避障任务
void HCSR04_Mode_Task(void *arg)
{
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		UsartPrintf(USART2, "正在自动避障... \r\n");
	    //自动避障模式（只要HCSR04Mode为1就一直运行）
		while(HCSR04Mode == 1)
		{
			LED_Front_ON();
			LED1.LED1_Status = 1;
			Servo_SetAngle(90);
			// 1. 实时测距
			xSemaphoreTake(HCSR04_Mutex,portMAX_DELAY);
			Distance = HCSR04_Distance(); 
			xSemaphoreGive(HCSR04_Mutex);
			// 2. 发现障碍物
		    if(Distance > 0 && Distance <= 25) 
		    {
				SmallCar_Stop();
				LED_Right_ON(); 
				LED_Left_ON();
				Hummer_ON();          
				vTaskDelay(pdMS_TO_TICKS(300));
				if(HCSR04Mode==0)
				{
					continue;
				}

				// 安全后退（如果离得太近，给旋转留空间）
				if(Distance <= 10)
				{
					SmallCar_Retreat();
					vTaskDelay(pdMS_TO_TICKS(300));
					if(HCSR04Mode==0)
					{
						continue;
					}
					SmallCar_Stop();
					turn_state=4;
				}
				
				//  雷达扫描寻找方向
				float left_dist, right_dist;
				
				// 往左看
				Servo_SetAngle(150);  
				vTaskDelay(pdMS_TO_TICKS(500)); 
				if(HCSR04Mode==0)
				{
					continue;
				}     
				left_dist = HCSR04_Distance(); // 记录左边距离
				
				// 往右看
				Servo_SetAngle(30);   
				vTaskDelay(pdMS_TO_TICKS(500));  
				if(HCSR04Mode==0)
				{
					continue;
				}   
				right_dist = HCSR04_Distance(); // 记录右边距离
				
				Servo_SetAngle(90);   
				Hummer_OFF();         
				
				if(left_dist > right_dist)
				{
					// 左边更宽敞，原地左转一小段
					SmallCar_AntiClockWise(); 
					turn_state = 1; // 开启左转向灯
					vTaskDelay(pdMS_TO_TICKS(600)); 
					if(HCSR04Mode==0)
					{
						continue;
					}
				}
				else
				{
					// 右边更宽，原地右转一小段
					SmallCar_ClockWise();
					turn_state = 2; // 开启右转向灯
					vTaskDelay(pdMS_TO_TICKS(600));
					if(HCSR04Mode==0)
					{
						continue;
					}
				}
				
				SmallCar_Stop();
				turn_state = 0; // 关闭转向灯
			}
		    else
			{
				//前方空旷:前进，舵机面向正前方
				SmallCar_Forword();
				Servo_SetAngle(90);
				
				//根据距离调节速度
				if(Distance > 50 || Distance == 0) PWM_Compare2(60), PWM_Compare1(60); // 远距离快
				else PWM_Compare2(40), PWM_Compare1(40); // 中距离慢
			}
	    }
    }
}

//循迹任务
void Tracking_Task(void *arg)
{
	while(1)
	{
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		UsartPrintf(USART2, "正在循迹... \r\n");
		// 循迹模式执行逻辑（只要 TrackingMode为1就一直运行）
		while(TrackingMode == 1)
		{
			//LED_Front_ON();
			if(Track_L1()==0 && Track_L2()==0 && Track_L3()==0 && Track_L4()==0)
			{
				SmallCar_Forword();
				turn_state = 0;
			}
			if(Track_L1()==1 && Track_L2()==1 && Track_L3()==1 && Track_L4()==1)
			{
				SmallCar_Stop();
				turn_state = 0;
			}
			else if(Track_L1()==0 && Track_L2()==0 && (Track_L3()==1 || Track_L4()==1))
			{
				SmallCar_TurnLeft(); // 左边传感器触线，应向左修正
				turn_state = 1;
			}
			else if((Track_L1()==1 || Track_L2()==1) && Track_L3()==0 && Track_L4()==0)
			{
				SmallCar_TurnRight(); // 右边传感器触线，应向右修正
				turn_state = 2;
			}
			else if(Track_L2()==1 && Track_L3()==1)
			{
				SmallCar_Forword();
				turn_state = 0;
			}
			else if(Track_L4()==1)
			{
				SmallCar_TurnRight();
				turn_state = 2;
			}
			else if(Track_L1()==1)
			{
				SmallCar_TurnLeft();
				turn_state = 1;
			}
		}
	}
}

//蓝牙控制
void Ctrl_Task(void *arg)
{
	uint16_t count1=0;
	uint16_t count2=0;
	while(1)
	{
		if(xQueueReceive(BlueToothQueue,&RxData,portMAX_DELAY)==pdPASS)
		{
			switch(RxData)
			{
				case 0X00: 
				    SmallCar_Stop();
				    turn_state = 3;
				    TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车停止 \r\n");
					Mode.car_status=0;
					LED2.LED2_Status=1;
					break;

				case 0X01: 
				    Servo_SetAngle(90);
					SmallCar_Forword();
					turn_state = 8;
					if(Distance > 0 && Distance <= 15) 
					{
						SmallCar_Stop();
						turn_state = 3;
					}
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车前进 \r\n");
					Mode.car_status=1;
					LED2.LED2_Status=0;
					break;

				case 0X02: 
				    SmallCar_TurnRight();
					turn_state = 2;
					if(Distance > 0 && Distance <= 15) 
					{
						SmallCar_Stop();
						turn_state = 3;
					}
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车右转 \r\n");
					Mode.car_status=4;
					LED2.LED2_Status=3;
					break;

				case 0X03: 
				    SmallCar_TurnLeft();
					turn_state = 1;
					xSemaphoreTake(HCSR04_Mutex, portMAX_DELAY);
					int16_t cur_dist = HCSR04_Distance();
					xSemaphoreGive(HCSR04_Mutex);
					if(cur_dist > 0 && cur_dist <= 15)
					{
						SmallCar_Stop();
						turn_state = 3;
						break;
					}
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车左转 \r\n");
					Mode.car_status=3;
					LED2.LED2_Status=4;
					break;

				case 0X04: 
				    SmallCar_ClockWise();
					turn_state = 2;
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车顺时针旋转 \r\n");
					Mode.car_status=5;
					LED2.LED2_Status=3;
					break;

				case 0X05: 
				    SmallCar_AntiClockWise();
					turn_state = 1;
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车逆时针旋转 \r\n");
					Mode.car_status=6;
					LED2.LED2_Status=4;
					break;

				case 0X06: 
				    SmallCar_Retreat();
					turn_state = 4;
					TrackingMode = 0;
					HCSR04Mode = 0;
					UsartPrintf(USART2, "小车后退 \r\n");
					Mode.car_status=2;
					LED2.LED2_Status=2;
					break;

				case 0X07:
				    TrackingMode = 1;
					xTaskNotifyGive(Tracking_Handler);
					HCSR04Mode=0;
					UsartPrintf(USART2, "小车循迹功能准备就绪... \r\n");
					Mode.car_status=7;
					break;
					
				case 0X08:
				    HCSR04Mode=1;
					xTaskNotifyGive(HCSR04_Mode_Handler);
					TrackingMode=0;
					UsartPrintf(USART2, "小车自动避障功能准备就绪... \r\n");
					Mode.car_status=8;
					break;

				case 0X09:
				    if(count2 ==0 )
					{
						count2++;
						hummer_flag=1;
						UsartPrintf(USART2, "小车喇叭启动 \r\n");
					}
					else
					{
						count2=0;
						hummer_flag=0;
						UsartPrintf(USART2, "小车喇叭关闭 \r\n");
					}
					turn_state = 3;
					TrackingMode = 0;
					HCSR04Mode = 0;
					break;

				case 0X10:
				if(count1==0)
				{
					count1++;turn_state = 7;
					LED1.LED1_Status=1;
					UsartPrintf(USART2, "小车远光灯启动 \r\n");
				}
				else
				{
					count1=0;
					turn_state = 6;
					LED1.LED1_Status=0;
					UsartPrintf(USART2, "小车远光灯关闭 \r\n");
				}
				TrackingMode = 0;
				HCSR04Mode = 0;
				break;
			}
		}
	}
}

//接收蓝牙任务
void BlueTooth_Task(void *arg)
{
	while(1)
	{
		//蓝牙遥控模块
		xSemaphoreTake(BluetoothSem, portMAX_DELAY);
		RxData=SmallCar_GetRxData();
		xQueueSend(BlueToothQueue,&RxData,0);
		
	}
}


void LED_Hummer_Task(void *arg)
{
	while(1)
	{
		
		// 转向灯主逻辑：每 500ms 翻转一次
		if(led_timer >= 50)
		{
			led_timer = 0;
			if(turn_state == 1) { LED_Left_Turn(); LED_Right_OFF(); }
			else if(turn_state == 2)  { LED_Right_Turn(); LED_Left_OFF(); }
			else if(turn_state == 3)  { LED_Left_ON(); LED_Right_ON(); }
			else if(turn_state == 4)  { LED_Left_Turn();  LED_Right_Turn();}
			else if(turn_state == 5)  { LED_Front1_Turn();LED_Front2_Turn();}
			else if(turn_state == 6)  { LED_Front_OFF();}
			else if(turn_state == 7)  { LED_Front_ON();}
			else if(turn_state == 8)  { LED_Left_OFF(); LED_Right_OFF(); }

		    if(near_flag || hummer_flag || battery_voltage_flag)
		    {
				Hummer.Hummer_Status = 1;
			    Hummer_ON();
		    }
		    else
		    {
				Hummer.Hummer_Status = 0;
		        Hummer_OFF();
		    }
		}
		vTaskDelay(pdMS_TO_TICKS(10));
	}
}

// MQTT_Prep_Task — 高优先级，拼 JSON + 打包 MQTT
void MQTT_Prep_Task(void *arg)
{
    while(1)
    {
		//UsartPrintf(USART2, "产数据 %d\r\n", xTaskGetTickCount());
        MQTT_QueueItem_t item;
        MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};
        char buf[256];
        // 1. 拼 JSON
        memset(buf, 0, sizeof(buf));
        OneNet_FillBuf(buf);
        // 2. 打包成 MQTT 协议
        if (MQTT_PacketSaveData(PROID, DEVICE_NAME, strlen(buf), (int8 *)buf, &mqttPacket) == 0)
        {
            // 3. 拷贝进队列
            memset(&item, 0, sizeof(item));
            item.len = mqttPacket._len;
            if (item.len > sizeof(item.data))
                item.len = sizeof(item.data);
            memcpy(item.data, mqttPacket._data, item.len);
            // 4. 塞进队列，满了就丢最旧的
            if (xQueueSend(MQTT_Queue, &item, 0) != pdPASS)
            {
				UsartPrintf(USART2, "处理数据5\r\n");
                MQTT_QueueItem_t dummy;
                xQueueReceive(MQTT_Queue, &dummy, 0);   // 丢掉最旧的
                xQueueSend(MQTT_Queue, &item, 0);        // 塞新的
            }
            MQTT_DeleteBuffer(&mqttPacket);             // 释放动态内存
			}
        vTaskDelay(pdMS_TO_TICKS(2000));  // 2 秒准备一次
    }
}

// MQTT_Send_Task — 低优先级，从队列取数据并通过 ESP8266 发送
void MQTT_Send_Task(void *arg)
{
    MQTT_QueueItem_t item;
    while(1)
    {
        if (xQueueReceive(MQTT_Queue, &item, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(esp8266_Mutex, portMAX_DELAY) == pdTRUE)
            {
                if(ESP8266_SendData(item.data, item.len) == 0)
                    tcp_alive = 1;
                else
                    tcp_alive = 0;

                xSemaphoreGive(esp8266_Mutex);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
      }
  } 

void HeartBeat_Task(void *arg)
{
    uint8_t connect_fail_cnt = 0;

    while(1)
    {
        // PC13每500ms翻转一次
        if(heartbeat_led_timer >= 50)
        {
            heartbeat_led_timer = 0;
            LED13_Turn();
        }

        // 每10秒检测一次ESP8266连接
        if(heartbeat_check_timer >= 1000)
        {
            heartbeat_check_timer = 0;

            /* 检测时不拿互斥量 — 发一个AT+CIPSTATUS耗时很短
                 与MQTT_Send_Task冲突的概率极低，偶尔冲突也无所谓 */
            if(tcp_alive == 0)
            {
                connect_fail_cnt++;
                UsartPrintf(USART2, "心跳: TCP断开(%d/2)\r\n", connect_fail_cnt);
            }
            else
            {
                connect_fail_cnt = 0;
            }

            /*连续2次失败 → 抢互斥量重连 */
            if(connect_fail_cnt >= 2)
            {
                connect_fail_cnt = 0;

                if(xSemaphoreTake(esp8266_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE)
                {
                    UsartPrintf(USART2, "心跳: 开始重连...\r\n");

                    ESP8266_ReconnectWiFi();

                    while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))
                        vTaskDelay(pdMS_TO_TICKS(500));

                    while(OneNet_DevLink())
                        vTaskDelay(pdMS_TO_TICKS(500));

                    OneNET_Subscribe();

                    tcp_alive = 1;  // 重连成功，恢复标志
                    UsartPrintf(USART2, "心跳: 重连成功!\r\n");

                    xSemaphoreGive(esp8266_Mutex);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Car_Receive_Task(void *arg)
{
    unsigned char *dataPtr = NULL;

    while(1)
    {
        if (xSemaphoreTake(esp8266_Mutex, pdMS_TO_TICKS(100)) == pdTRUE)  // 只等100ms
        {
            dataPtr = ESP8266_GetIPD(50);     // 最多等 250ms
            if(dataPtr != NULL)
            {
                OneNet_RevPro(dataPtr);
            }
            xSemaphoreGive(esp8266_Mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
int main(void)
{
	Hardware_Init();
	OLED_Clear();
	ESP8266_Init();//连接wifi
	OLED_Clear();

	OLED_ShowString(0,0, "Connect MQTTs Server...",8);
	while(ESP8266_SendCmd(ESP8266_ONENET_INFO, "CONNECT"))//连接 MQTT 服务器
	Delay_ms(500);
	
	OLED_ShowString(0,4, "Connect success",8);
	Delay_ms(500);
	
	OLED_Clear();
	OLED_ShowString(0,0, "Device login...",16);
	Delay_ms(500);
	
	OLED_Clear();
	while(OneNet_DevLink())//设备登录（鉴权）
	Delay_ms(500);

	OneNET_Subscribe();//订阅主题

	UsartPrintf(USART2, "小车已就绪 \r\n");
	OLED_ShowCHinese(0,0,0);
	OLED_ShowCHinese(18,0,1);
	OLED_ShowCHinese(36,0,2);
	
	OLED_ShowCHinese(0,2,3);
	OLED_ShowCHinese(18,2,4);
	OLED_ShowCHinese(36,2,2);
	
	OLED_ShowCHinese(0,4,5);
	OLED_ShowCHinese(18,4,6);
	OLED_ShowCHinese(36,4,2);
	
	OLED_ShowCHinese(0,6,7);
	OLED_ShowCHinese(18,6,8);
	OLED_ShowCHinese(36,6,2);
	Timer_Handler = xTimerCreate(
		"Timer",
		pdMS_TO_TICKS(10),//定时时间为10MS
		pdTRUE,//重复定时
		0,//定时器ID
		Timer_Callback//定时器回调函数
	);
	xTimerStart(Timer_Handler,0);//开启软件定时器

	OLED_Mutex = xSemaphoreCreateMutex();
	HCSR04_Mutex = xSemaphoreCreateMutex();
	esp8266_Mutex = xSemaphoreCreateMutex();

	BlueToothQueue = xQueueCreate(3,sizeof(uint8_t));
	MQTT_Queue = xQueueCreate(3,sizeof(MQTT_QueueItem_t));
	BluetoothSem = xSemaphoreCreateBinary();

	xTaskCreate(Ctrl_Task,"Ctrl_Task",128,NULL,4,&Ctrl_Handler);
    xTaskCreate(ADC_Task,"ADC_Task",200,NULL,2,&ADC_Handler);
    xTaskCreate(OLED_Task,"OLED_Task",128,NULL,1,&OLED_Handler);
    xTaskCreate(LED_Hummer_Task,"LED_Hummer_Task",128,NULL,1,&LED_Hummer_Handler);
    xTaskCreate(HCSR04_Task,"HCSR04_Task",200,NULL,2,&HCSR04_Handler);
    xTaskCreate(DHT11_Task,"DHT11_Task",128,NULL,2,&DHT11_Handler);
    xTaskCreate(BlueTooth_Task,"BlueTooth_Task",128,NULL,4,&BlueTooth_Handler);
    xTaskCreate(HCSR04_Mode_Task,"HCSR_Mode_Task",200,NULL,3,&HCSR04_Mode_Handler);
    xTaskCreate(Tracking_Task,"Tracking_Task",200,NULL,3,&Tracking_Handler);
	xTaskCreate(MQTT_Prep_Task,  "MQTT_Prep_Task",  500, NULL,5, NULL);
	xTaskCreate(Car_Receive_Task,"Car_Receive_Task",500,NULL,2,&Car_Receive_Handler);
	xTaskCreate(HeartBeat_Task,"HeartBeat_Task",128,NULL,2,&HeartBeat_Handler);
	xTaskCreate(MQTT_Send_Task, "MQTT_Send_Task", 256, NULL,2, NULL);
	UsartPrintf(USART2, "剩余堆: %d bytes\r\n", xPortGetFreeHeapSize());

	//初始化独立看门狗
    IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);      // 允许修改寄存器
    IWDG_SetPrescaler(IWDG_Prescaler_64);              // 分频系数 64
    // LSI 约 40kHz，分频后 ≈ 625Hz，一个 tick ≈ 1.6ms
    // 设重装载值 937 → 约 1.5 秒超时
    IWDG_SetReload(937);
    IWDG_ReloadCounter();                               // 初次喂狗
    IWDG_Enable();                                      // 启动看门狗

	LED13_Turn();
	MYDelay_ms(300);
	LED13_Turn();
	MYDelay_ms(300);
	LED13_Turn();
	MYDelay_ms(300);
	vTaskStartScheduler();

	while(1)
	{
		
    }
}
