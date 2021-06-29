/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: analyse_ts.c                                                                */
/* Copyright 2019 Heather Lomond                                                                      */
/* Copyright 2021 Phil Crump                                                                          */
/* -------------------------------------------------------------------------------------------------- */
/*
    This file is part of longmynd.

    Longmynd is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Longmynd is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with longmynd.  If not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>

#include "libts.h"

static void _print_usage(void)
{
	printf(
		"\n"
		"Usage: ts_analyse <filename>\n"
		"\n"
	);
}

static void ts_callback_sdt_service(
    uint8_t *service_provider_name_ptr, uint32_t *service_provider_name_length_ptr,
    uint8_t *service_name_ptr, uint32_t *service_name_length_ptr
)
{
    char *service_name = malloc(1 + (*service_name_length_ptr * sizeof(char)));
    char *service_provider = malloc(1 + (*service_provider_name_length_ptr * sizeof(char)));
                
    memcpy(service_name, service_name_ptr, *service_name_length_ptr);
    service_name[*service_name_length_ptr] = '\0';

    memcpy(service_provider, service_provider_name_ptr, *service_provider_name_length_ptr);
    service_provider[*service_provider_name_length_ptr] = '\0';

    printf(" * SDT: Service Name: %s, Service Provider: %s\n", service_name, service_provider);

    free(service_name);
    free(service_provider);
}

static void ts_callback_pmt_pids(uint32_t *ts_pmt_index_ptr, uint32_t *ts_pmt_es_pid, uint32_t *ts_pmt_es_type)
{
    printf(" * PMT: Index: %"PRIu32", PID: %"PRIu32", Type: %"PRIu32"\n", *ts_pmt_index_ptr, *ts_pmt_es_pid, *ts_pmt_es_type);
}

static uint32_t ts_packet_total_count = 0;

static void ts_callback_ts_stats(uint32_t *ts_packet_total_count_ptr, uint32_t *ts_null_percentage_ptr)
{
    (void)ts_null_percentage_ptr;

    ts_packet_total_count += *ts_packet_total_count_ptr;
}

int main(int argc, char **argv)
{
    FILE *ts_fd;
    uint32_t ts_length;

    if(argc != 2)
    {
        _print_usage();
        return -1;
    }

    ts_fd = fopen(argv[1], "rb");
    if(ts_fd == NULL)
    {
        fprintf(stderr, "Failed to open input file.\n");
        return(-1);
    }

    fseek(ts_fd, 0, SEEK_END);
    ts_length = ftell(ts_fd);
    fseek(ts_fd, 0, SEEK_SET);
    printf("TS File Size: %"PRIu32" Bytes\n", ts_length);

    uint8_t *ts_databuf = malloc(TS_PACKET_SIZE * sizeof(uint8_t));
    if(ts_databuf == NULL)
    {
        fprintf(stderr, "Failed to allocate databuffer\n");
        fclose(ts_fd);
        return(-1);
    }

    uint32_t fastforward_count = 0;
    while(fread(ts_databuf, sizeof(uint8_t), 1, ts_fd) == 1 && ts_databuf[0] != TS_HEADER_SYNC)
    {
        fastforward_count++;
    };
    fseek(ts_fd, -1, SEEK_CUR);
    if(fastforward_count > 0)
    {
        printf("Fastforward %"PRIu32" Bytes\n", fastforward_count);
    }

    uint32_t total_bytes_read = 0;
    while(fread(ts_databuf, sizeof(uint8_t), TS_PACKET_SIZE, ts_fd) == TS_PACKET_SIZE)
    {
        total_bytes_read += TS_PACKET_SIZE;

        if(ts_databuf[0] != TS_HEADER_SYNC)
        {
            fseek(ts_fd, -1 * TS_PACKET_SIZE, SEEK_CUR);
            fastforward_count = 0;
            while(fread(ts_databuf, sizeof(uint8_t), 1, ts_fd) == 1 && ts_databuf[0] != TS_HEADER_SYNC)
            {
                fastforward_count++;
            };
            fseek(ts_fd, -1, SEEK_CUR);
            printf("Fastforward %"PRIu32" Bytes\n", fastforward_count);
            continue;
        }

        ts_parse(
            ts_databuf, TS_PACKET_SIZE,
            &ts_callback_sdt_service,
            &ts_callback_pmt_pids,
            &ts_callback_ts_stats,
            true
        );
    }

    printf("Total bytes read: %"PRIu32"\n", total_bytes_read);
    printf("Total TS Packets parsed: %"PRIu32"\n", ts_packet_total_count);

    free(ts_databuf);

    fclose(ts_fd);

    return 0;
}
