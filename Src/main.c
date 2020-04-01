#include "main.h"
#include "stm32f4xx_hal.h"
#include "lwip.h"
#include "net.h"
#include "Driver_USART.h"

extern struct netif gnetif;
extern char str[30];

void SystemClock_Config(void);
static void USART6_UART_Init(void);

int main(void)
{
  ETH_GPIO_Config();
	ETH_MACDMA_Config();
  net_ini();

  while (1)
  {
    ethernetif_input(&gnetif);
    sys_check_timeouts();
  }
}


void ETH_GPIO_Config(void) {
	/* Enable SYSCFG clock */
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

	/* Configure MCO1 (PA8) */
	/* This pin must be initialized as MCO, but not needed to be used */
	/* It looks like a bug in STM32F4 */
	/* Init alternate function for PA8 = MCO */
	TM_GPIO_Init(GPIOA, GPIO_PIN_8, TM_GPIO_Mode_AF, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High);
	
#ifdef ETHERNET_MCO_CLOCK
	/* Set PA8 output HSE value */
	RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
#endif

	/* MII Media interface selection */
	SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII);
	/* Check if user has defined it's own pins */
	if (!TM_ETHERNET_InitPinsCallback()) {
		/* MII */
		/* GPIOA                     REF_CLK      MDIO         RX_DV */
		TM_GPIO_InitAlternate(GPIOA, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOB                     PPS_PUT      TDX3 */
		TM_GPIO_InitAlternate(GPIOB, GPIO_PIN_5 | GPIO_PIN_8, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOC                     MDC          TDX2         TX_CLK       RXD0         RXD1 */
		TM_GPIO_InitAlternate(GPIOC, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOG                     TX_EN         TXD0          TXD1 */
		TM_GPIO_InitAlternate(GPIOG, GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOH                     CRS          COL          RDX2         RXD3 */
		TM_GPIO_InitAlternate(GPIOH, GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
		
		/* GPIOI                     RX_ER */
		TM_GPIO_InitAlternate(GPIOI, GPIO_PIN_10, TM_GPIO_OType_PP, TM_GPIO_PuPd_NOPULL, TM_GPIO_Speed_High, GPIO_AF_ETH);
	}
}


//-----------------------------------------------
void ETH_MACDMA_Config(void) {
	/* Enable ETHERNET clock  */
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACRXEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_ETHMACTXEN;

	/* Reset ETHERNET on AHB Bus */
	ETH_DeInit();

	/* Software reset */
	ETH_SoftwareReset();

	/* Wait for software reset */
	while (ETH_GetSoftwareResetStatus() == SET);

	/* ETHERNET Configuration --------------------------------------------------*/
	/* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
	ETH_StructInit(&ETH_InitStructure);

	/* Fill ETH_InitStructure parametrs */
	/*------------------------   MAC   -----------------------------------*/
	ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
	ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
	ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
	ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
	ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
	ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
	ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
	ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
	ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
	ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

	/*------------------------   DMA   -----------------------------------*/  
	/* When we use the Checksum offload feature, we need to enable the Store and Forward mode: 
	the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
	if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
	ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable; 
	ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
	ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;

	ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
	ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
	ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
	ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
	ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
	ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
	ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

	/* Configure Ethernet */
	EthStatus = ETH_Init(&ETH_InitStructure, ETHERNET_PHY_ADDRESS);
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart==&huart6)
  {
    UART6_RxCpltCallback();
  }
}

void _Error_Handler(char * file, int line)
{
  while(1) 
  {
  }

}

