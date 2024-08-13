#include "helper.h"
#include "sys/node-id.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"  
#include "sys/clock.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO


// Heartbeat sent to router
#define HEARTBEAT_INTERVAL (20 * CLOCK_SECOND)

// Interval to check for timed out devices
#define CHECK_INTERVAL (65 * CLOCK_SECOND) 

// Maximum number of devices
#define MAX_DEVICES 10
#define MAX_BULBS 2

typedef struct {
    linkaddr_t address;
    uint8_t device_type; // e.g., LIGHT_SENSOR, BULB, IRRIGATION
    clock_time_t last_heartbeat; // Timestamp of the last heartbeat
} device_entry_t;

typedef struct {
    device_entry_t devices[MAX_DEVICES];
    int device_count;
    linkaddr_t router; // Router address
} routing_table_t;

// Initialize the routing table with default values
static routing_table_t routing_table = {
    .router = {{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }},
    .device_count = 0
};

// Set default values for devices in the routing table
void init_routing_table() {
    for (int i = 0; i < MAX_DEVICES; i++) {
        routing_table.devices[i].device_type = 0; // Mark slot as empty
        routing_table.devices[i].last_heartbeat = 0; // Initialize heartbeat time
    }
}

// Utility function to check if an address is in the routing table
bool address_in_routing_table(const linkaddr_t *addr) {
    for (int i = 0; i < routing_table.device_count; i++) {
        if (linkaddr_cmp(addr, &routing_table.devices[i].address)) {
            return true;
        }
    }
    return false;
}

// Utility function to find a device entry in the routing table
device_entry_t *find_device_in_routing_table(const linkaddr_t *addr) {
    for (int i = 0; i < routing_table.device_count; i++) {
        if (linkaddr_cmp(addr, &routing_table.devices[i].address)) {
            return &routing_table.devices[i];
        }
    }
    return NULL;
}

// Utility function to find an empty slot in the routing table
int find_empty_slot() {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (routing_table.devices[i].device_type == 0) {
            return i;
        }
    }
    return -1;
}
// Utility function to check if a device of a given type exists in the routing table
bool has_device_of_type(uint8_t type) {
    for (int i = 0; i < routing_table.device_count; i++) {
        if (routing_table.devices[i].device_type == type) {
            return true;
        }
    }
    return false;
}

// Utility function to check if the routing table can accept a new device of the given type
bool can_accept_device_of_type(uint8_t type) {
    if (type == 4) { // Bulb
        int bulb_count = 0;
        for (int i = 0; i < routing_table.device_count; i++) {
            if (routing_table.devices[i].device_type == 4) {
                bulb_count++;
            }
        }
        return bulb_count < MAX_BULBS;
    }
    return !has_device_of_type(type);
}

void print_routing_table() {
    printf("Routing Table:\n");
    printf("Index | Address              | Device Type\n");
    printf("------|----------------------|-------------\n");

    for (int i = 0; i < routing_table.device_count; i++) {
        printf("%d     | %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x | %u\n",
               i,
               routing_table.devices[i].address.u8[0],
               routing_table.devices[i].address.u8[1],
               routing_table.devices[i].address.u8[2],
               routing_table.devices[i].address.u8[3],
               routing_table.devices[i].address.u8[4],
               routing_table.devices[i].address.u8[5],
               routing_table.devices[i].address.u8[6],
               routing_table.devices[i].address.u8[7],
               routing_table.devices[i].device_type);
    }

    // Calculate and print the number of slots left
    int slots_left = MAX_DEVICES - routing_table.device_count;
    printf("Slots left in the table: %d\n", slots_left);
}

