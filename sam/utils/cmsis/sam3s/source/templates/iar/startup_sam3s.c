/**
 * \file
 *
 * \brief Startup file for SAM3S.
 *
 * Copyright (c) 2011-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#include "exceptions.h"
#include "sam3s.h"
#include "system_sam3s.h"

typedef void (*intfunc) (void);
typedef union { intfunc __fun; void * __ptr; } intvec_elem;

void __iar_program_start(void);
int __low_level_init(void);

/* Exception Table */
#pragma language=extended
#pragma segment="CSTACK"

/* The name "__vector_table" has special meaning for C-SPY: */
/* it is where the SP start value is found, and the NVIC vector */
/* table register (VTOR) is initialized to this address if != 0 */

#pragma section = ".intvec"
#pragma location = ".intvec"
const intvec_elem __vector_table[] = {
	{.__ptr = __sfe("CSTACK")},
	{Reset_Handler},

	{NMI_Handler},
	{HardFault_Handler},
	{MemManage_Handler},
	{BusFault_Handler},
	{UsageFault_Handler},
	{0}, {0}, {0}, {0},   /*  Reserved */
	{SVC_Handler},
	{DebugMon_Handler},
	{0},                   /*  Reserved */
	{PendSV_Handler},
	{SysTick_Handler},

	/* Configurable interrupts */
	{SUPC_Handler},     /* 0  Supply Controller */
	{RSTC_Handler},     /* 1  Reset Controller */
	{RTC_Handler},      /* 2  Real Time Clock */
	{RTT_Handler},      /* 3  Real Time Timer */
	{WDT_Handler},      /* 4  Watchdog Timer */
	{PMC_Handler},      /* 5  PMC */
	{EFC_Handler},      /* 6  EFC */
	{Dummy_Handler},    /* 7  Reserved */
	{UART0_Handler},    /* 8  UART0 */
	{UART1_Handler},    /* 9  UART1 */
#ifdef ID_SMC
	{SMC_Handler},      /* 10 SMC */
#else
	Dummy_Handler,
#endif
	{PIOA_Handler},     /* 11 Parallel IO Controller A */
	{PIOB_Handler},     /* 12 Parallel IO Controller B */
#ifdef ID_PIOC
	{PIOC_Handler},     /* 13 Parallel IO Controller C */
#else
	Dummy_Handler,
#endif
	{USART0_Handler},   /* 14 USART 0 */
#ifdef ID_USART1
	{USART1_Handler},   /* 15 USART 1 */
#else
	Dummy_Handler,
#endif
	{Dummy_Handler},    /* 16 Reserved */
	{Dummy_Handler},    /* 17 Reserved */
#ifdef ID_HSMCI
	{HSMCI_Handler},    /* 18 HSMCI */
#else
	Dummy_Handler,
#endif
	{TWI0_Handler},     /* 19 TWI 0 */
	{TWI1_Handler},     /* 20 TWI 1 */
	{SPI_Handler},      /* 21 SPI */
	{SSC_Handler},      /* 22 SSC */
	{TC0_Handler},      /* 23 Timer Counter 0 */
	{TC1_Handler},      /* 24 Timer Counter 1 */
	{TC2_Handler},      /* 25 Timer Counter 2 */
#ifdef ID_TC3
	{TC3_Handler},      /* 26 Timer Counter 3 */
#else
	Dummy_Handler,
#endif
#ifdef ID_TC4
	{TC4_Handler},      /* 27 Timer Counter 4 */
#else
	Dummy_Handler,
#endif
#ifdef ID_TC5
	{TC5_Handler},      /* 28 Timer Counter 5 */
#else
	Dummy_Handler,
#endif
	{ADC_Handler},      /* 29 ADC controller */
#ifdef ID_DACC
	{DACC_Handler},     /* 30 DACC controller */
#else
	Dummy_Handler,
#endif
	{PWM_Handler},      /* 31 PWM */
	{CRCCU_Handler},    /* 32 CRC Calculation Unit */
	{ACC_Handler},      /* 33 Analog Comparator */
	{UDP_Handler},      /* 34 USB Device Port */
	{Dummy_Handler}     /* 35 not used */
};

/* EWARM 6.30 integrates CMSIS 2.10 (__CM3_CMSIS_VERSION 0x0210),
 * in which SCB_VTOR_TBLBASE_Msk not defined.
 */
#if (__VER__ >= 6030000)
/* TEMPORARY PATCH FOR SCB */
#define SCB_VTOR_TBLBASE_Pos               29                            /*!< SCB VTOR: TBLBASE Position */
#define SCB_VTOR_TBLBASE_Msk               (1UL << SCB_VTOR_TBLBASE_Pos) /*!< SCB VTOR: TBLBASE Mask */
#endif

/**------------------------------------------------------------------------------
 * This is the code that gets called on processor reset. To initialize the
 * device.
 *------------------------------------------------------------------------------*/
int __low_level_init(void)
{
	uint32_t *pSrc = __section_begin(".intvec");

	SCB->VTOR = ((uint32_t) pSrc & SCB_VTOR_TBLOFF_Msk);

	if (((uint32_t) pSrc >= IRAM_ADDR) && ((uint32_t) pSrc < (uint32_t) IRAM_ADDR + (uint32_t) IRAM_SIZE)) {
		SCB->VTOR |= (uint32_t) (1 << SCB_VTOR_TBLBASE_Pos);
	}

	return 1; /* if return 0, the data sections will not be initialized */
}

/**------------------------------------------------------------------------------
 * This is the code that gets called on processor reset. To initialize the
 * device.
 *------------------------------------------------------------------------------*/
void Reset_Handler(void)
{
	__iar_program_start();
}
