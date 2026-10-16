/** a simple fixed size ring fer implementation where data can be inserted
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
 * 
 * Call this only once before any other operation on the ring buffer
 * 
 * @param rb the buffer to initialise - silently ignore missing ring buffer
 * @param buf the storage to use - this must not be used by any other code
 * @param size the length of the storage, in bytes
 */
void RingBufferInit (RingBuffer_t *rb, uint8_t *buf, size_t size) {
	if (rb) {
		rb->buf = buf;
		rb->size = size;
		rb->head = 0;
		rb->tail = 0;
		rb->overflow_count = 0;
	}
}

/**
 * @brief Empty a ring buffer
 * @param rb the buffer to empty - silently ignore missing ring buffer
 */
void RingBufferClear (RingBuffer_t *rb) {
	if (rb) {
		rb->head = 0;
		rb->tail = 0;
	}
}

/**
 * @brief add a byte to the ring buffer
 * @param rb the ring buffer
 * @param byte the byte to add
 * @retval true if the byte was added */
bool RingBufferPut (RingBuffer_t *rb, uint8_t byte) {
	if (! rb) return false;
		
    uint16_t next = (rb->head + 1) % rb->size;

    if (next == rb->tail) {
        rb->overflow_count++;
        return false;
    }
    *(rb->buf + rb->head) = byte;
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
bool RingBufferGet (RingBuffer_t *rb, uint8_t *byte) {
	if (! rb) return false;
    if (rb->head == rb->tail) return false;

    *byte = *(rb->buf + rb->tail);
	// Ensure byte read completes before tail index updates
    memory_barrier();
    rb->tail = (rb->tail + 1) % rb->size;

    return true;
}

/**
 * @brief find the number of bytes in the buffer
 * @param rb the ring buffer
 * @retval the number of bytes 
 */
uint16_t RingBufferAvailable (RingBuffer_t *rb) {
	if (! rb) return 0;
    if (rb->head >= rb->tail)
        return rb->head - rb->tail;

    return rb->size - rb->tail + rb->head;
}

/**
 * @brief get a newline terminated string from the ring buffer
 * 
 * The string is always null terminated
 * 
 * @param rb the ring buffer
 * @param line the line, including its terminating newline
 * @param size the size of the line which must be at least 2 bytes
 * @param discard_on_overflow in the event of an overflow, discard data
 *        from the ring buffer up to the size of the line
 * @retval RBGL_OK - a line was retrieved
 *         RBGL_NA - a line was not present in the ring buffer
 *         RBGL_OVERFLOW - the line is too small for the data
 */
RBGLReturn_t RingBufferGetLine (RingBuffer_t *rb, char *line, size_t size, bool discard_on_overflow) {
	if (! rb) return RBGL_NA;
    if (size <= 1) return RBGL_OVERFLOW;

    // Snapshot head and tail atomically to prevent race conditions
    uint16_t snap_head = rb->head;
    uint16_t snap_tail = rb->tail;
	memory_barrier();

    // Calculate available bytes based on snapshot
    uint16_t available = (snap_head >= snap_tail) 
                         ? (snap_head - snap_tail) 
                         : (rb->size - snap_tail + snap_head);

	// use a separate index for the output data
    size_t out_idx = 0;

    for (uint16_t count = 0; count < available; count++) {
        uint16_t pos = (snap_tail + count) % rb->size;
        uint8_t ch = *(rb->buf + pos);

		// check for overflow - keep 2 bytes spare because if there is
		// a newline we will need to add the newline and a null terminator
		if (out_idx >= size - 2) {
			if (discard_on_overflow) {
				// Consumer safely updates tail without caring if head changed concurrently
				memory_barrier();
				rb->tail = (pos + 1) % rb->size;
			}
			line[out_idx] = '\0';
			return RBGL_OVERFLOW;
		}
            
		// store the byte
		line[out_idx++] = (char) ch;
            
		// check for newline
		if (ch == '\n') {
			line[out_idx] = '\0';
			// Consumer safely updates tail without caring if head changed concurrently
			memory_barrier();
			rb->tail = (pos + 1) % rb->size;
			return RBGL_OK;
		}
    }

    return RBGL_NA;
}

