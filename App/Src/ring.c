/** a simple ring buffer implementation where data can be inserted
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
 * @brief Intialise a ring buffer
 * 
 * Call this only once before any other operation on the ring buffer.
 * If the parameters are incorrect, the ring buffer will be initialised
 * with an empty buffer.
 * 
 * @param rb the buffer to initialise - silently ignore missing ring buffer
 * @param buf the storage to use - this must not be used by any other code
 * @param size the length of the storage, in bytes
 */
void RingBufferInit (RingBuffer_t *rb, uint8_t *buf, size_t size) {
	if (rb) {
	    if (size < 2 || size > UINT16_MAX) {
		    rb->buf = (uint8_t *) 0;
		    rb->size = 0;
	    } else {
			rb->buf = buf;
			rb->size = size;
		}
		rb->head = 0;
		rb->tail = 0;
		rb->overflow_count = 0;
	}
}

/**
 * @brief check whether a ring buffer is initialised
 * @return true if the ring buffer has been initialised
 */
bool RingBufferIsInit (RingBuffer_t *rb) {
	if (! rb || ! rb->buf)
		return false;
	return true;
}

/**
 * @brief Empty a ring buffer
 * @param rb the buffer to empty - silently ignore missing ring buffer
 */
void RingBufferClear (RingBuffer_t *rb) {
	if (RingBufferIsInit (rb)) {
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
	if (! RingBufferIsInit (rb)) return false;
		
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
	if (! RingBufferIsInit (rb)) return false;
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
	if (! RingBufferIsInit (rb)) return 0;

    // Snapshot head and tail atomically to prevent race conditions
    uint16_t snap_head = rb->head;
    uint16_t snap_tail = rb->tail;
	memory_barrier();

    if (snap_head >= snap_tail)
        return snap_head - snap_tail;

    return rb->size - snap_tail + snap_head;
}

/**
 * @brief get a newline terminated string from the ring buffer
 * 
 * The string is always null terminated, provided the ring buffer has
 * been initialised and the size of the supplied line buffer is at
 * least 1. If any data is available it will be returned. Data is 
 * never discarded. 
 * 
 * This function does not block - it returns whatever is available 
 * at the time of the call. More data may arrive after the call.
 * This can be retrieved using a while loop:
 * 
 *   buffer [0] = '\0';
 *   while (RingBufferGetLine(rb, 
 *                            buffer + strlen(buffer),
 *                            sizeof(buffer) - strlen(buffer)) == LINE_PARTIAL)
 *     ...
 * 
 * @param rb the ring buffer
 * @param line the line, including its terminating newline - on repeated
 *        calls (as described in the example above) line will only be
 *        filled with new data
 * @param size the size of the line which must be at least 2 bytes
 * @retval LINE_OK a line, including newline terminator, was retrieved
 *         LINE_PARTIAL data was retrieved but no newline was found
 *         LINE_OVERFLOW data was retrieved but there is not enough
 *                       space to store it all - as much data as
 *                       possible has been retrieved - more will
 *                       be available on a subsequent call - the caller
 *                       may choose to retrieve more data or abandon the
 *                       attempt and clear the buffer
 *         LINE_NONE there was not data available
 */
LineRetrievalType_t RingBufferGetLine (RingBuffer_t *rb, char *line, size_t size) {
	// Ensure line is returned terminated
	if (size > 0)
		line[0] = '\0';

	// Check parameters
	if (! line || ! RingBufferIsInit (rb)) return LINE_NONE;
    if (size <= 1) return LINE_OVERFLOW;

    // Snapshot head and tail atomically to prevent race conditions
    uint16_t snap_head = rb->head;
    uint16_t snap_tail = rb->tail;
	memory_barrier();

    // Calculate available bytes based on snapshot
    uint16_t available = (snap_head >= snap_tail) 
                         ? (snap_head - snap_tail) 
                         : (rb->size - snap_tail + snap_head);

	// Use a separate index for the output data
    size_t out_idx = 0;

	// Get whatever is available...
	bool overflow = false;
	bool newline_found = false;
	uint16_t pos = snap_tail;
    for (uint16_t count = 0; 
                  count < available && ! overflow && ! newline_found; 
                  count++) {
		// Check for overflow
		if (out_idx >= size - 1) {
			overflow = true;
		} else {
			// Get the byte
			pos = (snap_tail + count) % rb->size;
			uint8_t ch = *(rb->buf + pos);

			// Store the byte
			line[out_idx++] = (char) ch;
            
			// Check for newline
			if (ch == '\n') 
				newline_found = true;
		}
    }

	// Finalise the returned line
	line[out_idx] = '\0';
	
	// Update the ring buffer
	if (available > 0) {
		// Consumer safely updates tail without caring if head changed concurrently
		memory_barrier();
		rb->tail = (pos + 1) % rb->size;
	}

	// work out what to return
	if (available == 0) return LINE_NONE;
	if (overflow) return LINE_OVERFLOW;
	if (newline_found) return LINE_OK;
	return LINE_PARTIAL;
}

