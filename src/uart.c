#define USE_DMA

#include <string.h>
#include <ch32v00x.h>
#ifdef USE_SPL
#include <ch32v00x_rcc.h>
#include <ch32v00x_gpio.h>
#include <ch32v00x_usart.h>
#ifdef USE_DMA
#include <ch32v00x_dma.h>
#endif
#endif
#include "utils.h"
#include "uart.h"

void uartInit(void) {
#ifdef USE_SPL
#ifdef USE_DMA
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    {
        DMA_InitTypeDef DMA_InitStructure = { 0 };

        DMA_DeInit(DMA1_Channel4);
        DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&USART1->DATAR;
        DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
        DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
        DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
        DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
        DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
        DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
        DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
        DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
        DMA_Init(DMA1_Channel4, &DMA_InitStructure);
    }
#endif

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE);

    {
        GPIO_InitTypeDef GPIO_InitStructure;

        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
        GPIO_Init(GPIOD, &GPIO_InitStructure);
    }

    {
        USART_InitTypeDef USART_InitStructure;

        USART_InitStructure.USART_BaudRate = UART_SPEED;
        USART_InitStructure.USART_WordLength = USART_WordLength_8b;
        USART_InitStructure.USART_StopBits = USART_StopBits_1;
        USART_InitStructure.USART_Parity = USART_Parity_No;
        USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
        USART_InitStructure.USART_Mode = USART_Mode_Tx;
        USART_Init(USART1, &USART_InitStructure);

    }

#ifdef USE_DMA
    USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
#endif

    USART_Cmd(USART1, ENABLE);
#else
#ifdef USE_DMA
    RCC->AHBPCENR |= RCC_DMA1EN;

    DMA1_Channel4->CFGR = DMA_CFGR1_DIR | DMA_CFGR1_MINC | DMA_CFGR1_PL_0 | DMA_CFGR1_PL_1;
    DMA1_Channel4->CNTR = 0;
    DMA1_Channel4->PADDR = (uint32_t)&USART1->DATAR;
    DMA1_Channel4->MADDR = 0;
#endif

    RCC->APB2PCENR |= (RCC_USART1EN | RCC_IOPDEN);

    /* USART1 TX-->D5 */
    GPIOD->CFGLR &= ~((uint32_t)0x0F << (4 * 5));
    GPIOD->CFGLR |= ((uint32_t)0x0B << (4 * 5));

    {
        uint32_t tmp, brr;

        tmp = (25 * FCPU) / (4 * UART_SPEED);
        brr = (tmp / 100) << 4;
        tmp -= 100 * (brr >> 4);
        brr |= (((tmp * 16) + 50) / 100) & 0x0F;

        USART1->BRR = brr;
    }

    USART1->CTLR2 = 0;
#ifdef USE_DMA
    USART1->CTLR3 = USART_CTLR3_DMAT;
#else
    USART1->CTLR3 = 0;
#endif
    USART1->CTLR1 = USART_CTLR1_UE | USART_CTLR1_TE;
#endif
}

void uartWrite(char c) {
#ifdef USE_SPL
#ifdef USE_DMA
    uartFlush();
    DMA_Cmd(DMA1_Channel4, DISABLE);
    DMA1_Channel4->MADDR = (uint32_t)&c;
    DMA1_Channel4->CNTR = 1;
    DMA_Cmd(DMA1_Channel4, ENABLE);
#else
    while (! USART_GetFlagStatus(USART1, USART_FLAG_TXE)) {}
    USART_SendData(USART1, c);
#endif
#else // #ifdef USE_SPL
#ifdef USE_DMA
    uartFlush();
    DMA1_Channel4->CFGR &= ~DMA_CFGR1_EN;
    DMA1_Channel4->MADDR = (uint32_t)&c;
    DMA1_Channel4->CNTR = 1;
    DMA1_Channel4->CFGR |= DMA_CFGR1_EN;
#else
    while (! (USART1->STATR & USART_STATR_TXE)) {}
    USART1->DATAR = c;
#endif
#endif
}

void uartPrint(const char *str) {
#ifdef USE_DMA
    uint16_t len = strlen(str);

    if (len) {
        uartFlush();
#ifdef USE_SPL
        DMA_Cmd(DMA1_Channel4, DISABLE);
#else
        DMA1_Channel4->CFGR &= ~DMA_CFGR1_EN;
#endif
        DMA1_Channel4->MADDR = (uint32_t)str;
        DMA1_Channel4->CNTR = len;
#ifdef USE_SPL
        DMA_Cmd(DMA1_Channel4, ENABLE);
#else
        DMA1_Channel4->CFGR |= DMA_CFGR1_EN;
#endif
    }
#else
    while (*str) {
        uartWrite(*str++);
    }
#endif
}

void uartFlush(void) {
#ifdef USE_SPL
#ifdef USE_DMA
    if (DMA1_Channel4->CFGR & DMA_CFG4_EN) {
        while (! DMA_GetFlagStatus(DMA1_FLAG_TC4)) {}
    }
#else
    while (! USART_GetFlagStatus(USART1, USART_FLAG_TC)) {}
#endif
#else // #ifdef USE_SPL
#ifdef USE_DMA
    if (DMA1_Channel4->CFGR & DMA_CFG4_EN) {
        while (! (DMA1->INTFR & DMA_TCIF4)) {}
    }
#else
    while (! (USART1->STATR & USART_STATR_TC)) {}
#endif
#endif
}
