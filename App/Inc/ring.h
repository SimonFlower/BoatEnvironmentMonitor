#ifndef RING_H
#define RING_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define RING_SIZE 1024

// return valuse for RingBufferGetLine()
typedef enum {RBGL_OK, RBGL_NA, RBGL_OVERFLOW} RBGLReturn_t;

typedef struct {
    char buf[RING_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint32_t overflow_count;
} RingBuffer_t;

void RingBufferInit (RingBuffer_t *rb);
bool RingBufferPut (RingBuffer_t *rb, char byte);
bool RingBufferGet (RingBuffer_t *rb, char *byte);
uint16_t RingBufferAvailable (RingBuffer_t *rb);
RBGLReturn_t RingBufferGetLine (RingBuffer_t *rb, char *line, size_t size, bool discard_on_overflow);

#endif /* RING_H */
