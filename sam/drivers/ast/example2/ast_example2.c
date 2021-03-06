/**
 * \file
 *
 * \brief SAM Asynchronous Timer (AST) example 2 alarm wakeup.
 *
 * Copyright (C) 2012-2015 Atmel Corporation. All rights reserved.
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

#include <asf.h>
#include "ast_example2.h"

volatile bool flag = false;

/**
 * \brief Callback handler for AST alarm Interrupt.
 */
static void ast_alarm_callback(void)
{
	ast_disable_interrupt(AST, AST_INTERRUPT_ALARM);
	flag = true;
}

/**
 * \brief Configure timer.
 */
static void config_ast(void)
{
	struct ast_config ast_conf;

	/* Enable osc32 oscillator*/
	if (!osc_is_ready(OSC_ID_OSC32)) {
		osc_enable(OSC_ID_OSC32);
		osc_wait_ready(OSC_ID_OSC32);
	}

	/* Enable the AST */
	ast_enable(AST);

	ast_conf.mode = AST_COUNTER_MODE;
	ast_conf.osc_type = AST_OSC_32KHZ;
	ast_conf.psel = AST_PSEL_32KHZ_1HZ;
	ast_conf.counter = 0;

	/*
	 * Using counter mode and set it to 0.
	 * Initialize the AST.
	 */
	if (!ast_set_config(AST, &ast_conf)) {
		printf("Error initializing the AST\r\n");
		while (1) {
		}
	}

	/* First clear alarm status. */
	ast_clear_interrupt_flag(AST, AST_INTERRUPT_ALARM);

	/* Enable wakeup from alarm0. */
	ast_enable_wakeup(AST, AST_WAKEUP_ALARM);

	/* Set callback for alarm0. */
	ast_set_callback(AST, AST_INTERRUPT_ALARM, ast_alarm_callback,
		AST_ALARM_IRQn, 1);

	/* Disable first interrupt for alarm0. */
	ast_disable_interrupt(AST, AST_INTERRUPT_ALARM);

}

/**
 * \brief Configure the wake-up.
 */
static void config_wakeup(void)
{
	/* AST can wakeup the device. */
	bpm_enable_wakeup_source(BPM,(1 << BPM_BKUPWEN_AST));
}

/**
 *  \brief Configure serial console.
 */
static void configure_console(void)
{
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
#ifdef CONF_UART_CHAR_LENGTH
		.charlength = CONF_UART_CHAR_LENGTH,
#endif /* CONF_UART_CHAR_LENGTH */
		.paritytype = CONF_UART_PARITY,
#ifdef CONF_UART_STOP_BITS
		.stopbits = CONF_UART_STOP_BITS,
#endif /* CONF_UART_STOP_BITS */
	};

	/* Configure console. */
	stdio_serial_init(CONF_UART, &uart_serial_options);
}

/**
 * \brief Display the user menu on the terminal.
 */
static void display_menu(void)
{
	printf("Menu :\n\r"
			"  -- Select low power mode:\n\r"
			"  1: Sleep mode 0 \n\r"
			"  2: Sleep mode 1 \n\r"
			"  3: Sleep mode 2 \n\r"
			"  4: Sleep mode 3 \n\r"
			"  5: Wait mode \n\r"
			"  6: Retention mode \n\r"
			"  7: Backup mode \n\r"
			"\n\r");
}

/**
 * \brief main function : do init and loop.
 */
int main(void)
{
	uint32_t ast_alarm, ast_counter;
	uint8_t key;

	/* Initialize the SAM system. */
	sysclk_init();
	board_init();

	/* Initialize the console uart. */
	configure_console();

	/* Output example information. */
	printf("-- AST Example 2 in Counter Mode --\r\n");
	printf("-- %s\n\r", BOARD_NAME);
	printf("-- Compiled: %s %s --\n\r", __DATE__, __TIME__);

	printf("Config AST with 32kHz oscillator.\r\n");
	printf("Use alarm0 to wakeup from low power mode.\r\n");
	config_ast();

	/* AST can wakeup the device. */
	config_wakeup();

	while (1) {

		/* let the user select the low power mode. */
		key = 0;
		while ((key < 0x31) || (key > 0x37)) {
			/* Display menu */
			display_menu();
			scanf("%c", (char *)&key);
		}
		key = key - '0';

		/* ast_init_counter Set Alarm to current time+6 seconds. */
		ast_counter = ast_read_counter_value(AST);
		ast_alarm = ast_counter + 6;
		ast_write_alarm0_value(AST, ast_alarm);

		ast_enable_interrupt(AST, AST_INTERRUPT_ALARM);

		printf("\r\n--Enter low power mode.\r\n");
		/* 
		 * Wait for the printf operation to finish before setting the
		 * device in a power save mode.
		 */
		delay_ms(30);

		/* Go into selected low power mode. */
		bpm_sleep(BPM, key);
		while (flag == false);
		flag = true;

		/* After wake up, clear the Alarm0. */
		ast_clear_interrupt_flag(AST, AST_INTERRUPT_ALARM);

		printf("\r\n--Exit low power mode.\r\n");

		/* Output the counter value. */
		ast_counter = ast_read_counter_value(AST);
		printf("\n\r Counter value: %02u \n\r", ast_counter);
	}
}
