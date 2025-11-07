#pragma once

#include <inttypes.h>
#include <stdbool.h>

#define QUEUE_SIZE(d, i)    ((d) * (i) + 4)

void QUEUE_init(uint8_t *queue, uint8_t depth, uint8_t item_size);
void QUEUE_clear(uint8_t *queue);
uint8_t QUEUE_length(const uint8_t *queue);
bool QUEUE_get(uint8_t *queue, void *item);
bool QUEUE_put(uint8_t *queue, const void *item);
