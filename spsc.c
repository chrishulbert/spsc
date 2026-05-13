#include <pthread.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <time.h>

// ------------------
// Benchmark tracker:
// ------------------
uint64_t itemsConsumed = 0;

// -----------
// SPSC Queue:
// -----------

// You may want to have a 'type' for your queue entries:
typedef enum {
    QUEUE_ENTRY_TYPE_PLAY,
    QUEUE_ENTRY_TYPE_STOP,
} QueueEntryType;

// Put whatever you want to have in your queue entries here:
typedef struct {
    QueueEntryType type;
} QueueEntry;

// If the queue size is a power of two eg 16,
// the '%' calculations later will be optimised to '& 0xF'.
#define QUEUE_SIZE 16

// This is the SPSC queue:
static struct {
    // Producer's index (writer thread, perhaps the game):
    atomic_size_t writeIndex;

    // Since the producer is the only thread that updates writeIndex,
    // it can use this 'mirror' as its source of truth,
    // thus no synchronisation is needed when reading.
    size_t writeIndexMirror; 

    // Consumer's index (reader thread, perhaps the audio thread).
    atomic_size_t readIndex;

    // Since the consumer is the only thread that updates readIndex,
    // it can use this 'mirror' as the source of truth,
    // thus no synchronisation is needed when reading.
    size_t readIndexMirror; 

    // In my benchmarking, it runs twice as fast when the indices are next
    // to each other in the struct, which surprised me, as i thought
    // they should be separated by the buffer so they could update
    // independently without affecting each other's cache lines.

    // The ring buffer.
    QueueEntry buffer[QUEUE_SIZE]; 
} queue;

// Write an item to the queue.
// To be called from the producer thread.
// Returns true on success.
bool queue_write(QueueEntry entry) {
    // Since only this thread updates the write index, use a non-atomic
    // mirror for a potentially-quicker read:
    size_t writeIndex = queue.writeIndexMirror;
    size_t nextWriteIndex = (writeIndex + 1) % QUEUE_SIZE;

    // Acquire the read index, so that any updates the reader made are visible.
    // Not that the reader makes any changes to the buffer, though.
    // Open to feedback here if this is necessary.
    size_t readIndex = atomic_load_explicit(
                            &queue.readIndex,
                            memory_order_acquire);

    // Check if queue is full.
    if (nextWriteIndex == readIndex) {
        return false; // Full!
    }

    // Store this entry in the queue.
    queue.buffer[writeIndex] = entry;
    
    // Use 'release' to ensure the entry in the buffer is visible
    // to the reader after it 'acquires' the write index.
    atomic_store_explicit(&queue.writeIndex, nextWriteIndex, memory_order_release);

    // Use this mirror as the source of truth, so it can be read next time
    // without needing synchronisation, since only this thread uses it.
    queue.writeIndexMirror = nextWriteIndex;

    return true; // Success, there was room!
}

// Read items from the queue, handling each one.
// To be called from the consumer thread.
void queue_read() {
    size_t readIndex = queue.readIndexMirror;

    // Acquire the write index from the writer thread.
    // Once acquired, any buffer updates made before updating the
    // write index will be visible to this thread.
    size_t writeIndex = atomic_load_explicit(
                            &queue.writeIndex, 
                            memory_order_acquire);

    // Loop through all entries:
    while (readIndex != writeIndex) {
        // Get this entry:
        QueueEntry* entry = &queue.buffer[readIndex];

        // Increment the read index, wrapping to 0 at the end of the queue buffer:
        readIndex = (readIndex + 1) % QUEUE_SIZE;
        
        // Deal with this entry:
        switch (entry->type) {
            case QUEUE_ENTRY_TYPE_PLAY:
                itemsConsumed++;
                break;

            case QUEUE_ENTRY_TYPE_STOP:
                // Do something.
                break;
        }
    }

    // Release the read index so the producer knows space has been cleared in
    // the buffer that it can use to store incoming entries:
    atomic_store_explicit(&queue.readIndex, readIndex, memory_order_release);

    queue.readIndexMirror = readIndex;
}

// -----------
// Benchmarks:
// -----------

#define REPETITIONS 1000000000

uint64_t getNanos() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void* producerThread(void* arg) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0); // Hint to run on a performance core.
    QueueEntry entry = {
        .type = QUEUE_ENTRY_TYPE_PLAY,
    };
    for (uint64_t i=0; i < REPETITIONS; i++) {
        if (!queue_write(entry)) {
            i--; // Retry.
        }
    }
    return NULL;
}

void* consumerThread(void* arg) {
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0); // Hint to run on a performance core.
    auto start = getNanos();
    while (itemsConsumed < REPETITIONS) {
        queue_read();
    }
    auto nanos = getNanos() - start;
    double milliseconds = ((double)nanos) / 1000000.0;
    printf("Speed (lower is better): %.3f nanos per entry\n", ((double)nanos) / ((double)REPETITIONS));
    printf("Throughput (higher is better): %.0f ops / millisecond\n", ((double)REPETITIONS) / milliseconds);
    return NULL;
}

int main() {
    pthread_t producer;
    pthread_t consumer;
    pthread_create(&producer, NULL, producerThread, NULL);
    pthread_create(&consumer, NULL, consumerThread, NULL);
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    return 0;
}

// 31.484 nanos per entry.
