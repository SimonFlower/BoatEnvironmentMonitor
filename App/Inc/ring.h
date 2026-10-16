#ifndef RING_H
#define RING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// return values for RingBufferGetLine()
typedef enum {RBGL_OK, RBGL_NA, RBGL_OVERFLOW} RBGLReturn_t;

typedef struct {
    volatile uint8_t *buf;
    size_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t overflow_count;
} RingBuffer_t;

void RingBufferInit (RingBuffer_t *rb, uint8_t *storage, size_t size);
void RingBufferClear (RingBuffer_t *rb);
bool RingBufferPut (RingBuffer_t *rb, uint8_t byte);
bool RingBufferGet (RingBuffer_t *rb, uint8_t *byte);
uint16_t RingBufferAvailable (RingBuffer_t *rb);
RBGLReturn_t RingBufferGetLine (RingBuffer_t *rb, char *line, size_t size, bool discard_on_overflow);

#endif /* RING_H */
