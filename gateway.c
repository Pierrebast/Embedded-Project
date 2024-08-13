#include "contiki.h"
#include "dev/serial-line.h"
#include "cpu/msp430/dev/uart0.h"
#include "helper.h"
#include <inttypes.h> 
#include "sys/node-id.h"
#include "sys/etimer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/clock.h" 



/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO

/* Configuration */

#define SEND_INTERVAL (2 * CLOCK_SECOND)


#define MAX_GREENHOUSES 4



typedef struct {
    linkaddr_t address;
    uint8_t greenhouse_id;
    clock_time_t last_heartbeat;
} subgateway_t;

subgateway_t subgateways[MAX_GREENHOUSES];
uint8_t nbSubgateway = 0;

/*---------------------------------------------------------------------------*/
/* Function to handle heartbeat messages */
void handle_heartbeat(const linkaddr_t *src) {
    for (int i = 0; i < nbSubgateway; i++) {
        if (linkaddr_cmp(&subgateways[i].address, src)) {
            subgateways[i].last_heartbeat = clock_time();
            printf("Heartbeat received from sub-gateway %u\n", subgateways[i].greenhouse_id);
            return;
        }
    }
    printf("Unknown heartbeat source. Ignoring.\n");
}

/*---------------------------------------------------------------------------*/


PROCESS(test_serial, "Receive messages from server and modify values");
PROCESS(receive, "Receive messages from others");

AUTOSTART_PROCESSES(&test_serial, &receive);

int irrigation = 0;
int bulb=0;
uint8_t id=0;


/* Input callback to handle incoming messages */
void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest)
{
    msg_t rcv_msg;
    dataToStruct(&rcv_msg, data);

  /*
     if ((char)rcv_msg.type == 'H' && (char)rcv_msg.node == '2') { 

	// Handle heartbeat message
        handle_heartbeat(src);

    }*/

    if ((char)rcv_msg.type == '0' && (char)rcv_msg.node == '2') { // New sub-gateway connection request
	
	
		
        if (nbSubgateway < MAX_GREENHOUSES) {



    	    // Retrieve the correct Mote ID
	    printf("Message from Mote ID: %u\n", rcv_msg.id);

	    // Retrieve the RSSI
	    int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
	    printf("Received signal strength: %d dBm\n",(int8_t) rssi);
            linkaddr_copy(&subgateways[nbSubgateway].address, src);
            subgateways[nbSubgateway].greenhouse_id = rcv_msg.id; // Assign greenhouse ID
            
            // Send connection confirmation + assigned ID back to sub-gateway
            msg_t confirm_msg = {1, 1, subgateways[nbSubgateway].greenhouse_id, 1, 0, 0};
            nbSubgateway++;

            uint8_t payload[sizeof(msg_t)];
            structToPayload(&confirm_msg, payload);

            nullnet_buf = payload;
            nullnet_len = sizeof(payload);
            NETSTACK_NETWORK.output(src); // Send the ID back to the requesting sub-gateway
        } else {
            LOG_INFO("Max number of greenhouses reached!\n");
        }
    } 
    // Handle data from sensors and forward to server
    else if ((char)rcv_msg.type == '2' && (char)rcv_msg.node == '3') {
	id = rcv_msg.id;
	printf("ID bulb data: %u\n",id);
        printf("%lu\n",rcv_msg.data); 
    } 
    else if ((char)rcv_msg.type == '4' && (char)rcv_msg.node == '5') {
		if((char)rcv_msg.signal == '1'){
			printf("Acknowledgment for the irrigation system of the greenhouse %u \n",rcv_msg.id);
		}else{
        		printf("Irrigation system of the greenhouse %u stopped successfully\n",rcv_msg.id);
		}
    } 
    else {
        LOG_INFO("Received unknown type,ignoring.\n");
	
    }
}

/*---------------------------------------------------------------------------*/
/* Main process to handle serial input from the server */
PROCESS_THREAD(test_serial, ev, data)
{
    PROCESS_BEGIN();
    serial_line_init();
    uart0_set_input(serial_line_input_byte);

    LOG_INFO("Starting serial process\n");
  

    while (1) {
        PROCESS_YIELD();
        if (ev == serial_line_event_message) {
            
            if (strcmp(data, "4") == 0) {
		// Handle irrigation message
		irrigation = 1;
            }
	    if (strcmp(data, "1") == 0) {
		// Handle irrigation message
		bulb = 1;
	    }
	
    	}

    }
    PROCESS_END();
}

/*---------------------------------------------------------------------------*/
/* Main process to handle receiving and multicast messaging */
PROCESS_THREAD(receive, ev, data)
{
    static struct etimer periodic_timer;
  


    PROCESS_BEGIN();
    nullnet_set_input_callback(input_callback);
    NETSTACK_NETWORK.output(NULL);
    etimer_set(&periodic_timer, SEND_INTERVAL);


    while (1) {
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

	/*if(subgateways!=NULL){
		for (int i = 0; i < nbSubgateway; i++) {
		    
			LOG_INFO("Sub-gateway %u is still connected\n", subgateways[i].greenhouse_id);
		}
	}*/
        if (irrigation == 1) { // Multicast irrigation message
            irrigation = 0;
            msg_t irri_msg = {'4', '0', '0', '1', '0', 0}; // Type 4, node type 1
            uint8_t payload[sizeof(msg_t)];
            structToPayload(&irri_msg, payload);
            nullnet_buf = payload;
            nullnet_len = sizeof(payload);
	  
 	    

           for(int i=0;i < nbSubgateway;i++) {
                // Send to the current sub-gateway
                NETSTACK_NETWORK.output(&subgateways[i].address); 
                //printf("Gateway sending irrigation message to greenhouse ID %u with address ", subgateways[i].greenhouse_id);
                
             
            }
            
        }

        if (bulb == 1) { // Multicast bulb control message
            bulb = 0;
            msg_t bulb_msg = {'2', '0', id, '1', '0', 0}; // Type 2, node type 1
            uint8_t payload[sizeof(msg_t)];
            structToPayload(&bulb_msg, payload);
            nullnet_buf = payload;
            nullnet_len = sizeof(payload);

            for (int i = 0; i < nbSubgateway; i++) {
                
                
                if(subgateways[i].greenhouse_id == id)
		  // printf("Data sent back to subgateway ID %u\n",subgateways[i].greenhouse_id);
                   NETSTACK_NETWORK.output(&subgateways[i].address); // Send to corresponding id sub-gateway
		   
            }
	    id=0;
        }

	/*
	clock_time_t current_time = clock_time();
	for (int i = 0; i < nbSubgateway; i++) {
	    if (current_time - subgateways[i].last_heartbeat > HEARTBEAT_TIMEOUT) {
		LOG_INFO("Sub-gateway %u is disconnected\n", subgateways[i].greenhouse_id);
		
		// Shift the remaining sub-gateways up to fill the gap
		for (int j = i; j < nbSubgateway - 1; j++) {
		    subgateways[j] = subgateways[j + 1];
		}

		// Decrease the count of sub-gateways
		nbSubgateway--;

		// Adjust the loop index since we've modified the array
		i--;
	    }
	}*/

        etimer_reset(&periodic_timer);
    }

    PROCESS_END();
}

