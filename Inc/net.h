#ifndef NET_H_
#define NET_H_
//-----------------------------------------------
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "lwip.h"
#include "lwip/tcp.h"
//-----------------------------------------------
void net_ini(void);
void UART6_RxCpltCallback(void);
//-----------------------------------------------
typedef struct USART_prop{
  uint8_t usart_buf[26];
  uint8_t usart_cnt;
  uint8_t is_tcp_connect;
  uint8_t is_text;
} USART_prop_ptr;
//-----------------------------------------------
#endif /* NET_H_ */
