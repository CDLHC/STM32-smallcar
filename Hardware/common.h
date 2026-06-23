// 在 main.h 里
#ifndef __COMMON_H
#define __COMMON_H

#include "stm32f10x.h"

typedef struct
{
    uint16_t car_status;  
} Mode_Status;

typedef struct
{
    uint16_t LED1_Status;   
} LED1_Status;
typedef struct
{
    uint16_t LED2_Status;   // 0=停止,1=前进,2=后退,3=左转,4=右转
} LED2_Status;
typedef struct
{
    uint16_t Hummer_Status;   // 0=停止,1=前进,2=后退,3=左转,4=右转
} Hummer_Status;

 typedef struct
{
      unsigned char data[300];
      unsigned short len;
} MQTT_QueueItem_t;


extern LED1_Status LED1;
extern LED2_Status LED2;
extern Mode_Status Mode; 
extern MQTT_QueueItem_t item;
extern Hummer_Status Hummer;
#endif