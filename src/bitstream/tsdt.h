/*****************************************************************************
 * tsdt.h: ISO/IEC 13818-1 Transport stream descriptor table (TSDT)
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

#ifndef __BITSTREAM_MPEG_TSDT_H__
#define __BITSTREAM_MPEG_TSDT_H__

#include "common.h"
#include "psi.h"
#include "descriptors.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * Transport stream descriptor table
 *****************************************************************************/
#define TSDT_PID                 0x02
#define TSDT_TABLE_ID            0x03
#define TSDT_HEADER_SIZE         PSI_HEADER_SIZE_SYNTAX1

static inline void tsdt_init(uint8_t *p_tsdt)
{
    psi_init(p_tsdt, true);
    psi_set_tableid(p_tsdt, TSDT_TABLE_ID);
    p_tsdt[1] &= ~0x40;
    psi_set_tableidext(p_tsdt, 0xffff);
    psi_set_section(p_tsdt, 0);
    psi_set_lastsection(p_tsdt, 0);
}

static inline void tsdt_set_length(uint8_t *p_tsdt, uint16_t i_tsdt_length)
{
    psi_set_length(p_tsdt, TSDT_HEADER_SIZE + PSI_CRC_SIZE - PSI_HEADER_SIZE
                    + i_tsdt_length);
}

static inline uint16_t tsdt_get_desclength(const uint8_t *p_tsdt)
{
    return psi_get_length(p_tsdt) - (TSDT_HEADER_SIZE + PSI_CRC_SIZE - PSI_HEADER_SIZE);
}

static inline void tsdt_set_desclength(uint8_t *p_tsdt, uint16_t i_desc_len)
{
    tsdt_set_length(p_tsdt, i_desc_len);
}

static inline uint8_t *tsdt_get_descl(uint8_t *p_tsdt)
{
    return p_tsdt + TSDT_HEADER_SIZE;
}

static inline const uint8_t *tsdt_get_descl_const(const uint8_t *p_tsdt)
{
    return p_tsdt + TSDT_HEADER_SIZE;
}

static inline bool tsdt_validate(const uint8_t *p_tsdt)
{
    uint16_t i_section_size = psi_get_length(p_tsdt) + PSI_HEADER_SIZE
                               - PSI_CRC_SIZE;

    if (!psi_get_syntax(p_tsdt) || psi_get_section(p_tsdt)
         || psi_get_lastsection(p_tsdt)
         || psi_get_tableid(p_tsdt) != TSDT_TABLE_ID)
        return false;

    if (i_section_size < TSDT_HEADER_SIZE
         || i_section_size < TSDT_HEADER_SIZE + tsdt_get_desclength(p_tsdt))
        return false;

    if (!descl_validate(tsdt_get_descl_const(p_tsdt), tsdt_get_desclength(p_tsdt)))
        return false;

    return true;
}

static inline bool tsdt_table_validate(uint8_t **pp_sections)
{
    uint8_t i_last_section = psi_table_get_lastsection(pp_sections);
    uint8_t i;

    for (i = 0; i <= i_last_section; i++) {
        uint8_t *p_section = psi_table_get_section(pp_sections, i);

        if (!psi_check_crc(p_section))
            return false;
    }

    return true;
}

#ifdef __cplusplus
}
#endif

#endif
