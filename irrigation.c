
#include "contiki.h"
#include "dev/serial-line.h"
#include "cpu/msp430/dev/uart0.h"
#include "helper.h"
#include <stdio.h> /* For printf() */
#include "sys/node-id.h"
/*---------------------------------------------------------------------------*/
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_INFO
/* Configuration */
#define DISCOVERY_INTERVAL (4 * CLOCK_SECOND) // Interval between discovery broadcasts
#define DISCOVERY_WAIT_TIME (10 * CLOCK_SECOND) // Total discovery time
#define HEARTBEAT_INTERVAL (20 * CLOCK_SECOND) // Interval for sending heartbeat messages
#define SEND_INTERVAL (20 * CLOCK_SECOND) // Interval for sending data messages
#define BACKOFF_MAX (3 * CLOCK_SECOND) // Maximum backoff time
#define MIN_INTERVAL  CLOCK_SECOND * 10  // Minimum timer interval
#define MAX_INTERVAL  CLOCK_SECOND * 60  // Maximum timer interval

#define MAX_CANDIDATES 50



PROCESS(irrigation, "Irrigation sensor");
PROCESS(irrigation_timer, "Irrigation timer");
AUTOSTART_PROCESSES(&irrigation);

typedef struct {
    linkaddr_t address;
    uint8_t node_type; // Device type
    uint8_t node_id;
    int16_t signal_strength; // RSSI value
} candidate_t;

static candidate_t candidates[MAX_CANDIDATES];
static int candidate_count = 0;
uint8_t parent_id = 0;
static linkaddr_t null_addr = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
static linkaddr_t parent; // Variable to store the selected parent



void input_callback(const void *data, uint16_t len,
  const linkaddr_t *src, const linkaddr_t *dest)
{
  msg_t rcv_msg;
  dataToStruct(&rcv_msg, data);

  	if ((char)rcv_msg.type == 'i') {
		// Retrieve the RSSI value
		int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
		LOG_INFO("Received signal strength of candidate: %d dBm\n", rssi);

		// Check if we have space for more candidates
		if (candidate_count < MAX_CANDIDATES) {
		    candidates[candidate_count].address = *src;
		    candidates[candidate_count].node_id = rcv_msg.id;
		    candidates[candidate_count].node_type = (uint8_t)(rcv_msg.node - '0');
		    candidates[candidate_count].signal_strength = rssi;
		    candidate_count++;
		} else {
		    LOG_WARN("Candidate list is full, ignoring additional candidates.\n");
		}

       	}else if ((char)rcv_msg.type == '4' && (char)rcv_msg.node == '1' && rcv_msg.id == parent_id){  

  	process_start(&irrigation_timer, NULL); // second process used to set the irrigation timer 

  }
}




/*---------------------------------------------------------------------------*/

