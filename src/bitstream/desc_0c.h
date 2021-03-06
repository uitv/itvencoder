/*****************************************************************************
 * desc_0c.h: ISO/IEC 13818-1 Descriptor 0x0c (Multiplex buffer utilization)
 *****************************************************************************
 * Copyright (C) 2011 Unix Solutions Ltd.
 *
 * Authors: Georgi Chorbadzhiyski <georgi@unixsol.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *****************************************************************************/

/*
 * Normative references:
 *  - ISO/IEC 13818-1:2007(E) (MPEG-2 Systems)
 */

#ifndef __BITSTREAM_MPEG_DESC_0C_H__
#define __BITSTREAM_MPEG_DESC_0C_H__

#include "common.h"
#include "descriptors.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * Descriptor 0x0c: Multiplex buffer utilization descriptor
 *****************************************************************************/
#define DESC0C_HEADER_SIZE      (DESC_HEADER_SIZE + 4)

static inline void desc0c_init(uint8_t *p_desc)
{
    desc_set_tag(p_desc, 0x0c);
    desc_set_length(p_desc, DESC0C_HEADER_SIZE - DESC_HEADER_SIZE);
}

static inline bool desc0c_get_bound_valid(const uint8_t *p_desc)
{
    return ((p_desc[2] & 0x80) == 0x80);
}

static inline void desc0c_set_bound_valid(uint8_t *p_desc, bool bound_valid)
{
    p_desc[2] = bound_valid ? (p_desc[2] | 0x80) : (p_desc[2] &~ 0x80);
}

static inline uint16_t desc0c_get_LTW_offset_lower_bound(const uint8_t *p_desc)
{
    return ((p_desc[2] & 0x7f) << 8) | p_desc[3]; // 1xxxxxxx xxxxxxxx
}

static inline void desc0c_set_LTW_offset_lower_bound(uint8_t *p_desc, uint16_t LTW_offset_lower_bound)
{
    p_desc[2] = ((LTW_offset_lower_bound >> 8) & 0x7f) | (p_desc[2] & 0x80);
    p_desc[3] = LTW_offset_lower_bound & 0xff;
}

static inline uint16_t desc0c_get_LTW_offset_upper_bound(const uint8_t *p_desc)
{
    return ((p_desc[4] & 0x7f) << 8) | p_desc[5]; // 1xxxxxxx xxxxxxxx
}

static inline void desc0c_set_LTW_offset_upper_bound(uint8_t *p_desc, uint16_t LTW_offset_upper_bound)
{
    p_desc[4] = ((LTW_offset_upper_bound >> 8) & 0x7f) | 0x80;
    p_desc[5] = LTW_offset_upper_bound & 0xff;
}

static inline bool desc0c_validate(const uint8_t *p_desc)
{
    return desc_get_length(p_desc) >= DESC0C_HEADER_SIZE - DESC_HEADER_SIZE;
}

static inline void desc0c_print(const uint8_t *p_desc, f_print pf_print,
                                void *opaque, print_type_t i_print_type)
{
    switch (i_print_type) {
    case PRINT_XML:
        pf_print(opaque, "<MULTIPLEX_BUFFER_UTILIZATION_DESC bound_valid=\"%u\" LTW_offset_lower_bound=\"%u\" LTW_offset_upper_bound=\"%u\"/>",
                 desc0c_get_bound_valid(p_desc),
                 desc0c_get_LTW_offset_lower_bound(p_desc),
                 desc0c_get_LTW_offset_upper_bound(p_desc));
        break;
    default:
        pf_print(opaque, "    - desc 0c multiplex_buffer_utilization bound_valid=%u LTW_offset_lower_bound=%u LTW_offset_upper_bound=%u",
                 desc0c_get_bound_valid(p_desc),
                 desc0c_get_LTW_offset_lower_bound(p_desc),
                 desc0c_get_LTW_offset_upper_bound(p_desc));
    }
}

#ifdef __cplusplus
}
#endif

#endif
