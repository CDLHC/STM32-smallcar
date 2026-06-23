// 单片机头文件
#include "stm32f10x.h"
// 网络设备
#include "esp8266.h"

// 云平台
#include "onenet.h"
#include "mqttkit.h"

// 编解码
#include "base64.h"
#include "hmac_sha1.h"

// 板级外设
#include "usart.h"
#include "delay.h"
#include "LED.h"
#include "Motor.h"
#include "Servo.h"
#include "LED.h"

// C库
#include <string.h>
#include <stdio.h>

#include "cJSON.h"
#include "common.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"

#define PROID			"4447G2Igb6"

#define ACCESS_KEY		"Mm1XbE9nallUOVNTdWZ2SXV0Z1VGN25lZWY0Qm54MGY="

#define DEVICE_NAME		"car"

char devid[16];

char key[48];


extern unsigned char esp8266_buf[512];

static unsigned char OTA_UrlEncode(char *sign)
{

	char sign_t[40];
	unsigned char i = 0, j = 0;
	unsigned char sign_len = strlen(sign);

	if(sign == (void *)0 || sign_len < 28)
		return 1;

	for(; i < sign_len; i++)
	{
		sign_t[i] = sign[i];
		sign[i] = 0;
	}
	sign_t[i] = 0;

	for(i = 0, j = 0; i < sign_len; i++)
	{
		switch(sign_t[i])
		{
			case '+':
				strcat(sign + j, "%2B");j += 3;
			break;

			case ' ':
				strcat(sign + j, "%20");j += 3;
			break;

			case '/':
				strcat(sign + j, "%2F");j += 3;
			break;

			case '?':
				strcat(sign + j, "%3F");j += 3;
			break;

			case '%':
				strcat(sign + j, "%25");j += 3;
			break;

			case '#':
				strcat(sign + j, "%23");j += 3;
			break;

			case '&':
				strcat(sign + j, "%26");j += 3;
			break;

			case '=':
				strcat(sign + j, "%3D");j += 3;
			break;

			default:
				sign[j] = sign_t[i];j++;
			break;
		}
	}

	sign[j] = 0;

	return 0;

}


#define METHOD		"sha1"
static unsigned char OneNET_Authorization(char *ver, char *res, unsigned int et, char *access_key, char *dev_name,
												char *authorization_buf, unsigned short authorization_buf_len, _Bool flag)
{

	size_t olen = 0;

	char sign_buf[64];						// 存放HMAC-SHA1后的Base64签名，再做URL编码
	char hmac_sha1_buf[64];					// HMAC-SHA1结果
	char access_key_base64[64];				// 存放access_key的Base64解码结果
	char string_for_signature[72];			// 存放string_for_signature拼接参数，用作HMAC-SHA1的key

	//---------------------------------------------------- 参数检查 ----------------------------------------------------
		if(ver == (void *)0 || res == (void *)0 || et < 1564562581 || access_key == (void *)0
			|| authorization_buf == (void *)0 || authorization_buf_len < 120)
			return 1;

	//---------------------------------------------------- 对access_key进行Base64解码 ----------------------------------------------------
		memset(access_key_base64, 0, sizeof(access_key_base64));
		BASE64_Decode((unsigned char *)access_key_base64, sizeof(access_key_base64), &olen, (unsigned char *)access_key, strlen(access_key));
		//UsartPrintf(USART_DEBUG, "access_key_base64: %s\r\n", access_key_base64);

	//---------------------------------------------------- 拼接string_for_signature -----------------------------------------------------
		memset(string_for_signature, 0, sizeof(string_for_signature));
		if(flag)
			snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s\n%s", et, METHOD, res, ver);
		else
			snprintf(string_for_signature, sizeof(string_for_signature), "%d\n%s\nproducts/%s/devices/%s\n%s", et, METHOD, res, dev_name, ver);
		//UsartPrintf(USART_DEBUG, "string_for_signature: %s\r\n", string_for_signature);

	//---------------------------------------------------- 计算HMAC-SHA1 --------------------------------------------------------
		memset(hmac_sha1_buf, 0, sizeof(hmac_sha1_buf));

		hmac_sha1((unsigned char *)access_key_base64, strlen(access_key_base64),
					(unsigned char *)string_for_signature, strlen(string_for_signature),
					(unsigned char *)hmac_sha1_buf);

		//UsartPrintf(USART_DEBUG, "hmac_sha1_buf: %s\r\n", hmac_sha1_buf);

	//---------------------------------------------------- 对HMAC-SHA1结果进行Base64编码 ------------------------------------------------------
		olen = 0;
		memset(sign_buf, 0, sizeof(sign_buf));
		BASE64_Encode((unsigned char *)sign_buf, sizeof(sign_buf), &olen, (unsigned char *)hmac_sha1_buf, strlen(hmac_sha1_buf));

	//---------------------------------------------------- 对Base64编码后的签名做URL编码 ----------------------------------------------------
		OTA_UrlEncode(sign_buf);
		//UsartPrintf(USART_DEBUG, "sign_buf: %s\r\n", sign_buf);

	//---------------------------------------------------- 生成Token --------------------------------------------------------------------
		if(flag)
			snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s&et=%d&method=%s&sign=%s", ver, res, et, METHOD, sign_buf);
		else
			snprintf(authorization_buf, authorization_buf_len, "version=%s&res=products%%2F%s%%2Fdevices%%2F%s&et=%d&method=%s&sign=%s", ver, res, dev_name, et, METHOD, sign_buf);
		//UsartPrintf(USART_DEBUG, "Token: %s\r\n", authorization_buf);

		return 0;

}