PROCESS_THREAD(irrigation, ev, data)
{
        static struct etimer backoff_timer;
        static struct etimer discovery_timer;
        static struct etimer discovery_broadcast_timer;
        static struct etimer heartbeat_timer;
        static struct etimer connection_backoff_timer; // Timer for backoff before sending connection message

  	PROCESS_BEGIN();
// Initialize parent
    linkaddr_copy(&parent, &null_addr);
    candidate_count = 0; // Reset candidate count

    // Set discovery timer
    etimer_set(&discovery_timer, DISCOVERY_WAIT_TIME);

    // Set up periodic discovery broadcasts
    etimer_set(&discovery_broadcast_timer, DISCOVERY_INTERVAL);
    nullnet_set_input_callback(input_callback);


    // Define and set up the backoff timer
    uint16_t backoff_time = random_rand() % CLOCK_SECOND;
    etimer_set(&backoff_timer, backoff_time);

    // Wait until the backoff timer expires
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&backoff_timer));

    while (1) {
        PROCESS_WAIT_EVENT();

        if (ev == PROCESS_EVENT_TIMER) {
            if (data == &discovery_broadcast_timer) {
                // Broadcast discovery request
                msg_t discovery_msg;
                discovery_msg.type = 'a'; // Discovery message type
                discovery_msg.node = '5'; // Device type for irrigation

                uint8_t payload[sizeof(msg_t)];
                structToPayload(&discovery_msg, payload);

                nullnet_buf = payload;
                nullnet_len = sizeof(payload);
                // Broadcast discovery message
                NETSTACK_NETWORK.output(NULL); // Broadcast discovery request
                LOG_INFO("Sent discovery request.\n");

                // Reset discovery broadcast timer
                etimer_reset(&discovery_broadcast_timer);

            } else if (data == &discovery_timer) {



	        // Set random backoff time before sending connection message
                uint16_t backoff_time = (random_rand() % BACKOFF_MAX) + 1;
                etimer_set(&connection_backoff_timer, backoff_time);

                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&connection_backoff_timer));
                // End of discovery phase, select parent
                printf("Candidate count = %d\n", candidate_count);
                candidate_t best_candidate;
                linkaddr_copy(&best_candidate.address, &null_addr);
                best_candidate.signal_strength = -100; // Initialize to very low signal strength

                // Select the best candidate
                for (int i = 0; i < candidate_count; i++) {
                    if (candidates[i].node_type == 2) {
                        // Prefer subgateway if available
                        if (candidates[i].signal_strength > best_candidate.signal_strength) {
                            best_candidate = candidates[i];
                        }
                    }
                }

                if (linkaddr_cmp(&best_candidate.address, &null_addr)) {
                    // No subgateway found, choose any node with the best signal strength
                    for (int i = 0; i < candidate_count; i++) {
                        if (candidates[i].signal_strength > best_candidate.signal_strength) {
                            best_candidate = candidates[i];
                        }
                    }
                }

                if (!linkaddr_cmp(&best_candidate.address, &null_addr)) {
                    linkaddr_copy(&parent, &best_candidate.address);
		    parent_id = best_candidate.node_id;
                    LOG_INFO("Selected parent with address: %02x:%02x and signal strength %d and id %u\n",
                             parent.u8[0], parent.u8[1], best_candidate.signal_strength,best_candidate.node_id);

                    // Send connection message to the selected parent
                    msg_t init_msg;
                    init_msg.type = '0'; // Connection message type
                    init_msg.node = '5'; // Irrigation type
                    init_msg.id = (uint8_t)node_id; // Device ID

                    uint8_t payload[sizeof(msg_t)];
                    structToPayload(&init_msg, payload);

                    nullnet_buf = payload;
                    nullnet_len = sizeof(payload);
                    NETSTACK_NETWORK.output(&parent);

                    LOG_INFO("Sent connection message to parent.\n");

                    // Start heartbeat
                    etimer_set(&heartbeat_timer, HEARTBEAT_INTERVAL);

                    while (1) {
                        PROCESS_WAIT_EVENT();

                        if (ev == PROCESS_EVENT_TIMER) {
                            if (data == &heartbeat_timer) {
                                // Send heartbeat message
                                msg_t heartbeat_msg;
                                heartbeat_msg.type = 'H'; // Heartbeat message type
                                heartbeat_msg.node = '5'; // Irrigation type
                                heartbeat_msg.id = (uint8_t)node_id; // Device ID

                                uint8_t payload[sizeof(msg_t)];
                                structToPayload(&heartbeat_msg, payload);
                                nullnet_buf = payload;
                                nullnet_len = sizeof(payload);

                                if (!linkaddr_cmp(&parent, &null_addr)) {
                                    NETSTACK_NETWORK.output(&parent);
                                    LOG_INFO("Sending heartbeat to parent.\n");
                                }

                                etimer_reset(&heartbeat_timer);
                            }
                        }
                    }
                } else {
                    LOG_INFO("No suitable parent found.\n");
                }

                PROCESS_EXIT();
            }
        }
    }

    PROCESS_END();
}

/*---------------------------------------------------------------------------*/


PROCESS_THREAD(irrigation_timer, ev, data)
{
  static struct etimer periodic_timer;
  uint32_t random_duration;

  PROCESS_BEGIN();
    

     random_duration = (random_rand() % (MAX_INTERVAL - MIN_INTERVAL + 1)) + MIN_INTERVAL;
     etimer_set(&periodic_timer, random_duration);

    printf("Irrigation started \n");
    // message to be sent to the server to inform irrigation stopped
	msg_t init_msg;
	init_msg.type = '4';
	init_msg.node = '5';
	init_msg.id = (uint8_t) node_id;
        init_msg.signal = '1';

	uint8_t payload[sizeof(msg_t)];
	structToPayload(&init_msg, payload);

	nullnet_buf = payload;
	nullnet_len = sizeof(payload);
	if (!linkaddr_cmp(&parent, &null_addr)) {
            NETSTACK_NETWORK.output(&parent);
            LOG_INFO("Sending Irrigation acknowledgment.\n");
        }

    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer)); // periodic timer to switch irrigation mode
	
    printf("Irrigation stopped \n");

	// message to be sent to the server to inform irrigation stopped
	msg_t stop_msg;
	stop_msg.type = '4';
	stop_msg.node = '5';
	stop_msg.id = (uint8_t) node_id;

	uint8_t payload2[sizeof(msg_t)];
	structToPayload(&stop_msg, payload2);
	nullnet_buf = payload2;
	nullnet_len = sizeof(payload2);
	if (!linkaddr_cmp(&parent, &null_addr)) {
            NETSTACK_NETWORK.output(&parent);
            LOG_INFO("Sending Irrigation STOP information to parent.\n");
        }
	

  PROCESS_END();
}

