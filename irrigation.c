
 

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

PROCESS(irrigation, "Irrigation sensor");
PROCESS(irrigation_timer, "Irrigation timer");
AUTOSTART_PROCESSES(&irrigation);




void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  msg_t rcv_msg;
  dataToStruct(&rcv_msg, data);
  
  //message from gateway 
  if((char)rcv_msg.type == '4' && (char)rcv_msg.node== '1'){  


  	process_start(&irrigation_timer, NULL); // second process used to set the irrigation timer 

  }
}




/*---------------------------------------------------------------------------*/

PROCESS_THREAD(irrigation, ev, data)
{


  	PROCESS_BEGIN();

	
	// initial message for new connection to subgateway
  	msg_t init_msg;
	init_msg.type = '0';
	init_msg.unicast = '1';
	init_msg.node = '5';

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);

	nullnet_set_input_callback(input_callback);
	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	NETSTACK_NETWORK.output(NULL);

  
  	PROCESS_END();
}
/*---------------------------------------------------------------------------*/


PROCESS_THREAD(irrigation_timer, ev, data)
{
  static struct etimer periodic_timer;

  PROCESS_BEGIN();
    
    etimer_set(&periodic_timer, SEND_INTERVAL);

    printf("Irrigation started \n");

    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer)); // periodic timer to switch irrigation mode
	
    printf("Irrigation stopped \n");

	// message to be sent to the server to inform irrigation stopped
	msg_t init_msg;
	init_msg.type = '4';
	init_msg.unicast = '1';
	init_msg.node = '5';

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);

	nullnet_set_input_callback(input_callback);
	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	NETSTACK_NETWORK.output(NULL);
	

  PROCESS_END();
}