_Bool OneNET_RegisterDevice(void)
{

	_Bool result = 1;
	unsigned short send_len = 11 + strlen(DEVICE_NAME);
	char *send_ptr = NULL, *data_ptr = NULL;

	char authorization_buf[144];													// 认证key

	send_ptr = malloc(send_len + 240);
	if(send_ptr == NULL)
		return result;

	while(ESP8266_SendCmd("AT+CIPSTART=\"TCP\",\"183.230.40.33\",80\r\n", "CONNECT"))
		DelayXms(500);

	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, NULL,
								authorization_buf, sizeof(authorization_buf), 1);

	snprintf(send_ptr, 240 + send_len, "POST /mqtt/v1/devices/reg HTTP/1.1\r\n"
					"Authorization:%s\r\n"
					"Host:ota.heclouds.com\r\n"
					"Content-Type:application/json\r\n"
					"Content-Length:%d\r\n\r\n"
					"{\"name\":\"%s\"}",

					authorization_buf, 11 + strlen(DEVICE_NAME), DEVICE_NAME);

	ESP8266_SendData((unsigned char *)send_ptr, strlen(send_ptr));

	/*
	{
	  "request_id" : "f55a5a37-36e4-43a6-905c-cc8f958437b0",
	  "code" : "onenet_common_success",
	  "code_no" : "000000",
	  "message" : null,
	  "data" : {
		"device_id" : "589804481",
		"name" : "mcu_id_43057127",

	"pid" : 282932,
		"key" : "indu/peTFlsgQGL060Gp7GhJOn9DnuRecadrybv9/XY="
	  }
	}
	*/

	data_ptr = (char *)ESP8266_GetIPD(250);							// 获取平台返回数据

	if(data_ptr)
	{
		data_ptr = strstr(data_ptr, "device_id");
	}

	if(data_ptr)
	{
		char name[16];
		int pid = 0;

		if(sscanf(data_ptr, "device_id\" : \"%[^\"]\",\r\n\"name\" : \"%[^\"]\",\r\n\r\n\"pid\" : %d,\r\n\"key\" : \"%[^\"]\"", devid, name, &pid, key) == 4)
		{
			UsartPrintf(USART_DEBUG, "create device: %s, %s, %d, %s\r\n", devid, name, pid, key);
			result = 0;
		}
	}

	free(send_ptr);
	ESP8266_SendCmd("AT+CIPCLOSE\r\n", "OK");

	return result;

}


