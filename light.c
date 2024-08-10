

#include "contiki.h"
#include "dev/serial-line.h"
#include "cpu/msp430/dev/uart0.h"
#include "helper.h"
#include <stdio.h> /* For printf() */
/*---------------------------------------------------------------------------*/
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO
/* Configuration */
#define SEND_INTERVAL (2 * CLOCK_SECOND)

static linkaddr_t null_addr = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};

linkaddr_t subgateway[1];

void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  msg_t rcv_msg;
  dataToStruct(&rcv_msg, data);

  

  if( (char)rcv_msg.type == '1' && (char)rcv_msg.node=='2'){
	
	//copying address of the subgateway
	
	linkaddr_copy(&subgateway[0],src);

  }


}



PROCESS(light, "Light sensor");
AUTOSTART_PROCESSES(&light);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(light, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();


	etimer_set(&periodic_timer, SEND_INTERVAL);
	linkaddr_copy(&subgateway[0], &null_addr);
		
	// initial message for new connection to the subgateway
  	msg_t init_msg;
	init_msg.type = '0';
	init_msg.unicast = '1';
	init_msg.node = '3';

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);

	nullnet_set_input_callback(input_callback);
	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	NETSTACK_NETWORK.output(NULL);

  
  while(1) {
    
    	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
	
	// message to be sent periodically containing light lvl data 
	msg_t init_msg;
	init_msg.type = '2';
	init_msg.unicast = '1';
	init_msg.node = '3';
	init_msg.data = (char)(random_rand() % 100);

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);
	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	
	NETSTACK_NETWORK.output(&subgateway[0]);
    	etimer_reset(&periodic_timer);
  }

  PROCESS_END();
}




