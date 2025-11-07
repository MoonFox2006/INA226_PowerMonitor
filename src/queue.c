#include <string.h>
#include "queue.h"

typedef volatile struct __attribute__((__packed__)) {
    uint8_t depth, item_size;
    uint8_t len, pos;
    uint8_t items[0];
} queue_t;

void QUEUE_init(uint8_t *queue, uint8_t depth, uint8_t item_size) {
    ((queue_t*)queue)->depth = depth;
    ((queue_t*)queue)->item_size = item_size;
    ((queue_t*)queue)->len = 0;
    ((queue_t*)queue)->pos = 0;
}

void QUEUE_clear(uint8_t *queue) {
    ((queue_t*)queue)->len = 0;
    ((queue_t*)queue)->pos = 0;
}

inline __attribute__((always_inline)) uint8_t QUEUE_length(const uint8_t *queue) {
    return ((queue_t*)queue)->len;
}

bool QUEUE_get(uint8_t *queue, void *item) {
    if (! ((queue_t*)queue)->len)
        return false;
    if (((queue_t*)queue)->item_size == 1)
        *(uint8_t*)item = ((queue_t*)queue)->items[((queue_t*)queue)->pos];
    else if (((queue_t*)queue)->item_size == 2)
        *(uint16_t*)item = *(uint16_t*)&((queue_t*)queue)->items[((queue_t*)queue)->pos << 1];
    else if (((queue_t*)queue)->item_size == 4)
        *(uint32_t*)item = *(uint32_t*)&((queue_t*)queue)->items[((queue_t*)queue)->pos << 2];
    else
        memcpy(item, (const void*)&((queue_t*)queue)->items[((queue_t*)queue)->pos * ((queue_t*)queue)->item_size], ((queue_t*)queue)->item_size);
    if (++((queue_t*)queue)->pos >= ((queue_t*)queue)->depth)
        ((queue_t*)queue)->pos = 0;
    --((queue_t*)queue)->len;
    return true;
}

bool QUEUE_put(uint8_t *queue, const void *item) {
    if (((queue_t*)queue)->len >= ((queue_t*)queue)->depth)
        return false;
    if (((queue_t*)queue)->item_size == 1)
        ((queue_t*)queue)->items[(((queue_t*)queue)->pos + ((queue_t*)queue)->len) & (((queue_t*)queue)->depth - 1)] = *(uint8_t*)item;
    else if (((queue_t*)queue)->item_size == 2)
        *(uint16_t*)&((queue_t*)queue)->items[((((queue_t*)queue)->pos + ((queue_t*)queue)->len) & (((queue_t*)queue)->depth - 1)) << 1] = *(uint16_t*)item;
    else if (((queue_t*)queue)->item_size == 4)
        *(uint32_t*)&((queue_t*)queue)->items[((((queue_t*)queue)->pos + ((queue_t*)queue)->len) & (((queue_t*)queue)->depth - 1)) << 2] = *(uint32_t*)item;
    else
        memcpy((void*)&((queue_t*)queue)->items[((((queue_t*)queue)->pos + ((queue_t*)queue)->len) & (((queue_t*)queue)->depth - 1)) * ((queue_t*)queue)->item_size], item, ((queue_t*)queue)->item_size);
    ++((queue_t*)queue)->len;
    return true;
}