_Bool OneNet_DevLink(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};					// 定义MQTT包

	unsigned char *dataPtr;

	char authorization_buf[160];

	_Bool status = 1;

	OneNET_Authorization("2018-10-31", PROID, 1956499200, ACCESS_KEY, DEVICE_NAME,
									authorization_buf, sizeof(authorization_buf), 0);

	UsartPrintf(USART_DEBUG, "OneNET_DevLink\r\n"
							"NAME: %s,	PROID: %s,	KEY:%s\r\n"
	                   , DEVICE_NAME, PROID, authorization_buf);

	if(MQTT_PacketConnect(PROID, authorization_buf, DEVICE_NAME, 256, 1, MQTT_QOS_LEVEL0, NULL, NULL, 0, &mqttPacket) == 0)
	{
		ESP8266_SendData(mqttPacket._data, mqttPacket._len);			// 发送MQTT连接包

		dataPtr = ESP8266_GetIPD(250);									// 获取平台返回数据
		if(dataPtr != NULL)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_CONNACK)
			{
				switch(MQTT_UnPacketConnectAck(dataPtr))
				{
					case 0:UsartPrintf(USART_DEBUG, "Tips:	连接成功\r\n");status = 0;break;

					case 1:UsartPrintf(USART_DEBUG, "WARN:	连接被拒绝，协议版本错误\r\n");break;
					case 2:UsartPrintf(USART_DEBUG, "WARN:	连接被拒绝，无效的clientid\r\n");break;
					case 3:UsartPrintf(USART_DEBUG, "WARN:	连接被拒绝，服务器不可用\r\n");break;
					case 4:UsartPrintf(USART_DEBUG, "WARN:	连接被拒绝，错误的用户名或密码\r\n");break;
					case 5:UsartPrintf(USART_DEBUG, "WARN:	连接被拒绝，未经授权(可能是token异常)\r\n");break;

					default:UsartPrintf(USART_DEBUG, "ERR:	连接被拒绝，未知错误\r\n");break;
				}
			}
		}

		MQTT_DeleteBuffer(&mqttPacket);								// 释放内存
	}
	else
		UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketConnect Failed\r\n");

	return status;

}
extern uint8_t humi,temp;
extern int16_t Distance;
extern float battery_voltage;


unsigned char OneNet_FillBuf(char *buf)
{
	char text[128];

	uint16_t int_part = (uint16_t)battery_voltage;
    uint16_t dec_part = (uint16_t)((battery_voltage - int_part) * 100);

	memset(text, 0, sizeof(text));

	strcpy(buf, "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{");

	memset(text, 0, sizeof(text));
	sprintf(text, "\"temp\":{\"value\":%d},",temp);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"humi\":{\"value\":%d},",humi);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"voltage\":{\"value\":%d.%02d},", int_part, dec_part);
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	switch(Mode.car_status)
	{
		case 0: sprintf(text, "\"car_status\":{\"value\":\"停止\"},"); break;
		case 1: sprintf(text, "\"car_status\":{\"value\":\"前进\"},"); break;
		case 2: sprintf(text, "\"car_status\":{\"value\":\"后退\"},"); break;
		case 3: sprintf(text, "\"car_status\":{\"value\":\"左转\"},"); break;
		case 4: sprintf(text, "\"car_status\":{\"value\":\"右转\"},"); break;
		case 5: sprintf(text, "\"car_status\":{\"value\":\"顺时针旋转\"},"); break;
		case 6: sprintf(text, "\"car_status\":{\"value\":\"逆时针旋转\"},"); break;
		case 7: sprintf(text, "\"car_status\":{\"value\":\"自动避障\"},"); break;
		case 8: sprintf(text, "\"car_status\":{\"value\":\"自动循迹\"},"); break;
		default: sprintf(text, "\"car_status\":{\"value\":\"指令错误\"},"); break;
	}
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	switch(LED1.LED1_Status)
	{
		case 0: sprintf(text, "\"LED1\":{\"value\":\"关闭\"},"); break;
		case 1: sprintf(text, "\"LED1\":{\"value\":\"开启\"},"); break;
		case 2: sprintf(text, "\"LED1\":{\"value\":\"闪烁\"},"); break;
		default: sprintf(text, "\"LED1\":{\"value\":\"指令错误\"},"); break;
	}
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	switch(LED2.LED2_Status)
	{
		case 0: sprintf(text, "\"LED2\":{\"value\":\"关闭\"},"); break;
		case 1: sprintf(text, "\"LED2\":{\"value\":\"开启\"},"); break;
		case 2: sprintf(text, "\"LED2\":{\"value\":\"闪烁\"},"); break;
		case 3: sprintf(text, "\"LED2\":{\"value\":\"右灯开启\"},"); break;
		case 4: sprintf(text, "\"LED2\":{\"value\":\"左灯开启\"},"); break;
		default: sprintf(text, "\"LED2\":{\"value\":\"指令错误\"},"); break;
	}
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	switch(Hummer.Hummer_Status)
	{
		case 0: sprintf(text, "\"hummer\":{\"value\":\"关闭\"},"); break;
		case 1: sprintf(text, "\"hummer\":{\"value\":\"开启\"},"); break;
		default: sprintf(text, "\"hummer\":{\"value\":\"指令错误\"},"); break;
	}
	strcat(buf, text);

	memset(text, 0, sizeof(text));
	sprintf(text, "\"distance\":{\"value\":%d}",Distance);
	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"LED\":{\"value\":%s},",LED_info.LED_Status?"true":"false");
