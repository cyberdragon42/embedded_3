#include "net.h"
//-----------------------------------------------
uint8_t destinationIP[4];
uint16_t destinationPort; //port_dest;

USART_prop_ptr usartprop;
extern ARM_DRIVER_USART Driver_USART6;
ARM_DRIVER_USART* USARTdrv = &Driver_USART6;

char str[30];
char str1[100];
u8_t data[100];
struct tcp_pcb *client_pcb;
__IO uint32_t message_count=0;
//-----------------------------------------------
enum client_states
{
  ES_NOT_CONNECTED = 0,
  ES_CONNECTED,
  ES_RECEIVED,
  ES_CLOSING,
};
//-------------------------------------------------------
struct client_struct
{
  enum client_states state; /* connection status */
  struct tcp_pcb *pcb;          /* pointer on the current tcp_pcb */
  struct pbuf *p_tx;            /* pointer on pbuf to be transmitted */
};

struct client_struct *cs;
//-----------------------------------------------
static err_t clientTCP_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static void clientTCP_connection_close(struct tcp_pcb *tpcb, struct client_struct * es);
static err_t clientTCP_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void clientTCP_send(struct tcp_pcb *tpcb, struct client_struct * es);
static err_t clientTCP_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t clientTCP_poll(void *arg, struct tcp_pcb *tpcb);
//-----------------------------------------------
void clientTCP_connect(void)
{
	ip_addr_t DestIPaddr;
	client_pcb = tcp_new();
	if (client_pcb != NULL)
	{
	    IP4_ADDR( &DestIPaddr, destinationIP[0], destinationIP[1], idestinationIP[2], destinationIP[3]);
	    tcp_connect(client_pcb,&DestIPaddr,destinationPort,clientTCP_connected);
			GPIO_SetBits(GPIOD, GPIO_PIN_14);
	}
}
//-----------------------------------------------
static err_t clientTCP_connected(void *arg, struct tcp_pcb *tpcb, err_t err)
{
	struct client_struct *es = NULL;
	if (err == ERR_OK)
	{
		es = (struct client_struct *)mem_malloc(sizeof(struct client_struct));
	    if (es != NULL)
	    {
	    	es->state = ES_CONNECTED;
	    	es->pcb = tpcb;
	    	es->p_tx = pbuf_alloc(PBUF_TRANSPORT, strlen((char*)data) , PBUF_POOL);
	        if (es->p_tx)
	        {
	            /* copy data to pbuf */
	            pbuf_take(es->p_tx, (char*)data, strlen((char*)data));
	            /* pass newly allocated es structure as argument to tpcb */
	            tcp_arg(tpcb, es);
	            /* initialize LwIP tcp_recv callback function */
	            tcp_recv(tpcb, clientTCP_recv);
	            /* initialize LwIP tcp_sent callback function */
	            tcp_sent(tpcb, clientTCP_sent);
	            /* initialize LwIP tcp_poll callback function */
	            tcp_poll(tpcb, clientTCP_poll, 1);
	            /* send data */
	            clientTCP_send(tpcb,es);
	            return ERR_OK;
	        }
	    }
	    else
	    {
	      /* close connection */
	      clientTCP_connection_close(tpcb, es);
	      /* return memory allocation error */
	      return ERR_MEM;
	    }
	}
	else
	{
	/* close connection */
		clientTCP_connection_close(tpcb, es);
	}
	return err;
}
//----------------------------------------------------------
static void clientTCP_connection_close(struct tcp_pcb *tpcb, struct client_struct * es)
{
	/* remove callbacks */
	tcp_recv(tpcb, NULL);
	tcp_sent(tpcb, NULL);
	tcp_poll(tpcb, NULL,0);
	if (es != NULL)
	{
		mem_free(es);
	}
	/* close tcp connection */
	tcp_close(tpcb);
}
//----------------------------------------------------------
static err_t clientTCP_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
	struct client_struct *es;
	err_t ret_err;
	es = (struct client_struct *)arg;
	if (p == NULL)
	{
		es->state = ES_CLOSING;
	    if(es->p_tx == NULL)
	    {
	       clientTCP_connection_close(tpcb, es);
	    }
	    ret_err = ERR_OK;
	}
	else if(err != ERR_OK)
	{
		if (p != NULL)
		{
			pbuf_free(p);
		}
		ret_err = err;
	}
	else if(es->state == ES_CONNECTED)
	{
		message_count++;
		tcp_recved(tpcb, p->tot_len);
		GPIO_SetBits(GPIOD, GPIO_PIN_15);
		es->p_tx = p;
		strncpy(str1,es->p_tx->payload,es->p_tx->len);
		str1[es->p_tx->len] = '\0';

		USARTdrv->Control (ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control (ARM_USART_CONTROL_RX, 1);
		USARTdrv->Send((uint8_t*)str1,strlen(str1));
		ret_err = ERR_OK;
	}
	else if (es->state == ES_RECEIVED)
	{
		GPIO_SetBits(GPIOD, GPIO_PIN_13);
		ret_err = ERR_OK;
	}
	else
	{
		/* Acknowledge data reception */
		tcp_recved(tpcb, p->tot_len);
		/* free pbuf and do nothing */
		pbuf_free(p);
		ret_err = ERR_OK;
	}
	return ret_err;
}
//----------------------------------------------------------
static void clientTCP_send(struct tcp_pcb *tpcb, struct client_struct * es)
{
	struct pbuf *ptr;
	err_t wr_err = ERR_OK;
	while ((wr_err == ERR_OK) &&
			 (es->p_tx != NULL) &&
			 (es->p_tx->len <= tcp_sndbuf(tpcb)))
	{
		ptr = es->p_tx;
		/* enqueue data for transmission */
		wr_err = tcp_write(tpcb, ptr->payload, ptr->len, 1);
		if (wr_err == ERR_OK)
		{
			es->p_tx = ptr->next;
			if(es->p_tx != NULL)
			{
				pbuf_ref(es->p_tx);
			}
			pbuf_free(ptr);
		}
		else if(wr_err == ERR_MEM)
		{
			es->p_tx = ptr;
		}
		else
		{
			
		}
	}
}
//----------------------------------------------------------
static err_t clientTCP_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
	struct client_struct *es;
	LWIP_UNUSED_ARG(len);
	es = (struct client_struct *)arg;
	if(es->p_tx != NULL)
	{
		clientTCP_send(tpcb, es);
	}
	return ERR_OK;
}
//----------------------------------------------------------
static err_t clientTCP_poll(void *arg, struct tcp_pcb *tpcb)
{
	err_t ret_err;
	struct client_struct *es;
	es = (struct client_struct*)arg;
	//HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_14);
	if (es != NULL)
	{
	    if (es->p_tx != NULL)
	    {
	    }
	    else
	    {
	        if(es->state == ES_CLOSING)
	        {
	          clientTCP_connection_close(tpcb, es);
	        }
	    }
	    ret_err = ERR_OK;
	}
	else
	{
	    tcp_abort(tpcb);
	    ret_err = ERR_ABRT;
	}
	return ret_err;
}
//----------------------------------------------------------
void net_ini(void)
{
	  usartprop.usart_buf[0]=0;
	  usartprop.usart_cnt=0;
	  usartprop.is_tcp_connect=0;
	  usartprop.is_text=0;
}
//-----------------------------------------------
uint16_t port_extract(char* ip_str, uint8_t len)
{
  uint16_t port=0;
  int ch1=':';
  char *ss1;
  uint8_t offset = 0;
  ss1=strchr(ip_str,ch1);
  offset=ss1-ip_str+1;
  ip_str+=offset;
  port = atoi(ip_str);
  return port;
}
//--------------------------------------------------
void ip_extract(char* ip_str, uint8_t len, uint8_t* ipextp)
{
  uint8_t offset = 0;
  uint8_t i;
  char ss2[5] = {0};
  char *ss1;
  int ch1 = '.';
  int ch2 = ':';
	for(i=0;i<3;i++)
	{
		ss1 = strchr(ip_str,ch1);
		offset = ss1-ip_str+1;
		strncpy(ss2,ip_str,offset);
		ss2[offset]=0;
		ipextp[i] = atoi(ss2);
		ip_str+=offset;
		len-=offset;
	}
	ss1=strchr(ip_str,ch2);
	if (ss1!=NULL)
	{
		offset=ss1-ip_str+1;
		strncpy(ss2,ip_str,offset);
		ss2[offset]=0;
		ipextp[3] = atoi(ss2);
		return;
	}
	strncpy(ss2,ip_str,len);
	ss2[len]=0;
	ipextp[3] = atoi(ss2);
}
//-----------------------------------------------
void net_cmd(char* buf_str)
{
	uint8_t ip[4];
	uint16_t port;
	if(usartprop.is_tcp_connect==1)//try to connect TCP server status
	{
		ip_extract(buf_str,usartprop.usart_cnt-1,destinationIP);
		destinationPort=port_extract(buf_str,usartprop.usart_cnt-1);
		usartprop.usart_cnt=0;
	    usartprop.is_tcp_connect=0;
	    tcp_client_connect();
		sprintf(str1,"%d.%d.%d.%d:%u\r\n", destinationIP[0],destinationIP[1],destinationIP[2],destinationIP[3],destinationIP);
		 USARTdrv->Control (ARM_USART_CONTROL_TX, 1);
     USARTdrv->Control (ARM_USART_CONTROL_RX, 1);
		 USARTdrv->Send((uint8_t*)str1,strlen(str1));
		 GPIO_SetBits(GPIOD, GPIO_PIN_12|GPIO_PIN_13);
		
	}
	if(usartprop.is_tcp_connect==2)//try to cut connection to TCP server status
	{
		ip_extract(buf_str,usartprop.usart_cnt-1,ip);
		port=port_extract(buf_str,usartprop.usart_cnt-1);
		usartprop.usart_cnt=0;
	    usartprop.is_tcp_connect=0;
		//check if IP correct
		if(!memcmp(ip,destinationIP,4))
		{
			//check if port correct
			if(port==destinationPort)
			{
				//close tcp connection
				tcp_recv(client_pcb, NULL);
				tcp_sent(client_pcb, NULL);
				tcp_poll(client_pcb, NULL,0);
				tcp_close(client_pcb);
        GPIO_SetBits(GPIOD, GPIO_PIN_15);
			}
		}
	}
}
//-----------------------------------------------
void sendstring(char* buf_str)
{
	tcp_sent(client_pcb, tcp_client_sent);
	tcp_write(client_pcb, (void*)buf_str, strlen(buf_str), 1);
	tcp_output(client_pcb);
	usartprop.usart_cnt=0;
    usartprop.is_text=0;
}
//-----------------------------------------------
void string_parse(char* buf_str)
{
		 USARTdrv->Control (ARM_USART_CONTROL_TX, 1);
     USARTdrv->Control (ARM_USART_CONTROL_RX, 1);
		 USARTdrv->Send((uint8_t*)buf_str,strlen(buf_str));
	//	if command("t:")
	if (strncmp(buf_str,"t:", 2) == 0)
	{
		usartprop.usart_cnt-=1;
		usartprop.is_tcp_connect=1;
		net_cmd(buf_str+2);
		GPIO_SetBits(GPIOD, GPIO_PIN_12);
	}

	else if (strncmp(buf_str,"c:", 2) == 0)
	{
		usartprop.usart_cnt-=1;
		usartprop.is_tcp_connect=2;
		net_cmd(buf_str+2);
					GPIO_SetBits(GPIOD, GPIO_PIN_12);
	}
	else
	{
		 usartprop.is_text=1;
		 sendstring(buf_str);
		 GPIO_ResetBits(GPIOD, GPIO_PIN_12|GPIO_PIN_13);
	}
}
//-----------------------------------------------
void UART6_RxCpltCallback(void)
{
	uint8_t b;
	b = str[0];
	//if buf length>25
	if (usartprop.usart_cnt>25)
	{
		usartprop.usart_cnt=0;
		USARTdrv->Receive((uint8_t*)str, 1); 
		return;
	}
	usartprop.usart_buf[usartprop.usart_cnt] = b;
	if(b==0x0A)
	{
		usartprop.usart_buf[usartprop.usart_cnt+1]=0;
		string_parse((char*)usartprop.usart_buf);
		usartprop.usart_cnt=0;
		USARTdrv->Receive((uint8_t*)str, 1); 
		return;
	}
	 usartprop.usart_cnt++;
	 USARTdrv->Receive((uint8_t*)str, 1);   
}
//-----------------------------------------------
