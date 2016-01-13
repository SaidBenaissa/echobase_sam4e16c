/*
 * print_task.c
 *
 *  Created on: Dec 21, 2015
 *      Author: jcobb
 */

#include <stdint.h>
#include <string.h>


/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "freertos_usart_serial.h"
#include "sysclk.h"
#include "comm.h"
#include "tcpip.h"
#include "wan.h"
#include "wan_task.h"
#include "print_task.h"
#include <cph.h>

QueueHandle_t xPrintQueue;

static volatile bool start_task = false;
static volatile bool wait_ack = false;

static void print_handler_task(void *pvParameters);
static void task_handler(void);
static sys_result socket_handler_cb(uint8_t *data, uint32_t len);
static sys_result socket_empty_cb(uint8_t *data, uint32_t len);
static sys_result parse_result(char * buffer, char * token, char ** ptr_out);

static uint8_t printer_send_buffer[128] = {0};


void create_printer_task(uint16_t stack_depth_words, unsigned portBASE_TYPE task_priority)
{

	// queue for handling print messages
	xPrintQueue = xQueueCreate(1, PRINTCMD_MSG_SIZE);


//	vSemaphoreCreateBinary(app_start_signal);

	xTaskCreate(	print_handler_task,			/* The task that implements the command console. */
					(const int8_t *const) "PRT",	/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
					stack_depth_words,					/* The size of the stack allocated to the task. */
					NULL,			/* The parameter is used to pass the already configured USART port into the task. */
					task_priority,						/* The priority allocated to the task. */
					NULL);

}


static void print_handler_task(void *pvParameters)
{
	task_handler();
}

// this function does nothing but keep the compiler happy
static sys_result socket_empty_cb(uint8_t *data, uint32_t len)
{
	return SYS_OK;
}

static sys_result socket_handler_cb(uint8_t *data, uint32_t len)
{
	sys_result result;
	printf("app_data_handler: %s\r\n", data);

	if(wait_ack) {
		char * ptr = NULL;
		result = parse_result(data, "ACK", &ptr);

		if(result == SYS_OK) {
			printf("ack found!\r\n");
		}
	}

	return result;
}


void task_handler(void)
{
	BaseType_t result;

//	uint8_t * ip_endpoint = "appserver02.cphandheld.com";

	uint8_t * ip_endpoint = "96.27.198.215";

	socket_connection_t sck_connection;

	while(true) {
		if(comm_network_state.context == 1) {
			start_task = true;
			break;
		}
		vTaskDelay(100);
	}


	while(true) {

		if(start_task) {

			start_task = false;

			printf("start print task.\r\n");

			// create a new socket connection
			socket_newconnection(&sck_connection, ip_endpoint, DEFAULT_TCIP_CONNECTTIMEOUT);

			printf("sck0: %s:%d\r\n", sck_connection.socket->endpoint, sck_connection.socket->socket_conf.port);

			// establish a connection
			printf("cph_tcp_connect\r\n");
			tcp_result result = cph_tcp_connect(&sck_connection);

			if(result == SYS_TCP_OK) {
				printf("successfully connected.\r\n");

				// send a message once per second
				while(true) {


					result = cph_tcp_send(&sck_connection, "abcdefghijklmnopqrstuvwxyz0123456789\r", socket_handler_cb);

					printf("cph_tcp_send result: %d\r\n", result);

					uint8_t data[1024] = {0};


					while(true) {
						memset(data, '\0', 1024);
						// call cph_tcp_receive passing in the socket_emtpy_cb
						// we will be handling the data received synchronously
						result = cph_tcp_receive(&sck_connection, data, socket_empty_cb);

						if(result == SYS_TCP_OK) {
							printf("%s\r\n", data);
						} else {
							printf("cph_tcp_receive: error(%d)", result);
						}

						printf("waiting for print command\r\n");
						vTaskDelay(1000);
					}

				}

			}

		}
	}
}


static sys_result parse_result(char * buffer, char * token, char ** ptr_out)
{
	sys_result result;

	char * ptr = NULL;

	if((ptr = strstr(buffer, token))) {
		if(ptr_out != NULL) {
			*ptr_out = ptr;
		}
		//printf("SYS_AT_OK\r\n");
		result = SYS_OK;
	} else {
		result = SYS_NOTFOUND;
	}

	return result;
}