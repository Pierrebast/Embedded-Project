#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "contiki.h"
#include "random.h"
#include "clock.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "dev/radio.h"
#include "random.h"
#include "net/packetbuf.h"
#include "dev/leds.h"
#include "sys/log.h"
#include "cc2420.h"

typedef struct message{
	uint8_t type; 	// 0 = new connection
			// 1 = connection accepted
			// 2 = sending sensor data
			// 4 = irrigation
			// H = heartbeat message from devices to subgateway
			// a = advertisment 
			// l,b,i = customed responses for light,bulb,irrigation candidates.
			
			
	uint8_t unicast; 	// 0 = broadcast
				// 1 = unicast
			
	uint8_t id;	// node id
			

	uint8_t node;	
			// 1 = gateway
			// 2 = sub-gateway
			// 3 = ligh-sensor
			// 4 = light-bulb
			// 5 = irrigation-system
			// 6 = mobile Terminal
	uint8_t signal;

	uint32_t data;	// sensor data (range 0-100)
			// bulb 0 
			
} msg_t;





void structToPayload(msg_t* s, uint8_t* payload);

void dataToStruct(msg_t* s, const void* data);

uint32_t u8_to_u32(const uint8_t* bytes);

void u32_to_u8(const uint32_t u32, uint8_t* u8);

int containsAddr(linkaddr_t* array, const linkaddr_t* item);


void pktt(uint8_t type, uint8_t unicast, uint8_t id, uint8_t node, uint8_t signal, uint32_t data);


