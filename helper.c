#include "helper.h"

void structToPayload(msg_t* s, uint8_t* payload){
	*payload = s->type;
	*(payload+sizeof(uint8_t)) = s->unicast;
	*(payload+2*sizeof(uint8_t)) = s->id;
	*(payload+3*sizeof(uint8_t)) = s->node;
    *(payload+4*sizeof(uint8_t)) = s->signal;
	u32_to_u8((const uint32_t) s->data, payload+5*sizeof(uint8_t));
}

void dataToStruct(msg_t* s, const void* data){
	s->type = (uint8_t) *((uint8_t*)data);
	s->unicast = (uint8_t) *((uint8_t*)data+sizeof(uint8_t));
	s->id = (uint8_t) *((uint8_t*)data+2*sizeof(uint8_t));
    s->node = (uint8_t) *((uint8_t*)data+3*sizeof(uint8_t));
	s->signal = (uint8_t) *((uint8_t*)data+4*sizeof(uint8_t));
	s->data = u8_to_u32((const uint8_t*)data+5*sizeof(uint8_t));
}

uint32_t u8_to_u32(const uint8_t* bytes) {
  uint32_t u32 = ((uint32_t)bytes[0] << 24) + ((uint32_t)bytes[1] << 16) + ((uint32_t)bytes[2] << 8) + (uint32_t)bytes[3];
  return u32;
}

void u32_to_u8(const uint32_t u32, uint8_t* u8) {
  u8[0] = (u32 & 0xff000000) >> 24;
  u8[1] = (u32 & 0x00ff0000) >> 16;
  u8[2] = (u32 & 0x0000ff00) >> 8;
  u8[3] = u32 & 0x000000ff;
}

int containsAddr(linkaddr_t* array, const linkaddr_t* item){
for (int i = 0; i < 4; i++) {
        if (linkaddr_cmp(array+(i*sizeof(linkaddr_t)), item)) {
            return 1;
        }
    }
return 0;
}

// Utility function to find the first empty slot in the routing table
int find_empty_slot(linkaddr_t *table, int size) {
    for (int i = 0; i < size; i++) {
        if (table[i].u8[0] == 0 && table[i].u8[1] == 0) { // Check if the address is NULL
            return i;
        }
    }
    return -1; // No empty slot found
}

void pktt(uint8_t type, uint8_t unicast, uint8_t id, uint8_t node, uint8_t signal, uint32_t data)
{
    struct message pkt;
    pkt.type = type;
    pkt.unicast = unicast;;
    pkt.id = id;
    pkt.node = node;
    pkt.signal = signal;
    pkt.data = data;

    uint8_t payload[sizeof(msg_t)];
	structToPayload(&pkt, payload);

    nullnet_buf = payload;
    nullnet_len = sizeof(payload);
}