// Handle new connection requests
void input_callback(const void *data, uint16_t len,
                    const linkaddr_t *src, const linkaddr_t *dest) {
    msg_t rcv_msg;
    dataToStruct(&rcv_msg, data);

    // printf("message recieved in sub : type %c , node %c \n",(char)rcv_msg.type,(char)rcv_msg.node);

     if ((char) rcv_msg.type == 'a') { // Discovery message type
	
	
 	uint8_t device_type = (uint8_t)(rcv_msg.node - '0');
        if (can_accept_device_of_type(device_type)) {
            // Respond to discovery request
            msg_t response_msg;
            char temp;
            if(device_type==3){
		temp = 'l';
	    }else if(device_type==4){
		temp = 'b';
	    }else {
	    	temp = 'i';
            }
            response_msg.type = temp; // Response message type
            response_msg.node = '2'; // Subgateway node ID
            response_msg.id = (uint8_t) node_id;
	 
            uint8_t payload[sizeof(msg_t)];
            structToPayload(&response_msg, payload);

            nullnet_buf = payload;
            nullnet_len = sizeof(payload);

            NETSTACK_NETWORK.output(src); // Send response to the requesting sensor
            LOG_INFO("Responding to discovery request from device type %u.\n", device_type);
        } else {
            LOG_INFO("No available slots for device type %c.\n", device_type);
        }
    }

    if ((char) rcv_msg.type == 'H' && (rcv_msg.node == '3' || rcv_msg.node == '4' || rcv_msg.node == '5')) { // Heartbeat message
        device_entry_t *device = find_device_in_routing_table(src);
        if (device) {
            device->last_heartbeat = clock_time(); // Update the last heartbeat time
	    printf("Heartbeat received from device id %u\n",rcv_msg.id );
        }else{
	
	   printf("Unknown heartbeat source. Ignoring.\n");
	}
    }

    // Handle new connection requests
    else if ((char) rcv_msg.type == '0') {
	
        // Check if the source address is already in the routing table
        if (!address_in_routing_table(src)) {
	    printf("Adding Adress of node ID %u to the routing table\n",rcv_msg.id);
            // Find an empty slot in the routing table
            int empty_slot = find_empty_slot();
            if (empty_slot != -1) {
                if ((char) rcv_msg.node == '3') { // Light sensor address copy
                    routing_table.devices[empty_slot].address = *src;
                    routing_table.devices[empty_slot].device_type = 3; // Light sensor type
                    routing_table.device_count++;
                } else if ((char) rcv_msg.node == '4') { // Bulb address copy
                    routing_table.devices[empty_slot].address = *src;
                    routing_table.devices[empty_slot].device_type = 4; // Bulb type
                    routing_table.device_count++;
                } else if ((char) rcv_msg.node == '5') { // Irrigation sensor address copy
                    routing_table.devices[empty_slot].address = *src;
                    routing_table.devices[empty_slot].device_type = 5; // Irrigation type
                    routing_table.device_count++;
                }
            } else {
                printf("Routing table is full.\n");
            }
        }
    }

    // Data from sensor to be sent to router
    if ((char) rcv_msg.type == '2' && (char) rcv_msg.node == '3') {

        device_entry_t *device = find_device_in_routing_table(src);
        if (device && device->device_type == 3) { // Ensure it's the correct light sensor
            msg_t init_msg;
	    init_msg.type = '2';
	    init_msg.id = (uint8_t)node_id;
	    init_msg.data = rcv_msg.data;
	    init_msg.node = '3';

	    uint8_t payload[sizeof(msg_t)];
	    structToPayload(&init_msg, payload);

	    nullnet_set_input_callback(input_callback);
	    nullnet_buf = payload;
	    nullnet_len = sizeof(payload);

            if (!linkaddr_cmp(&routing_table.router, &linkaddr_null)) {
                NETSTACK_NETWORK.output(&routing_table.router);
            }
        }
    }

    // Data from router to bulbs
    if ((char) rcv_msg.type == '2' && (char) rcv_msg.node == '1') {
           
            nullnet_buf = (uint8_t*) data;
            nullnet_len = len;

            // Iterate through the routing table to find all bulbs and send data to them
            for (int i = 0; i < routing_table.device_count; i++) {
                if (routing_table.devices[i].device_type == 4) { // Bulb type
                    NETSTACK_NETWORK.output(&routing_table.devices[i].address);
                }
            }
    }

    // Irrigation start message to irrigation
    if ((char) rcv_msg.type == '4' && (char) rcv_msg.node == '1') {
	   
            msg_t irrig_msg;
	    irrig_msg.type = '4';
	    irrig_msg.id = (uint8_t)node_id;
	    irrig_msg.node = '1';

	    uint8_t payload[sizeof(msg_t)];
	    structToPayload(&irrig_msg, payload);

	    nullnet_buf = payload;
	    nullnet_len = sizeof(payload);
            // Iterate through the routing table to find irrigation device and send data to it
            for (int i = 0; i < routing_table.device_count; i++) {
                if (routing_table.devices[i].device_type == 5) { // irrigation type
			
                    NETSTACK_NETWORK.output(&routing_table.devices[i].address);
                }
            }
        
    }

    // Irrigation ack/stop confirmation to router
    if ((char) rcv_msg.type == '4' && (char) rcv_msg.node == '5') {
        device_entry_t *device = find_device_in_routing_table(src);
        if (device && device->device_type == 5) { // Ensure it's the correct irrigation sensor
	    
            msg_t irrig_msg;
	    irrig_msg.type = '4';
	    irrig_msg.id = (uint8_t)node_id;
	    irrig_msg.node = '5';
            if((char)rcv_msg.signal == '1')
		irrig_msg.signal='1';
		

	    uint8_t payload[sizeof(msg_t)];
	    structToPayload(&irrig_msg, payload);

	    nullnet_buf = payload;
	    nullnet_len = sizeof(payload);

            if (!linkaddr_cmp(&routing_table.router, &linkaddr_null)) {
                NETSTACK_NETWORK.output(&routing_table.router);
            }
        }
    }
}