// 	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"LED1\":{\"value\":%s},",LED_info.LED1_Status?"true":"false");
// 	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"LED2\":{\"value\":%s},",LED_info.LED2_Status?"true":"false");
// 	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
// 	sprintf(text, "\"LED4\":{\"value\":%s},",LED_info.LED4_Status?"true":"false");
// 	strcat(buf, text);

// 	memset(text, 0, sizeof(text));
//  sprintf(text, "\"curtain\":{\"value\":%s},", curtain_status ? "true" : "false");
//  strcat(buf, text);

// 	memset(text, 0, sizeof(text));
//  sprintf(text, "\"fan-speed\":{\"value\":%d}", fan_speed);
//  strcat(buf, text);

	strcat(buf, "}}");
	//UsartPrintf(USART2, "upload: %s\r\n", buf);

	return strlen(buf);

}


void OneNet_SendData(void)
{

	MQTT_PACKET_STRUCTURE mqttPacket = {NULL, 0, 0, 0};												// 定义MQTT包

	char buf[256];

	short body_len = 0, i = 0;

	UsartPrintf(USART_DEBUG, "Tips:	OneNet_SendData-MQTT\r\n");

	memset(buf, 0, sizeof(buf));

	body_len = OneNet_FillBuf(buf);																	// 填充JSON数据到缓冲区

	if(body_len)
	{
		UsartPrintf(USART_DEBUG, "Sending JSON: %s\r\n", buf);
		if(MQTT_PacketSaveData(PROID, DEVICE_NAME, body_len, (int8 *)buf, &mqttPacket) == 0)		// 打包
		{
			ESP8266_SendData(mqttPacket._data, mqttPacket._len);									// 通过ESP8266发送
			UsartPrintf(USART_DEBUG, "Send %d Bytes\r\n", mqttPacket._len);

			MQTT_DeleteBuffer(&mqttPacket);															// 释放内存
		}
		else
			UsartPrintf(USART_DEBUG, "WARN:	MQTT_PacketSaveData Failed\r\n");
	}

}


void OneNET_Publish(const char *topic, const char *msg)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						// 定义MQTT包

	UsartPrintf(USART_DEBUG, "Publish Topic: %s, Msg: %s\r\n", topic, msg);

	if(MQTT_PacketPublish(MQTT_PUBLISH_ID, topic, msg, strlen(msg), MQTT_QOS_LEVEL0, 0, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);					// 通过ESP8266发送数据

		MQTT_DeleteBuffer(&mqtt_packet);										// 释放内存
	}

}

void OneNET_Subscribe(void)
{

	MQTT_PACKET_STRUCTURE mqtt_packet = {NULL, 0, 0, 0};						// 定义MQTT包

	char topic_buf[56];
	const char *topic = topic_buf;

	unsigned char *dataPtr = NULL;

	snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/thing/property/set", PROID, DEVICE_NAME);

	UsartPrintf(USART_DEBUG, "Subscribe Topic: %s\r\n", topic_buf);

	if(MQTT_PacketSubscribe(MQTT_SUBSCRIBE_ID, MQTT_QOS_LEVEL0, &topic, 1, &mqtt_packet) == 0)
	{
		ESP8266_SendData(mqtt_packet._data, mqtt_packet._len);    // 通过ESP8266发送订阅请求

		DelayXms(200);
		dataPtr = ESP8266_GetIPD(250);
		if(dataPtr)
		{
			if(MQTT_UnPacketRecv(dataPtr) == MQTT_PKT_SUBACK)
			{
				if(MQTT_UnPacketSubscribe(dataPtr) == 0)
					UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
				else
					UsartPrintf(USART_DEBUG, "ERR:	MQTT Subscribe Failed\r\n");
			}
			else
			{
				UsartPrintf(USART_DEBUG, "ERR:	Unexpected response type\r\n");
			}
		}
		else
		{
			UsartPrintf(USART_DEBUG, "ERR:	No SUBACK received\r\n");
		}

		MQTT_DeleteBuffer(&mqtt_packet);										// 释放内存
	}

}

