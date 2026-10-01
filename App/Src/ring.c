/** a simple fixed size ring buffer implementation where data can be inserted
 * and extracted asynchronously */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "stm32l4xx.h"

#include "ring.h"

// Hardware Data Memory Barrier macro for ARM Cortex-M
#ifndef memory_barrier
#define memory_barrier() __DMB()
#endif

/**
 * @brief Intiailise a ring buffer
 * @param rb the buffer to initialise
 */
void RingBufferInit (RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->overflow_count = 0;
}

/**
 * @brief Empty a ring buffer
 * @param rb the buffer to empty
 */
void RingBufferClear (RingBuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

/**
 * @brief add a byte to the ring buffer
 * @param rb the ring buffer
 * @param byte the byte to add
 * @retval true if the byte was added */
bool RingBufferPut (RingBuffer_t *rb, char byte) {
    uint16_t next = (rb->head + 1) % RING_SIZE;

    if (next == rb->tail) {
        rb->overflow_count++;
        return false;
    }
    rb->buf[rb->head] = byte;
	// Ensure byte write to buf is committed to RAM before head index updates
    memory_barrier();
    rb->head = next;

    return true;
}

/**
 * @brief get a byte from the ring buffer
 * @param rb the ring buffer
 * @param byte the byte to retrieve
 * @retval true if the byte was retrieved */
bool RingBufferGet (RingBuffer_t *rb, char *byte) {
    if (rb->head == rb->tail) return false;

    *byte = rb->buf[rb->tail];
	// Ensure byte read completes before tail index updates
    memory_barrier();
    rb->tail = (rb->tail + 1) % RING_SIZE;

    return true;
}

/**
 * @brief find the number of bytes in the buffer
 * @param rb the ring buffer
 * @retval the number of bytes 
 */
uint16_t RingBufferAvailable (RingBuffer_t *rb) {
    if (rb->head >= rb->tail)
        return rb->head - rb->tail;

    return RING_SIZE - rb->tail + rb->head;
}

/**
 * @brief get a newline terminated string from the ring buffer
 * 
 * The string is always null terminated
 * 
 * @param rb the ring buffer
 * @param line the line without its terminating newline
 * @param size the size of the line
 * @param discard_on_overflow in the event of an overflow, discard data
 *        from the ring buffer up to the size of the line
 * @retval RBGL_OK - a line was retrieved
 *         RBGL_NA - a line was not present in the ring buffer
 *         RBGL_OVERFLOW - the line is too small for the data
 */
RBGLReturn_t RingBufferGetLine (RingBuffer_t *rb, char *line, size_t size, bool discard_on_overflow) {
    if (size == 0) return RBGL_OVERFLOW;

    // Snapshot head and tail atomically to prevent race conditions
    uint16_t snap_head = rb->head;
    uint16_t snap_tail = rb->tail;
	memory_barrier();

    // Calculate available bytes based on snapshot
    uint16_t available = (snap_head >= snap_tail) 
                         ? (snap_head - snap_tail) 
                         : (RING_SIZE - snap_tail + snap_head);

	// use a separate index for the output data
    size_t out_idx = 0;

    for (uint16_t count = 0; count < available; count++) {
        uint16_t pos = (snap_tail + count) % RING_SIZE;
        char ch = rb->buf[pos];

        if (ch == '\n') {
            line[out_idx] = '\0';
            // Consumer safely updates tail without caring if head changed concurrently
			memory_barrier();
            rb->tail = (pos + 1) % RING_SIZE;
            return RBGL_OK;
        } else if (ch == '\r') {
            continue;
        } else {
            if (out_idx >= size - 1) {
                if (discard_on_overflow) {
					// Consumer safely updates tail without caring if head changed concurrently
					memory_barrier();
                    rb->tail = (pos + 1) % RING_SIZE;
                }
                line[out_idx] = '\0';
                return RBGL_OVERFLOW;
            }
            line[out_idx++] = ch;
        }
    }

    return RBGL_NA;
}
