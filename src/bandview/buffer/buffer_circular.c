#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>

#include <pthread.h>

#include "buffer_circular.h"

buffer_circular_t buffer_circular_iq_main;
buffer_circular_t buffer_circular_iq_if;
buffer_circular_t buffer_circular_audio;

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)

#define NEON_ALIGNMENT (4*4*2) // From libcsdr

bool buffer_circular_init(buffer_circular_t *buffer_ptr, uint32_t unit_size, uint32_t buffer_capacity)
{    
    pthread_mutex_init(&buffer_ptr->Mutex, NULL);
    pthread_cond_init(&buffer_ptr->Signal, NULL);
    
    pthread_mutex_lock(&buffer_ptr->Mutex);

    buffer_ptr->Head = 0;
    buffer_ptr->Tail = 0;

    /* Allocate extra memory and then align the pointer */
    buffer_ptr->Data = NULL;
    buffer_ptr->Data = malloc((buffer_capacity * unit_size) + (NEON_ALIGNMENT-1));
    buffer_ptr->Data = (void *)(((uintptr_t)buffer_ptr->Data + (NEON_ALIGNMENT-1)) & ~((uintptr_t)0x1F));

    buffer_ptr->Unitsize = unit_size;

    buffer_ptr->Capacity = buffer_capacity * unit_size;

    pthread_mutex_unlock(&buffer_ptr->Mutex);

    return (buffer_ptr->Data != NULL);
}

uint32_t buffer_circular_notEmpty(buffer_circular_t *buffer_ptr)
{
    uint32_t result;
    
    pthread_mutex_lock(&buffer_ptr->Mutex);
    result = (buffer_ptr->Head!=buffer_ptr->Tail);
    pthread_mutex_unlock(&buffer_ptr->Mutex);
    
    return result;
}

uint32_t buffer_circular_head(buffer_circular_t *buffer_ptr)
{    
    uint32_t result;
    
    pthread_mutex_lock(&buffer_ptr->Mutex);
    result = (buffer_ptr->Head / buffer_ptr->Unitsize);
    pthread_mutex_unlock(&buffer_ptr->Mutex);
    
    return result;
}

uint32_t buffer_circular_tail(buffer_circular_t *buffer_ptr)
{
    uint32_t result;
    
    pthread_mutex_lock(&buffer_ptr->Mutex);
    result = (buffer_ptr->Tail / buffer_ptr->Unitsize);
    pthread_mutex_unlock(&buffer_ptr->Mutex);
    
    return result;
}

