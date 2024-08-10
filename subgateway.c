#include "helper.h"
#include "sys/node-id.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"  



// heartbeat to router
#define HEARTBEAT_INTERVAL (20 * CLOCK_SECOND)




// Define a struct to hold the routing table
typedef struct {
    linkaddr_t light_sensors; // One light sensors
    linkaddr_t bulbs[2];         // Two bulbs
    linkaddr_t irrigation_sensor; // One irrigation device
    linkaddr_t router;            // Router address
} routing_table_t;

// Initialize the routing table with default values
static routing_table_t routing_table = {
    .router = {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}
    
};


/**
* @function: input_callback: function called each time a message is received
* @in: 	data : data of the message
*		len : length of the data
*		src : address of the source
*		dest : address of the destination (NULL if broadcast, address of this coordinator if unicast)
*/
void input_callback(const void *data, uint16_t len,
                    const linkaddr_t *src, const linkaddr_t *dest)
{
    msg_t rcv_msg;
    dataToStruct(&rcv_msg, data);

    // Utility function to check if an address is null
    bool is_null_addr(const linkaddr_t *addr) {
        for (int i = 0; i < LINKADDR_SIZE; i++) {
            if (addr->u8[i] != 0) {
                return false;
            }
        }
        return true;
    }

    bool contains_addr(linkaddr_t *table, const linkaddr_t *addr, int size) {
    for (int i = 0; i < size; i++) {
        if (linkaddr_cmp(&table[i], addr)) {
            return true;
        }
    }
    return false;
}


    // Handle new connection requests
    if ((char) rcv_msg.type == '0' && 
        (!linkaddr_cmp(src,&routing_table.light_sensors) && !linkaddr_cmp(src, &routing_table.irrigation_sensor))) {
        
        if ((char) rcv_msg.node == '3') { // Light sensor address copy + confirmation message of connection
            
            linkaddr_copy(&routing_table.light_sensors, src);
            msg_t init_msg;
            init_msg.type = '1';
            init_msg.unicast = '1';
            init_msg.node = '2';

            uint8_t payload[sizeof(msg_t)];
            structToPayload(&init_msg, payload);

            nullnet_set_input_callback(input_callback);
            nullnet_buf = payload;
            nullnet_len = sizeof(payload);

            if (!is_null_addr(&routing_table.light_sensors)) {
                NETSTACK_NETWORK.output(&routing_table.light_sensors);
            }

        } else if ((char) rcv_msg.node == '4') { // Bulb address copy
            
	    if (!contains_addr(routing_table.bulbs, src, 2)) {
		    // Find an empty slot in the bulbs array
		    int empty_slot = find_empty_slot(routing_table.bulbs, 2);
		    if (empty_slot != -1) {
		        linkaddr_copy(&routing_table.bulbs[empty_slot], src);
		    } else {
		        printf("Routing table for bulbs is full.\n");
		    }
            }

        } else {    
            // New connection from irrigation sensor
            linkaddr_copy(&routing_table.irrigation_sensor, src);
        }
    }

    // Data from sensor to be sent to router
    if ((char) rcv_msg.type == '2' && (char) rcv_msg.node == '3' && linkaddr_cmp(src, &routing_table.light_sensors)) {

        nullnet_buf = (uint8_t*) data;
        nullnet_len = len;

        if (!is_null_addr(&routing_table.router)) {
            NETSTACK_NETWORK.output(&routing_table.router);
        }
    }

    // Data from router to bulb
    if ((char) rcv_msg.type == '2' && (char) rcv_msg.node == '1' && linkaddr_cmp(src, &routing_table.router)) {

        nullnet_buf = (uint8_t*) data;
        nullnet_len = len;

        if (!is_null_addr(&routing_table.bulbs[1])) {
            NETSTACK_NETWORK.output(&routing_table.bulbs[1]);
        }
    }

    // Irrigation start message to irrigation
    if ((char) rcv_msg.type == '4' && (char) rcv_msg.node == '1' && linkaddr_cmp(src, &routing_table.router)) {
       
        nullnet_buf = (uint8_t*) data;
        nullnet_len = len;

        if (!is_null_addr(&routing_table.irrigation_sensor)) {
            NETSTACK_NETWORK.output(&routing_table.irrigation_sensor);
        }
    }

    // Irrigation stop confirmation to router
    if ((char) rcv_msg.type == '4' && (char) rcv_msg.node == '5' && linkaddr_cmp(src, &routing_table.irrigation_sensor)) {
      
        nullnet_buf = (uint8_t*) data;
        nullnet_len = len;

        if (!is_null_addr(&routing_table.router)) {
            NETSTACK_NETWORK.output(&routing_table.router);
        }
    }
}



/*---------------------------------------------------------------------------*/
PROCESS(subgateway_process, "Subgateway");

		
//PROCESS(sensor_process, "Sensor");
AUTOSTART_PROCESSES(&subgateway_process);
/*---------------------------------------------------------------------------*/



PROCESS_THREAD(subgateway_process, ev, data)
{
	
	static struct etimer backoff_timer;
	static struct etimer heartbeat_timer;	
        
	
	PROCESS_BEGIN();
	

        // Define and set up the backoff timer
        uint16_t backoff_time = random_rand() % CLOCK_SECOND;
        etimer_set(&backoff_timer, backoff_time);

	// Wait until the backoff timer expires
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&backoff_timer));


	// new connection: sends new connection message to router	
	uint8_t my_node_id = (uint8_t) node_id;    	    	
	msg_t init_msg;
	init_msg.type = '0';
	init_msg.id = my_node_id;
	init_msg.unicast = '1';
	init_msg.node = '2';

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);

	nullnet_set_input_callback(input_callback);
	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	NETSTACK_NETWORK.output(&routing_table.router);


	etimer_set(&heartbeat_timer, HEARTBEAT_INTERVAL);
	
	while (1) {

	
		
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&heartbeat_timer));
		uint8_t my_node_id = (uint8_t) node_id; 
		 

		// Send heartbeat message
		msg_t heartbeat_msg;
		heartbeat_msg.type = 'H'; // Custom type for heartbeat
		heartbeat_msg.node = '2';
		heartbeat_msg.id = node_id;
		uint8_t payload[sizeof(msg_t)];
		structToPayload(&heartbeat_msg, payload);

		nullnet_buf = payload;
		nullnet_len = sizeof(payload);
		
		NETSTACK_NETWORK.output(&routing_table.router);	
		printf("Sending heart beat message from ID %u to router.\n",my_node_id);
		

		etimer_reset(&heartbeat_timer);
    	}
	
    	
	PROCESS_END();
}