extern int turn_state;
extern uint8_t hummer_flag;
extern uint8_t near_flag;
extern uint8_t battery_voltage_flag;
extern uint8_t RxData;
extern QueueHandle_t BlueToothQueue;
void OneNet_RevPro(unsigned char *cmd)
{

	char *req_payload = NULL;
	char *cmdid_topic = NULL;

	unsigned short topic_len = 0;
	unsigned short req_len = 0;

	unsigned char qos = 0;
	static unsigned short pkt_id = 0;

	unsigned char type = 0;

	short result = 0;

	char *dataPtr = NULL;
	char numBuf[10];
	int num = 0;

	cJSON *raw_json, *params_json, *LED1_json,**LED2_json,**hummer_json;

	type = MQTT_UnPacketRecv(cmd);
	switch(type)
	{
		case MQTT_PKT_PUBLISH:						// 收到Publish消息

			result = MQTT_UnPacketPublish(cmd, &cmdid_topic, &topic_len, &req_payload, &req_len, &qos, &pkt_id);
			if(result == 0)
			{
				char *data_ptr = NULL;

				UsartPrintf(USART_DEBUG, "topic: %s, topic_len: %d, payload: %s, payload_len: %d\r\n",
																		cmdid_topic, topic_len, req_payload, req_len);

				raw_json = cJSON_Parse(req_payload);
				// OneNet物模型格式：最外层包含params对象，params里包含各属性
				if(raw_json == NULL)
				{
					UsartPrintf(USART_DEBUG, "ERR: JSON Parse Failed\r\n");
					if(cmdid_topic != NULL)
					{
						MQTT_FreeBuffer(cmdid_topic);
						cmdid_topic = NULL;
					}
					if(req_payload != NULL)
					{
						MQTT_FreeBuffer(req_payload);
						req_payload = NULL;
					}
					break;
				}
				// 解析OneNet物模型JSON：获取params对象，再从params中提取各属性值
				params_json = cJSON_GetObjectItem(raw_json,"params");

				// 检查params对象是否存在
				if(params_json == NULL)
				{
					UsartPrintf(USART_DEBUG, "ERR: params not found\r\n");
					cJSON_Delete(raw_json);
					break;
				}

			//UsartPrintf(USART2, "收到的JSON: %s\r\n", req_payload);
			if (params_json != NULL)
			{
				cJSON *LED1_json = cJSON_GetObjectItem(params_json, "LED1");
				if (LED1_json != NULL && LED1_json->valuestring != NULL)
				{
					if(strcmp(LED1_json->valuestring, "开启") == 0)
					{
						LED_Front_ON();
						LED1.LED1_Status=1;
						turn_state = 7;
					}
					else if(strcmp(LED1_json->valuestring, "关闭") == 0)
					{
						LED_Front_OFF();
						LED1.LED1_Status=0;
						turn_state = 6;
					}
				}
			}

			if (params_json != NULL)
			{
				cJSON *LED2_json = cJSON_GetObjectItem(params_json, "LED2");
				if (LED2_json != NULL && LED2_json->valuestring != NULL)
				{
					if(strcmp(LED2_json->valuestring, "开启") == 0)
					{
						LED_Left_ON(); LED_Right_ON();
						LED2.LED2_Status=1;
						turn_state = 3;
					}
					else if(strcmp(LED2_json->valuestring, "关闭") == 0)
					{
						LED_Left_OFF(); LED_Right_OFF();
						LED2.LED2_Status=0;
						turn_state = 8;
					}
					else if(strcmp(LED2_json->valuestring, "右灯打开") == 0)
					{
						LED_Right_Turn(); LED_Left_OFF();
						LED2.LED2_Status=3;
						turn_state = 2;
					}
					else if(strcmp(LED2_json->valuestring, "左灯打开") == 0)
					{
						LED_Left_Turn(); LED_Right_OFF();
						LED2.LED2_Status=4;
						turn_state = 1;
					}
					else if(strcmp(LED2_json->valuestring, "闪烁") == 0)
					{
						LED_Right_Turn();LED_Left_Turn();
						LED2.LED2_Status=2;
						turn_state = 4;
					}
				}
			}

			if (params_json != NULL)
			{
				cJSON *hummer_json = cJSON_GetObjectItem(params_json, "hummer");
				if (hummer_json != NULL && hummer_json->valuestring != NULL)
				{
					if(strcmp(hummer_json->valuestring, "开启") == 0)
					{
						hummer_flag=1;
						Hummer.Hummer_Status=1;
					}
					else if(strcmp(hummer_json->valuestring, "关闭") == 0)
					{
						hummer_flag=0;near_flag=0;battery_voltage_flag=0;
						Hummer.Hummer_Status=0;
					}
				}
			}

			if(params_json != NULL)
			{
				cJSON *car_status_json = cJSON_GetObjectItem(params_json, "car_status");
				if (car_status_json != NULL && car_status_json->valuestring != NULL)
				{
					if(strcmp(car_status_json->valuestring,"前进") == 0)
					{
						RxData = 0X01;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"停止") == 0)
					{
						RxData = 0X00;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"向右转") == 0)
					{
						RxData = 0X02;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"向左转") == 0)
					{
						RxData = 0X03;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"顺时针旋转") == 0)
					{
						RxData = 0X04;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"逆时针旋转") == 0)
					{
						RxData = 0X05;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"后退") == 0)
					{
						RxData = 0X06;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"循迹") == 0)
					{
						RxData = 0X07;
						xQueueSend(BlueToothQueue,&RxData,0);
					}
					else if(strcmp(car_status_json->valuestring,"避障") == 0)
					{
						RxData = 0X08;
						xQueueSend(BlueToothQueue,&RxData,0);
					}

				}

			}
				cJSON_Delete(raw_json);// 释放旧的 JSON 对象，避免内存泄漏

				{
					char resp_buf[64];	// 存放响应消息
					char resp_topic[80];// 存放响应主题
					cJSON *id_json;     // 获取消息ID

					// 重新解析JSON获取id
					raw_json = cJSON_Parse(req_payload);
					if(raw_json != NULL)
					{
						id_json = cJSON_GetObjectItem(raw_json, "id");
						if(id_json != NULL)
						{
							snprintf(resp_buf, sizeof(resp_buf), "{\"id\":\"%s\",\"code\":200,\"msg\":\"success\"}", id_json->valuestring);
							snprintf(resp_topic, sizeof(resp_topic), "$sys/%s/%s/thing/property/set_reply", PROID, DEVICE_NAME);
							OneNET_Publish(resp_topic, resp_buf);
							UsartPrintf(USART_DEBUG, "Send property set reply\r\n");
						}
						cJSON_Delete(raw_json);
					}
				}


				if(cmdid_topic != NULL)
				{
					MQTT_FreeBuffer(cmdid_topic);
					cmdid_topic = NULL;
				}
				if(req_payload != NULL)
				{
					MQTT_FreeBuffer(req_payload);
					req_payload = NULL;
				}
			}
			break;

