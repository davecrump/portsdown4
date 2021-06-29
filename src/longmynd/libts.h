/* -------------------------------------------------------------------------------------------------- */
/* The LongMynd receiver: ts.h                                                                      */
/* Copyright 2019 Heather Lomond                                                                      */
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

#ifndef LIBTS_H
#define LIBTS_H

#define TS_PACKET_SIZE 188
#define TS_HEADER_SYNC 0x47

#define TS_PID_PAT 0x0000
#define TS_PID_SDT 0x0011
#define TS_PID_NULL 0x1FFF

#define TS_TABLE_PAT 0x00
#define TS_TABLE_PMT 0x02
#define TS_TABLE_SDT 0x42

#define TS_MAX_PID  8192

void ts_parse(
    uint8_t *ts_buffer, uint32_t ts_buffer_length,
    void (*callback_sdt_service)(uint8_t *, uint32_t *, uint8_t *, uint32_t *),
    void (*callback_pmt_pids)(uint32_t *, uint32_t *, uint32_t *),
    void (*callback_ts_stats)(uint32_t *, uint32_t *),
    bool parse_verbose
);

/*
    Example Callbacks:

    static void ts_callback_sdt_service(
        uint8_t *service_provider_name_ptr, uint32_t *service_provider_name_length_ptr,
        uint8_t *service_name_ptr, uint32_t *service_name_length_ptr
    )
    {
        memcpy(ts_longmynd_status->service_name, service_name_ptr, *service_name_length_ptr);
        ts_longmynd_status->service_name[*service_name_length_ptr] = '\0';

        memcpy(ts_longmynd_status->service_provider_name, service_provider_name_ptr, *service_provider_name_length_ptr);
        ts_longmynd_status->service_provider_name[*service_provider_name_length_ptr] = '\0';
    }

    static void ts_callback_pmt_pids(uint32_t *ts_pmt_index_ptr, uint32_t *ts_pmt_es_pid, uint32_t *ts_pmt_es_type)
    {
        ts_longmynd_status->ts_elementary_streams[*ts_pmt_index_ptr][0] = *ts_pmt_es_pid;
        ts_longmynd_status->ts_elementary_streams[*ts_pmt_index_ptr][1] = *ts_pmt_es_type;
    }

    static void ts_callback_ts_stats(uint32_t *ts_packet_total_count_ptr, uint32_t *ts_null_percentage_ptr)
    {
        if(*ts_packet_total_count_ptr > 0)
        {
            ts_longmynd_status->ts_null_percentage = *ts_null_percentage_ptr;
        }
    }
*/

#endif

