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
#define SEND_INTERVAL (2 * CLOCK_SECOND) // Interval for sending data messages


#define MAX_CANDIDATES 50
#define MAX_MSG 10

PROCESS(Mobile, "Mobile Terminal");
AUTOSTART_PROCESSES(&Mobile);

typedef struct {
    linkaddr_t address;
    uint8_t node_type; // Device type
    uint8_t node_id;
    int16_t signal_strength; // RSSI value
} candidate_t;

static int message_sent = 1;
static int message_rcv = 1;
static linkaddr_t parent;
static linkaddr_t null_addr = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
static candidate_t candidates[MAX_CANDIDATES];
static int candidate_count = 0;
static uint8_t parent_id = 0;



void input_callback(const void *data, uint16_t len, const linkaddr_t *src, const linkaddr_t *dest) {
    msg_t rcv_msg;
    dataToStruct(&rcv_msg, data);

    if ((char)rcv_msg.type == 'M' && (char)rcv_msg.node == '3') {
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
    } else if ((char)rcv_msg.type == 'Z' && (char)rcv_msg.node == '3' && rcv_msg.id == parent_id) {
        printf("Mobile received back message number %d from Light sensor ID %u.\n", message_rcv, rcv_msg.id);
        message_rcv++;
	if(message_rcv  >  MAX_MSG)
        	printf("Sent and received %d messages. Shutting down...\n", MAX_MSG);
    }
}

/*---------------------------------------------------------------------------*/

PROCESS_THREAD(Mobile, ev, data) {
    static struct etimer discovery_timer;
    static struct etimer discovery_broadcast_timer;
    static struct etimer periodic_timer;
    static struct etimer shutdown_timer;
    
    PROCESS_BEGIN();

    // Initialize parent
    linkaddr_copy(&parent, &null_addr);
    candidate_count = 0; // Reset candidate count

    // Set discovery timer
    etimer_set(&discovery_timer, DISCOVERY_WAIT_TIME);
    
    // Set up periodic discovery broadcasts
    etimer_set(&discovery_broadcast_timer, DISCOVERY_INTERVAL);
    nullnet_set_input_callback(input_callback);

    while (1) {
        PROCESS_WAIT_EVENT();

        if (ev == PROCESS_EVENT_TIMER) {
            if (data == &discovery_broadcast_timer) {
                // Broadcast discovery request
                msg_t discovery_msg;
                discovery_msg.type = 'M'; // Discovery message type
                discovery_msg.node = '6'; // Device type Mobile

                uint8_t payload[sizeof(msg_t)];
                structToPayload(&discovery_msg, payload);

                nullnet_buf = payload;
                nullnet_len = sizeof(payload);
                NETSTACK_NETWORK.output(NULL); // Broadcast discovery request
                LOG_INFO("Sent discovery request.\n");

                // Reset discovery timer for periodic broadcasts
                etimer_reset(&discovery_broadcast_timer);

            } else if (data == &discovery_timer) {
                // End of discovery phase, select parent
                printf("Candidate count = %d\n", candidate_count);
                candidate_t best_candidate;
                linkaddr_copy(&best_candidate.address, &null_addr);
                best_candidate.signal_strength = -100; // Initialize to very low signal strength

                // Select the best candidate
                for (int i = 0; i < candidate_count; i++) {
                    if (candidates[i].node_type == 3) { // Prefer subgateway
                        if (candidates[i].signal_strength > best_candidate.signal_strength) {
                            best_candidate = candidates[i];
                        }
                    }
                }

                if (!linkaddr_cmp(&best_candidate.address, &null_addr)) {
                    linkaddr_copy(&parent, &best_candidate.address);
                    parent_id = best_candidate.node_id;
                    LOG_INFO("Selected parent with address: %02x:%02x and signal strength %d and ID %u\n",
                             parent.u8[0], parent.u8[1], best_candidate.signal_strength, best_candidate.node_id);
		
		        msg_t init_msg;
                        init_msg.type = '0'; // Connection message type
                        init_msg.node = '6'; // Mobile type
                        init_msg.id = (uint8_t)node_id; // Device ID

                        uint8_t payload[sizeof(msg_t)];
                        structToPayload(&init_msg, payload);

                        nullnet_buf = payload;
                        nullnet_len = sizeof(payload);

                        NETSTACK_NETWORK.output(&parent); // Send message to the selected parent
                           
                            
                      

			
                    etimer_set(&periodic_timer, SEND_INTERVAL);

                    while (1) {
                        PROCESS_WAIT_EVENT();

                        if (ev == PROCESS_EVENT_TIMER) {
                            if (data == &periodic_timer) {
                                // Send mobile maintenance message to the selected parent
                                msg_t init_msg;
                                init_msg.type = '2'; // Connection message type
                                init_msg.node = '6'; // Mobile type
                                init_msg.id = (uint8_t)node_id; // Device ID

                                uint8_t payload[sizeof(msg_t)];
                                structToPayload(&init_msg, payload);

                                nullnet_buf = payload;
                                nullnet_len = sizeof(payload);

                                if (message_sent <= MAX_MSG) {
                                    NETSTACK_NETWORK.output(&parent); // Send message to the selected parent
                                    printf("Mobile sent maintenance message %d to Light sensor ID %u.\n", message_sent++, parent_id);
                                    
                                }

                                if (message_sent > MAX_MSG) {
                                    etimer_set(&shutdown_timer, 2);

				    // Wait until the backoff timer expires
				    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&shutdown_timer));
                                    
                                    PROCESS_EXIT();
                                }

                                // Reset periodic timer
                                etimer_reset(&periodic_timer);
                            }
                        }
                    }
                } else {
                    LOG_INFO("No suitable parent found.\n");
                    PROCESS_EXIT(); // Exit if no suitable parent is found
                }
            }
        }
    }

    PROCESS_END();
}