void buffer_circular_stats(buffer_circular_t *buffer_ptr, uint32_t *head, uint32_t *tail, uint32_t *capacity, uint32_t *occupied)
{
    if(capacity != NULL)
    {
        *capacity = (buffer_ptr->Capacity / buffer_ptr->Unitsize);
    }

    pthread_mutex_lock(&buffer_ptr->Mutex);

    if(head != NULL)
    {
        *head = (buffer_ptr->Head / buffer_ptr->Unitsize);
    }

    if(tail != NULL)
    {
        *tail = (buffer_ptr->Tail / buffer_ptr->Unitsize);
    }

    if(occupied != NULL)
    {
        if(buffer_ptr->Head >= buffer_ptr->Tail)
        {
            *occupied = (buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize;
        }
        else
        {
            *occupied = (buffer_ptr->Head + (buffer_ptr->Capacity - buffer_ptr->Tail)) / buffer_ptr->Unitsize;
        }
    }
    
    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

void buffer_circular_flush(buffer_circular_t *buffer_ptr)
{
    pthread_mutex_lock(&buffer_ptr->Mutex);

    buffer_ptr->Tail = buffer_ptr->Head;

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

/* Lossy when buffer is full */
void buffer_circular_push(buffer_circular_t *buffer_ptr, void *data, uint32_t *length)
{
    uint32_t copy_length;

    pthread_mutex_lock(&buffer_ptr->Mutex);

    if(buffer_ptr->Head >= buffer_ptr->Tail)
    {
        if(*length > ((buffer_ptr->Capacity - buffer_ptr->Head) / buffer_ptr->Unitsize))
        {
            if(buffer_ptr->Tail == 0)
            {
                /* Cannot wrap around. */
                copy_length = buffer_ptr->Capacity - buffer_ptr->Head - 1;
                memcpy(&((uint8_t *)buffer_ptr->Data)[buffer_ptr->Head], data, copy_length);

                buffer_ptr->Head += copy_length;
                *length -= (copy_length / buffer_ptr->Unitsize);
            }
            else
            {
                /* Will need to wrap around to save data */
                copy_length = buffer_ptr->Capacity - buffer_ptr->Head;
                memcpy(&((uint8_t *)buffer_ptr->Data)[buffer_ptr->Head], data, copy_length);

                buffer_ptr->Head = 0;
                *length -= (copy_length / buffer_ptr->Unitsize);

                if(buffer_ptr->Tail > (1 * buffer_ptr->Unitsize))
                {
                    copy_length = MIN(buffer_ptr->Tail - (1 * buffer_ptr->Unitsize), (*length * buffer_ptr->Unitsize));

                    memcpy(&((uint8_t *)buffer_ptr->Data)[buffer_ptr->Head], data, copy_length);

                    buffer_ptr->Head += copy_length;

                    *length -= (copy_length / buffer_ptr->Unitsize);
                }
            }
        }
        else
        {
            memcpy(&((uint8_t *)buffer_ptr->Data)[buffer_ptr->Head], data, (*length * buffer_ptr->Unitsize));

            buffer_ptr->Head += (*length * buffer_ptr->Unitsize);
            if(buffer_ptr->Head == buffer_ptr->Capacity)
                buffer_ptr->Head = 0;

            *length  = 0;
        }

        pthread_cond_signal(&buffer_ptr->Signal);
    }
    else if(buffer_ptr->Head < (buffer_ptr->Tail - (1 * buffer_ptr->Unitsize)))
    {
        copy_length = MIN((buffer_ptr->Tail - (1 * buffer_ptr->Unitsize)) - buffer_ptr->Head, (*length * buffer_ptr->Unitsize));

        memcpy(&((uint8_t *)buffer_ptr->Data)[buffer_ptr->Head], data, copy_length);

        buffer_ptr->Head += copy_length;

        *length -= (copy_length / buffer_ptr->Unitsize);

        pthread_cond_signal(&buffer_ptr->Signal);
    }

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

void buffer_circular_pop(buffer_circular_t *buffer_ptr, uint32_t maxLength, void *data, uint32_t *length)
{
    *length = 0;

    pthread_mutex_lock(&buffer_ptr->Mutex);

    if(buffer_ptr->Head!=buffer_ptr->Tail)
    {
        /* Data in the buffer */
        if(buffer_ptr->Head > buffer_ptr->Tail)
        {
            /* Normal */
            *length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength);
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
        }
        else
        {
            /* Wraparound */
            if(maxLength > ((buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize))
            {
                /* Data requested is over wraparound boundary */
                uint32_t remaining_length;

                *length = (buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize;
                memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
                buffer_ptr->Tail = 0;

                remaining_length = MIN((buffer_ptr->Head / buffer_ptr->Unitsize), maxLength - *length);
                memcpy(&((uint8_t *)data)[*length], &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (remaining_length * buffer_ptr->Unitsize));
                buffer_ptr->Tail += (remaining_length * buffer_ptr->Unitsize);
                *length += remaining_length;
            }
            else
            {
                /* Data copy doesn't need to cross wraparound */
                /*  - implicitly maxLength < data_available */
                *length = maxLength;
                memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
                buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
            }

        }
    }

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

void buffer_circular_thresholdPop(buffer_circular_t *buffer_ptr, uint32_t minLength, uint32_t maxLength, void *data, uint32_t *length)
{
    uint32_t remaining_length;
    *length = 0;

    pthread_mutex_lock(&buffer_ptr->Mutex);
    
    if(buffer_ptr->Head == buffer_ptr->Tail
        || ((buffer_ptr->Head > buffer_ptr->Tail) && (((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize) < minLength))
        || ((buffer_ptr->Head < buffer_ptr->Tail) && (((buffer_ptr->Head + (buffer_ptr->Capacity - buffer_ptr->Tail)) / buffer_ptr->Unitsize) < minLength)))
    {
        pthread_mutex_unlock(&buffer_ptr->Mutex);
        return;
    }

    /* Data in the buffer */
    if(buffer_ptr->Head > buffer_ptr->Tail)
    {
        /* Normal */
        *length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength);
        memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
        buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
    }
    else
    {
        /* Wraparound */
        if(maxLength > ((buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize))
        {
            /* Data requested is over wraparound boundary */
            *length = (buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail = 0;

            remaining_length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength - *length);
            memcpy(&((uint8_t *)data)[*length], &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (remaining_length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (remaining_length * buffer_ptr->Unitsize);
            *length += remaining_length;
        }
        else
        {
            /* Data copy doesn't need to cross wraparound */
            /*  - implicitly maxLength < data_available */
            *length = maxLength;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
        }

    }

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

void buffer_circular_waitPop(buffer_circular_t *buffer_ptr, uint32_t maxLength, void *data, uint32_t *length)
{
    uint32_t remaining_length;
    *length = 0;

    pthread_mutex_lock(&buffer_ptr->Mutex);
    
    while(buffer_ptr->Head==buffer_ptr->Tail) /* If buffer is empty */
    {
        /* Mutex is atomically unlocked on beginning waiting for signal */
        pthread_cond_wait(&buffer_ptr->Signal, &buffer_ptr->Mutex);
        /* and locked again on resumption */
    }

    /* Data in the buffer */
    if(buffer_ptr->Head > buffer_ptr->Tail)
    {
        /* Normal */
        *length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength);
        memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
        buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
    }
    else
    {
        /* Wraparound */
        if(maxLength > ((buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize))
        {
            /* Data requested is over wraparound boundary */
            *length = (buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail = 0;

            remaining_length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength - *length);
            memcpy(&((uint8_t *)data)[*length], &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (remaining_length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (remaining_length * buffer_ptr->Unitsize);
            *length += remaining_length;
        }
        else
        {
            /* Data copy doesn't need to cross wraparound */
            /*  - implicitly maxLength < data_available */
            *length = maxLength;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
        }

    }

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}

void buffer_circular_waitThresholdPop(buffer_circular_t *buffer_ptr, uint32_t minLength, uint32_t maxLength, void *data, uint32_t *length)
{
    uint32_t remaining_length;
    *length = 0;

    pthread_mutex_lock(&buffer_ptr->Mutex);
    
    while(buffer_ptr->Head == buffer_ptr->Tail
        || ((buffer_ptr->Head > buffer_ptr->Tail) && (((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize) < minLength))
        || ((buffer_ptr->Head < buffer_ptr->Tail) && (((buffer_ptr->Head + (buffer_ptr->Capacity - buffer_ptr->Tail)) / buffer_ptr->Unitsize) < minLength)))
    {
        /* Mutex is atomically unlocked on beginning waiting for signal */
        pthread_cond_wait(&buffer_ptr->Signal, &buffer_ptr->Mutex);
        /* and locked again on resumption */
    }

    /* Data in the buffer */
    if(buffer_ptr->Head > buffer_ptr->Tail)
    {
        /* Normal */
        *length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength);
        memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
        buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
    }
    else
    {
        /* Wraparound */
        if(maxLength > ((buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize))
        {
            /* Data requested is over wraparound boundary */
            *length = (buffer_ptr->Capacity - buffer_ptr->Tail) / buffer_ptr->Unitsize;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail = 0;

            remaining_length = MIN((buffer_ptr->Head - buffer_ptr->Tail) / buffer_ptr->Unitsize, maxLength - *length);
            memcpy(&((uint8_t *)data)[*length], &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (remaining_length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (remaining_length * buffer_ptr->Unitsize);
            *length += remaining_length;
        }
        else
        {
            /* Data copy doesn't need to cross wraparound */
            /*  - implicitly maxLength < data_available */
            *length = maxLength;
            memcpy(data, &((uint8_t *)buffer_ptr->Data)[buffer_ptr->Tail], (*length * buffer_ptr->Unitsize));
            buffer_ptr->Tail += (*length * buffer_ptr->Unitsize);
        }
    }

    pthread_mutex_unlock(&buffer_ptr->Mutex);
}