//					data_ptr = strstr(cmdid_topic, "request/");									// 查找cmdid
//					{
//						char topic_buf[80], cmdid[40];
//
//						data_ptr = strchr(data_ptr, '/');
//						data_ptr++;
//
//						memcpy(cmdid, data_ptr, 36);											// 获取cmdid
//						cmdid[36] = 0;
//
//						snprintf(topic_buf, sizeof(topic_buf), "$sys/%s/%s/cmd/response/%s",
//											PROID, DEVICE_NAME, cmdid);
//						OneNET_Publish(topic_buf, "ojbk");										// 回复指令
//					}

		case MQTT_PKT_PUBACK:										// 收到Publish消息的Ack回复

			if(MQTT_UnPacketPublishAck(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Publish Send OK\r\n");

		break;

		case MQTT_PKT_SUBACK:									// 收到Subscribe的Ack回复


			if(MQTT_UnPacketSubscribe(cmd) == 0)
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe OK\r\n");
			else
				UsartPrintf(USART_DEBUG, "Tips:	MQTT Subscribe Err\r\n");

		break;

		default:
			result = -1;
		break;
	}

	ESP8266_Clear();			// 清空缓冲区

	if(result == -1)
		return;

//	dataPtr = strchr(req_payload, ':');					// 查找':'

//	if(dataPtr != NULL && result != -1)					// 查找结果判断
//	{
//		dataPtr++;
//
//		while(*dataPtr >= '0' && *dataPtr <= '9')		// 将数字字符逐个取出，组成整数
//		{
//		{
//			numBuf[num++] = *dataPtr++;
//		}
//		numBuf[num] = 0;
//
//		num = atoi((const char *)numBuf);				// 转换成整数
//	}


	if(type == MQTT_PKT_CMD || type == MQTT_PKT_PUBLISH)
	{
		MQTT_FreeBuffer(cmdid_topic);
		MQTT_FreeBuffer(req_payload);
	}

}
