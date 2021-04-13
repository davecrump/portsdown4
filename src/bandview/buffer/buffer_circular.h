#ifndef __BUFFER_CIRCULAR_H__
#define __BUFFER_CIRCULAR_H__

typedef struct {
    float i;
    float q;
} __attribute__((__packed__)) buffer_iqsample_t;

typedef struct
{
    /* Buffer Access Lock */
    pthread_mutex_t Mutex;
    /* New Data Signal */
    pthread_cond_t Signal;
    /* Head and Tail Indexes (in bytes, regardless of Unitsize) */
    uint32_t Head, Tail;
    /* Data */
    void *Data;
    /* Size of Data unit */
    uint32_t Unitsize;
    /* Buffer Capacity (in bytes, regardless of Unitsize) */
    uint32_t Capacity;
} buffer_circular_t;

/** Common functions **/
bool buffer_circular_init(buffer_circular_t *buffer_ptr, uint32_t unit_size, uint32_t buffer_capacity);

uint32_t buffer_circular_notEmpty(buffer_circular_t *buffer_ptr);
uint32_t buffer_circular_head(buffer_circular_t *buffer_ptr);
uint32_t buffer_circular_tail(buffer_circular_t *buffer_ptr);
void buffer_circular_stats(buffer_circular_t *buffer_ptr, uint32_t *head, uint32_t *tail, uint32_t *capacity, uint32_t *occupied);
void buffer_circular_flush(buffer_circular_t *buffer_ptr);

void buffer_circular_push(buffer_circular_t *buffer_ptr, void *data, uint32_t *length);
void buffer_circular_pop(buffer_circular_t *buffer_ptr, uint32_t maxLength, void *data, uint32_t *length);
void buffer_circular_thresholdPop(buffer_circular_t *buffer_ptr, uint32_t minLength, uint32_t maxLength, void *data, uint32_t *length);
void buffer_circular_waitPop(buffer_circular_t *buffer_ptr, uint32_t maxLength, void *data, uint32_t *length);
void buffer_circular_waitThresholdPop(buffer_circular_t *buffer_ptr, uint32_t minLength, uint32_t maxLength, void *data, uint32_t *length);

/** Declared Buffer Objects **/
buffer_circular_t buffer_circular_iq_main;
buffer_circular_t buffer_circular_iq_if;
buffer_circular_t buffer_circular_audio;

#endif /* __BUFFER_CIRCULAR_H__ */