/*---------------------------------------------------------------------------*/
PROCESS(subgateway_process, "Subgateway");

AUTOSTART_PROCESSES(&subgateway_process);
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(subgateway_process, ev, data) {
    static struct etimer backoff_timer;
    //static struct etimer heartbeat_timer;
    static struct etimer check_timer; // Timer for checking device timeouts

    PROCESS_BEGIN();

    init_routing_table(); // Initialize routing table

    // Define and set up the backoff timer
    uint16_t backoff_time = random_rand() % CLOCK_SECOND;
    etimer_set(&backoff_timer, backoff_time);

    // Wait until the backoff timer expires
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&backoff_timer));

    // New connection: sends new connection message to router
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

   // etimer_set(&heartbeat_timer, HEARTBEAT_INTERVAL);
    etimer_set(&check_timer, CHECK_INTERVAL);

    while (1) {
        PROCESS_WAIT_EVENT();

        if (ev == PROCESS_EVENT_TIMER) {
	/*
            if (data == &heartbeat_timer) {
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
                printf("Sending heartbeat message from ID %u to router.\n", my_node_id);

                etimer_reset(&heartbeat_timer);
            }*/

            if (data == &check_timer) {
		    // Check for timed out devices
		    clock_time_t current_time = clock_time();
		    int i = 0;
		    while (i < routing_table.device_count) {
			if (current_time - routing_table.devices[i].last_heartbeat > HEARTBEAT_INTERVAL) {
			    // Device has timed out
			    printf("Device of type %u has timed out.\n", routing_table.devices[i].device_type);

			    // Optionally, reset last_heartbeat or any other relevant fields
			    routing_table.devices[i].last_heartbeat = 0;

			    // Remove device from the routing table
			    for (int j = i; j < routing_table.device_count - 1; j++) {
				routing_table.devices[j] = routing_table.devices[j + 1];
			    }
			    routing_table.device_count--;
			    // Note: Do not increment i here as we need to check the new device at index i
			} else {
			    i++; // Move to the next device if current one is not timed out
			}
		    }

		    print_routing_table(); // Print routing table after cleanup

		    etimer_reset(&check_timer);
	   }
        }
    }

    PROCESS_END();
}

