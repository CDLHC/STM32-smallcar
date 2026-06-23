#ifndef _ONENET_H_
#define _ONENET_H_


extern uint8_t curtain_status;


_Bool OneNET_RegisterDevice(void);

_Bool OneNet_DevLink(void);

void OneNet_SendData(void);

void OneNET_Subscribe(void);

void OneNet_RevPro(unsigned char *cmd);

#endif
