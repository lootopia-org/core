# Lootopia Core - Message Queue Architecture

## Overview

This project implements a WebSocket server that bridges between WebSocket clients and Apache Kafka, with a carefully designed threading architecture that uses dedicated message queues for both the Kafka consumer and producer.

## Architecture Diagram

```
┌─────────────────┐         ┌──────────────────┐         ┌─────────────┐
│  Kafka Topic    │────────▶│ Consumer Thread  │────────▶│  Consumer   │
│  (Input)        │         │                  │         │   Queue     │
└─────────────────┘         └──────────────────┘         └──────┬──────┘
                                                                 │
                                                                 ▼
                            ┌──────────────────────────────────────┐
                            │     WebSocket Server (Main Thread)   │
                            │  - Broadcasts to WS clients          │
                            │  - Receives from WS clients          │
                            └──────────────────────────────────────┘
                                                                 │
┌─────────────────┐         ┌──────────────────┐         ┌──────▼──────┐
│  Kafka Topic    │◀────────│ Producer Thread  │◀────────│  Producer   │
│  (Output)       │         │                  │         │   Queue     │
└─────────────────┘         └──────────────────┘         └─────────────┘
```

## Why Separate Message Queues?

### The Problem with Direct Kafka Integration

While Kafka itself has internal message queues and buffering, **relying solely on Kafka's internal mechanisms is insufficient** for this use case. Here's why:

#### 1. **Thread Safety and Blocking Operations**

Even though librdkafka is thread-safe and has internal queues, **every interaction with the Kafka client requires thread synchronization**:

- `rd_kafka_produce()` must acquire locks to enqueue messages
- `rd_kafka_poll()` must be called regularly to handle callbacks
- Network I/O can cause unpredictable delays

**In our WebSocket server**, the main event loop must be highly responsive:
```c
// Without message queue - BLOCKS on Kafka operations
while (*running) {
    // WebSocket event handling
    lws_service(context, timeout);
    
    // This can BLOCK waiting for Kafka! ❌
    kafka_producer_send(producer, data, len);
}
```

With our message queue approach:
```c
// With message queue - NON-BLOCKING ✓
while (*running) {
    // WebSocket event handling
    lws_service(context, timeout);
    
    // Push to queue - immediate return ✓
    message_queue_push(producer_queue, data, len);
}
```

#### 2. **Decoupling Application Threads from Kafka Threads**

Kafka's internal threading model is optimized for batching and network efficiency, **not for application responsiveness**:

- **Kafka's internal queue**: Optimized for throughput, batching, and network transmission
- **Our application queue**: Optimized for low-latency handoff between application components

**Key Difference**: Our message queue provides a clean boundary between the WebSocket event loop and Kafka operations. The WebSocket server can push/pop messages in microseconds, while the Kafka thread handles the complexity of network I/O, retries, and callbacks independently.

#### 3. **Graceful Degradation**

If Kafka becomes temporarily unavailable:

**Without message queues**:
```c
// WebSocket callback becomes blocking/failing
case LWS_CALLBACK_RECEIVE:
    // If Kafka is down, this might block or fail repeatedly
    // affecting ALL WebSocket operations ❌
    kafka_producer_send(producer, data, len);
    break;
```

**With message queues**:
```c
// WebSocket continues operating normally
case LWS_CALLBACK_RECEIVE:
    // Always succeeds (until queue is full)
    // WebSocket server remains responsive ✓
    message_queue_push(producer_queue, data, len);
    break;
    
// Separate producer thread handles Kafka issues independently
// without affecting WebSocket performance ✓
```

The message queue acts as a **buffer** that absorbs temporary Kafka outages or slowdowns without affecting the WebSocket server's ability to receive and broadcast messages.

#### 4. **Resource Management and Backpressure**

Our message queues provide **explicit capacity limits** with clear semantics:

```c
message_queue_t *queue = message_queue_create(capacity);

// When queue is full, we can:
// 1. Drop messages with logging
// 2. Apply backpressure to clients
// 3. Shed load gracefully
if (!message_queue_push(queue, data, len)) {
    LOG_WARN("Queue full - dropping message");
    // Application-level decision on how to handle this
}
```

Kafka's internal queue limits are harder to reason about and can cause:
- Blocking behavior when full
- Less predictable resource usage
- Harder to monitor and debug

#### 5. **Consumer Side: Kafka Poll Isolation**

The consumer side benefits are even more critical:

```c
// Consumer thread - dedicated to Kafka polling
while (*running) {
    rkmessage = rd_kafka_consumer_poll(rk, timeout);
    
    // Process message and push to queue
    message_queue_push(queue, payload, len);
}

// WebSocket thread - dedicated to client I/O
while (*running) {
    // Non-blocking pop from queue
    while (message_queue_try_pop(queue, &payload, &len)) {
        broadcast_to_clients(payload, len);
        free(payload);
    }
    
    lws_service(context, timeout);
}
```

**Without this separation**, the WebSocket thread would need to:
- Call `rd_kafka_consumer_poll()` repeatedly
- Handle partition rebalancing callbacks
- Manage Kafka connection state
- All while maintaining low-latency WebSocket responses

This creates a **tangled mess of responsibilities** in a single thread.

## Benefits of This Architecture

### ✅ **Separation of Concerns**
- Kafka threads: Handle Kafka protocol, network I/O, retries
- WebSocket thread: Handle WebSocket protocol, client management, broadcasting
- Each thread does ONE thing well

### ✅ **Non-Blocking Operations**
- WebSocket event loop never blocks on Kafka operations
- Kafka operations never interfere with WebSocket responsiveness
- Message queues provide O(1) push/pop operations

### ✅ **Resilience**
- Temporary Kafka outages don't affect WebSocket operations
- Message queues buffer transient spikes in traffic
- Clear failure modes and logging at queue boundaries

### ✅ **Observability**
- Queue depth indicates system health
- Easy to monitor: "Is the producer keeping up?"
- Clear metrics: messages queued, messages dropped, queue utilization

### ✅ **Resource Control**
- Explicit memory limits via queue capacity
- Bounded resource usage even under load
- Can prioritize or shed load at application level

### ✅ **Testability**
- Can test WebSocket logic without Kafka
- Can test Kafka logic without WebSocket
- Can inject messages into queues for testing

## Why Kafka's Internal Queue Isn't Enough

Kafka's internal buffering is designed for:
- **Network efficiency** (batching, compression)
- **Durability** (replication, acks)
- **Ordering guarantees** (partition ordering)

Our message queues are designed for:
- **Thread isolation** (clean handoff between components)
- **Application responsiveness** (non-blocking operations)
- **Operational flexibility** (explicit capacity, monitoring)

**They solve different problems at different layers of the system.**

## Implementation Details

### Message Queue Properties
- **Thread-safe**: Multiple threads can safely push/pop
- **Bounded**: Fixed capacity prevents unbounded memory growth
- **Non-blocking**: `try_pop()` returns immediately if empty
- **Blocking**: `pop()` waits for messages (used in dedicated threads)

### Thread Model
1. **Consumer Thread**: Dedicated to `rd_kafka_consumer_poll()`
2. **Producer Thread**: Dedicated to `rd_kafka_produce()` + `rd_kafka_poll()`
3. **WebSocket Thread**: Dedicated to `lws_service()` and client management

Each thread has a single responsibility and communicates via message queues.

## Conclusion

This architecture provides **clear separation between I/O domains** (Kafka network I/O vs. WebSocket network I/O) while maintaining high throughput and low latency. The message queues act as **shock absorbers** that prevent problems in one domain from cascading to another, while providing clear interfaces for monitoring, testing, and operations.

The key insight: **Kafka's queues optimize for network efficiency; our queues optimize for application architecture.**
