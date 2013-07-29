/*****************************************************************************
 * dvb_print_si.c: Prints tables from a TS file
 *****************************************************************************
 * Copyright (C) 2010-2011 VideoLAN
 *
 * Authors: Christophe Massiot <massiot@via.ecp.fr>
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

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <getopt.h>
#include <iconv.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "ts.h"
#include "psi.h"
#include "si.h"
#include "si_print.h"
#include "psi_print.h"

/*****************************************************************************
 * Local declarations
 *****************************************************************************/
#define MAX_PIDS    8192
#define READ_ONCE   7

static int got_pmt = 0;

typedef struct ts_pid_t {
    int i_psi_refcount;
    int8_t i_last_cc;

    /* biTStream PSI section gathering */
    uint8_t *p_psi_buffer;
    uint16_t i_psi_buffer_used;
} ts_pid_t;

static ts_pid_t p_pids[MAX_PIDS];

typedef struct sid_t {
    uint16_t i_sid, i_pmt_pid;
    uint8_t *p_current_pmt;
    PSI_TABLE_DECLARE(pp_eit_sections);
} sid_t;

static sid_t **pp_sids = NULL;
static int i_nb_sids = 0;

typedef struct bouquet_t {
    uint16_t i_bouquet;
    PSI_TABLE_DECLARE(pp_current_bat_sections);
    PSI_TABLE_DECLARE(pp_next_bat_sections);
} bouquet_t;

static bouquet_t **pp_bouquets = NULL;
static int i_nb_bouquets = 0;

static PSI_TABLE_DECLARE(pp_current_pat_sections);
static PSI_TABLE_DECLARE(pp_next_pat_sections);
static PSI_TABLE_DECLARE(pp_current_tsdt_sections);
static PSI_TABLE_DECLARE(pp_next_tsdt_sections);
static PSI_TABLE_DECLARE(pp_current_nit_sections);
static PSI_TABLE_DECLARE(pp_next_nit_sections);
static PSI_TABLE_DECLARE(pp_current_sdt_sections);
static PSI_TABLE_DECLARE(pp_next_sdt_sections);

static const char *psz_native_encoding = "UTF-8";
static const char *psz_current_encoding = "";
static iconv_t iconv_handle = (iconv_t)-1;
static print_type_t i_print_type = PRINT_XML;

/* Please keep those two in sync */
enum tables_t {
    TABLE_PAT = 0,
    TABLE_TSDT,
    TABLE_NIT,
    TABLE_BAT,
    TABLE_SDT,
    TABLE_PMT,
    TABLE_END
};
static const char * const ppsz_all_tables[TABLE_END] = {
    "pat", "tsdt", "nit", "bat", "sdt", "pmt"
};
static bool pb_print_table[TABLE_END];

/*****************************************************************************
 * print_wrapper
 *****************************************************************************/
static void print_wrapper(void *_unused, const char *psz_format, ...)
{
    char psz_fmt[strlen(psz_format) + 2];
    va_list args;
    va_start(args, psz_format);
    strcpy(psz_fmt, psz_format);
    strcat(psz_fmt, "\n");
    vprintf(psz_fmt, args);
}

/*****************************************************************************
 * iconv_wrapper
 *****************************************************************************/
static char *iconv_append_null(const char *p_string, size_t i_length)
{
    char *psz_string = malloc(i_length + 1);
    memcpy(psz_string, p_string, i_length);
    psz_string[i_length] = '\0';
    return psz_string;
}

static char *iconv_wrapper(void *_unused, const char *psz_encoding,
                           char *p_string, size_t i_length)
{
    char *psz_string, *p;
    size_t i_out_length;

    if (!strcmp(psz_encoding, psz_native_encoding))
        return iconv_append_null(p_string, i_length);

    if (iconv_handle != (iconv_t)-1 &&
        strcmp(psz_encoding, psz_current_encoding)) {
        iconv_close(iconv_handle);
        iconv_handle = (iconv_t)-1;
    }

    if (iconv_handle == (iconv_t)-1)
        iconv_handle = iconv_open(psz_native_encoding, psz_encoding);
    if (iconv_handle == (iconv_t)-1) {
        fprintf(stderr, "couldn't convert from %s to %s (%m)\n", psz_encoding,
                psz_native_encoding);
        return iconv_append_null(p_string, i_length);
    }

    /* converted strings can be up to six times larger */
    i_out_length = i_length * 6;
    p = psz_string = malloc(i_out_length);
    if (iconv(iconv_handle, &p_string, &i_length, &p, &i_out_length) == -1) {
        fprintf(stderr, "couldn't convert from %s to %s (%m)\n", psz_encoding,
                psz_native_encoding);
        free(psz_string);
        return iconv_append_null(p_string, i_length);
    }
    if (i_length)
        fprintf(stderr, "partial conversion from %s to %s\n", psz_encoding,
                psz_native_encoding);

    *p = '\0';
    return psz_string;
}

/*****************************************************************************
 * handle_pat
 *****************************************************************************/
static void handle_pat(void)
{
    PSI_TABLE_DECLARE(pp_old_pat_sections);
    uint8_t i_last_section = psi_table_get_lastsection(pp_next_pat_sections);
    uint8_t i;

    if (psi_table_validate(pp_current_pat_sections) &&
        psi_table_compare(pp_current_pat_sections, pp_next_pat_sections)) {
        /* Identical PAT. Shortcut. */
        psi_table_free(pp_next_pat_sections);
        psi_table_init(pp_next_pat_sections);
        return;
    }

    if (!pat_table_validate(pp_next_pat_sections)) {
            printf("<ERROR type=\"invalid_pat\"/>\n");
        psi_table_free(pp_next_pat_sections);
        psi_table_init(pp_next_pat_sections);
        return;
    }

    /* Switch tables. */
    psi_table_copy(pp_old_pat_sections, pp_current_pat_sections);
    psi_table_copy(pp_current_pat_sections, pp_next_pat_sections);
    psi_table_init(pp_next_pat_sections);

    for (i = 0; i <= i_last_section; i++) {
        uint8_t *p_section = psi_table_get_section(pp_current_pat_sections, i);
        const uint8_t *p_program;
        int j = 0;

        while ((p_program = pat_get_program(p_section, j)) != NULL) {
            const uint8_t *p_old_program = NULL;
            uint16_t i_sid = patn_get_program(p_program);
            uint16_t i_pid = patn_get_pid(p_program);
            j++;

            if (i_sid == 0) {
                if (i_pid != NIT_PID)
                    fprintf(stderr,
                        "NIT is carried on PID %hu which isn't DVB compliant\n",
                        i_pid);
                continue; /* NIT */
            }

            if (!psi_table_validate(pp_old_pat_sections)
                  || (p_old_program =
                      pat_table_find_program(pp_old_pat_sections, i_sid))
                       == NULL
                  || patn_get_pid(p_old_program) != i_pid) {
                sid_t *p_sid;
                int i_pmt;
                if (p_old_program != NULL)
                    p_pids[patn_get_pid(p_old_program)].i_psi_refcount--;
                p_pids[i_pid].i_psi_refcount++;

                for (i_pmt = 0; i_pmt < i_nb_sids; i_pmt++)
                    if (pp_sids[i_pmt]->i_sid == i_sid ||
                        pp_sids[i_pmt]->i_sid == 0)
                        break;

                if (i_pmt == i_nb_sids) {
                    p_sid = malloc(sizeof(sid_t));
                    pp_sids = realloc(pp_sids, ++i_nb_sids * sizeof(sid_t *));
                    pp_sids[i_pmt] = p_sid;
                    p_sid->p_current_pmt = NULL;
                    psi_table_init(p_sid->pp_eit_sections);
                }
                else
                    p_sid = pp_sids[i_pmt];

                p_sid->i_sid = i_sid;
                p_sid->i_pmt_pid = i_pid;
            }
        }
    }

    if (psi_table_validate(pp_old_pat_sections)) {
        i_last_section = psi_table_get_lastsection( pp_old_pat_sections );
        for (i = 0; i <= i_last_section; i++) {
            uint8_t *p_section = psi_table_get_section(pp_old_pat_sections, i);
            const uint8_t *p_program;
            int j = 0;

            while ((p_program = pat_get_program(p_section, j)) != NULL) {
                uint16_t i_sid = patn_get_program(p_program);
                j++;

                if (i_sid == 0)
                    continue; /* NIT */

                if (pat_table_find_program(pp_current_pat_sections, i_sid)
                      == NULL) {
                    int i_pmt;
                    for (i_pmt = 0; i_pmt < i_nb_sids; i_pmt++)
                        if (pp_sids[i_pmt]->i_sid == i_sid) {
                            pp_sids[i_pmt]->i_sid = 0;
                            free(pp_sids[i_pmt]->p_current_pmt);
                            pp_sids[i_pmt]->p_current_pmt = NULL;
                            psi_table_free(pp_sids[i]->pp_eit_sections);
                            psi_table_init(pp_sids[i]->pp_eit_sections);
                            break;
                        }
                }
            }
        }

        psi_table_free(pp_old_pat_sections);
    }

    if (pb_print_table[TABLE_PAT])
        pat_table_print(pp_current_pat_sections, print_wrapper, NULL, i_print_type);
}

static void handle_pat_section(uint16_t i_pid, uint8_t *p_section)
{
    if (i_pid != PAT_PID || !pat_validate(p_section)) {
            printf("<ERROR type=\"invalid_pat_section\"/>\n");
        free(p_section);
        return;
    }

    if (!psi_table_section(pp_next_pat_sections, p_section))
        return;

    handle_pat();
}

/*****************************************************************************
 * handle_tsdt
 *****************************************************************************/
static void handle_tsdt(void)
{
    PSI_TABLE_DECLARE(pp_old_tsdt_sections);

    if (psi_table_validate(pp_current_tsdt_sections) &&
        psi_table_compare(pp_current_tsdt_sections, pp_next_tsdt_sections)) {
        /* Identical TSDT. Shortcut. */
        psi_table_free(pp_next_tsdt_sections);
        psi_table_init(pp_next_tsdt_sections);
        return;
    }

    if (!tsdt_table_validate(pp_next_tsdt_sections)) {
            printf("<ERROR type=\"invalid_tsdt\"/>\n");
        psi_table_free(pp_next_tsdt_sections);
        psi_table_init(pp_next_tsdt_sections);
        return;
    }

    /* Switch tables. */
    psi_table_copy(pp_old_tsdt_sections, pp_current_tsdt_sections);
    psi_table_copy(pp_current_tsdt_sections, pp_next_tsdt_sections);
    psi_table_init(pp_next_tsdt_sections);

    if (psi_table_validate(pp_old_tsdt_sections))
        psi_table_free(pp_old_tsdt_sections);

    if (pb_print_table[TABLE_TSDT])
        tsdt_table_print(pp_current_tsdt_sections, print_wrapper, NULL,
                         i_print_type);
}

static void handle_tsdt_section(uint16_t i_pid, uint8_t *p_section)
{
    if (i_pid != TSDT_PID || !tsdt_validate(p_section)) {
            printf("<ERROR type=\"invalid_tsdt_section\"/>\n");
        free(p_section);
        return;
    }

    if (!psi_table_section(pp_next_tsdt_sections, p_section))
        return;

    handle_tsdt();
}

/*****************************************************************************
 * handle_pmt
 *****************************************************************************/
static void handle_pmt(uint16_t i_pid, uint8_t *p_pmt)
{
    uint16_t i_sid = pmt_get_program(p_pmt);
    sid_t *p_sid;
    int i;

    /* we do this before checking the service ID */
    if (!pmt_validate(p_pmt)) {
            printf("<ERROR type=\"invalid_pmt_section\" pid=\"%hu\"/>\n",
                   i_pid);
        free(p_pmt);
        return;
    }

    for (i = 0; i < i_nb_sids; i++)
        if (pp_sids[i]->i_sid && pp_sids[i]->i_sid == i_sid)
            break;

    if (i == i_nb_sids) {
            printf("<ERROR type=\"ghost_pmt\" program=\"%hu\" pid=\"%hu\"/>\n",
                   i_sid, i_pid);
        p_sid = malloc(sizeof(sid_t));
        pp_sids = realloc(pp_sids, ++i_nb_sids * sizeof(sid_t *));
        pp_sids[i] = p_sid;
        p_sid->i_sid = i_sid;
        p_sid->i_pmt_pid = i_pid;
        p_sid->p_current_pmt = NULL;
        psi_table_init(p_sid->pp_eit_sections);
    } else {
        p_sid = pp_sids[i];
        if (i_pid != p_sid->i_pmt_pid) {
                printf("<ERROR type=\"ghost_pmt\" program=\"%hu\" pid=\"%hu\"/>\n",
                       i_sid, i_pid);
        }
    }

    if (p_sid->p_current_pmt != NULL &&
        psi_compare(p_sid->p_current_pmt, p_pmt)) {
        /* Identical PMT. Shortcut. */
        free(p_pmt);
        return;
    }

    free(p_sid->p_current_pmt);
    p_sid->p_current_pmt = p_pmt;

    if (pb_print_table[TABLE_PMT])
        pmt_print(p_pmt, print_wrapper, NULL, iconv_wrapper, NULL,
                  i_print_type);

    got_pmt = 1;
}

/*****************************************************************************
 * handle_nit
 *****************************************************************************/
static void handle_nit(void)
{
    if (psi_table_validate(pp_current_nit_sections) &&
        psi_table_compare(pp_current_nit_sections, pp_next_nit_sections)) {
        /* Same version NIT. Shortcut. */
        psi_table_free(pp_next_nit_sections);
        psi_table_init(pp_next_nit_sections);
        return;
    }

    if (!nit_table_validate(pp_next_nit_sections)) {
            printf("<ERROR type=\"invalid_nit\"/>\n");
        psi_table_free(pp_next_nit_sections);
        psi_table_init(pp_next_nit_sections);
        return;
    }

    /* Switch tables. */
    psi_table_free(pp_current_nit_sections);
    psi_table_copy(pp_current_nit_sections, pp_next_nit_sections);
    psi_table_init(pp_next_nit_sections);

    if (pb_print_table[TABLE_NIT])
        nit_table_print(pp_current_nit_sections, print_wrapper, NULL,
                        iconv_wrapper, NULL, i_print_type);
}

static void handle_nit_section(uint16_t i_pid, uint8_t *p_section)
{
    if (i_pid != NIT_PID || !nit_validate(p_section)) {
            printf("<ERROR type=\"invalid_nit_section\" pid=\"%hu\"/>\n",
                   i_pid);
        free(p_section);
        return;
    }

    if (!psi_table_section(pp_next_nit_sections, p_section))
        return;

    handle_nit();
}

/*****************************************************************************
 * handle_sdt
 *****************************************************************************/
static void handle_sdt(void)
{
    if (psi_table_validate(pp_current_sdt_sections) &&
        psi_table_compare(pp_current_sdt_sections, pp_next_sdt_sections)) {
        /* Identical SDT. Shortcut. */
        psi_table_free(pp_next_sdt_sections);
        psi_table_init(pp_next_sdt_sections);
        return;
    }

    if (!sdt_table_validate(pp_next_sdt_sections)) {
            printf("<ERROR type=\"invalid_sdt\"/>\n");
        psi_table_free(pp_next_sdt_sections);
        psi_table_init(pp_next_sdt_sections);
        return;
    }

    /* Switch tables. */
    psi_table_free(pp_current_sdt_sections);
    psi_table_copy(pp_current_sdt_sections, pp_next_sdt_sections);
    psi_table_init(pp_next_sdt_sections);

    if (pb_print_table[TABLE_SDT])
        sdt_table_print(pp_current_sdt_sections, print_wrapper, NULL,
                        iconv_wrapper, NULL, i_print_type);
}

static void handle_sdt_section(uint16_t i_pid, uint8_t *p_section)
{
    if (i_pid != SDT_PID || !sdt_validate(p_section)) {
            printf("<ERROR type=\"invalid_sdt_section\" pid=\"%hu\"/>\n",
                   i_pid);
        free(p_section);
        return;
    }

    if (!psi_table_section(pp_next_sdt_sections, p_section))
        return;

    handle_sdt();
}
/*****************************************************************************
 * handle_section
 *****************************************************************************/
static void handle_section(uint16_t i_pid, uint8_t *p_section)
{
    uint8_t i_table_id = psi_get_tableid(p_section);

    if (!psi_validate(p_section)) {
        printf("<ERROR type=\"invalid_section\" pid=\"%hu\"/>\n", i_pid);
        free(p_section);
        return;
    }

    switch (i_table_id) {
    case PAT_TABLE_ID:
        handle_pat_section(i_pid, p_section);
        break;

    case TSDT_TABLE_ID:
        handle_tsdt_section(i_pid, p_section);
        break;

    case PMT_TABLE_ID:
        handle_pmt(i_pid, p_section);
        break;

    case NIT_TABLE_ID_ACTUAL:
        handle_nit_section(i_pid, p_section);
        break;

    case SDT_TABLE_ID_ACTUAL:
        handle_sdt_section(i_pid, p_section);
        break;

    default:
        free( p_section );
        break;
    }
}

/*****************************************************************************
 * handle_psi_packet
 *****************************************************************************/
static void handle_psi_packet(uint8_t *p_ts)
{
    uint16_t i_pid = ts_get_pid(p_ts);
    ts_pid_t *p_pid = &p_pids[i_pid];
    uint8_t i_cc = ts_get_cc(p_ts);
    const uint8_t *p_payload;
    uint8_t i_length;

    if (ts_check_duplicate(i_cc, p_pid->i_last_cc) || !ts_has_payload(p_ts))
        return;

    if (p_pid->i_last_cc != -1
          && ts_check_discontinuity(i_cc, p_pid->i_last_cc))
        psi_assemble_reset(&p_pid->p_psi_buffer, &p_pid->i_psi_buffer_used);

    p_payload = ts_section(p_ts);
    i_length = p_ts + TS_SIZE - p_payload;

    if (!psi_assemble_empty(&p_pid->p_psi_buffer, &p_pid->i_psi_buffer_used)) {
        uint8_t *p_section = psi_assemble_payload(&p_pid->p_psi_buffer,
                                                  &p_pid->i_psi_buffer_used,
                                                  &p_payload, &i_length);
        if (p_section != NULL)
            handle_section(i_pid, p_section);
    }

    p_payload = ts_next_section( p_ts );
    i_length = p_ts + TS_SIZE - p_payload;

    while (i_length) {
        uint8_t *p_section = psi_assemble_payload(&p_pid->p_psi_buffer,
                                                  &p_pid->i_psi_buffer_used,
                                                  &p_payload, &i_length);
        if (p_section != NULL)
            handle_section(i_pid, p_section);
    }
}

int mediainfo (char *uri)
{
    int i;
    struct sockaddr_in si_other;
    int s, slen = sizeof (si_other);
    char *server;
    int port;

    if (strncmp (uri, "udp://", 6) != 0) {
        exit (1);
    }
    server = uri + 6;
    for (i = 6; i < strlen (uri); i++) {
        if (*(uri + i) == ':') {
            *(uri + i) = '\0';
        }
        port = atoi (uri + i + 1);
    }
    if (i == strlen (uri)) {
        exit (1);
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    memset(p_pids, 0, sizeof(p_pids));

    if ((s = socket (AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        exit (1);
    }
    memset ((char *)&si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons (port);
    if (inet_aton (server, &si_other.sin_addr) == 0) {
        exit (1);
    }
    if (bind (s, (struct sockaddr *)&si_other, sizeof (si_other)) == -1) {
        exit (1);
    }

    for (i = 0; i < 8192; i++) {
        p_pids[i].i_last_cc = -1;
        psi_assemble_init(&p_pids[i].p_psi_buffer,
                          &p_pids[i].i_psi_buffer_used);
    }

    p_pids[PAT_PID].i_psi_refcount++;
    p_pids[TSDT_PID].i_psi_refcount++;
    p_pids[NIT_PID].i_psi_refcount++;
    p_pids[SDT_PID].i_psi_refcount++;

    psi_table_init(pp_current_pat_sections);
    psi_table_init(pp_next_pat_sections);
    psi_table_init(pp_current_tsdt_sections);
    psi_table_init(pp_next_tsdt_sections);
    psi_table_init(pp_current_nit_sections);
    psi_table_init(pp_next_nit_sections);
    psi_table_init(pp_current_sdt_sections);
    psi_table_init(pp_next_sdt_sections);
    for (i = 0; i < TABLE_END; i++)
        pb_print_table[i] = true;

    printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
    printf("<TS>\n");

    while (got_pmt == 0) {
        uint8_t p_ts[TS_SIZE*10];
        uint8_t *p;
        size_t i_ret = recvfrom (s, p_ts, sizeof (p_ts), MSG_CMSG_CLOEXEC, (struct sockaddr *)&si_other, &slen);
        if (i_ret == -1) {
            break;
        }

        p = p_ts;
        while (p < p_ts + i_ret) {
            if (!ts_validate(p)) {
                printf("<ERROR type=\"invalid_ts\"/>\n");
            } else {
                uint16_t i_pid = ts_get_pid(p);
                ts_pid_t *p_pid = &p_pids[i_pid];
                if (p_pid->i_psi_refcount)
                    handle_psi_packet(p);
                p_pid->i_last_cc = ts_get_cc(p);
            }
            p = p + TS_SIZE;
        }
    }

    printf("</TS>\n");

    if (iconv_handle != (iconv_t)-1)
    iconv_close(iconv_handle);

    psi_table_free(pp_current_pat_sections);
    psi_table_free(pp_next_pat_sections);
    psi_table_free(pp_current_tsdt_sections);
    psi_table_free(pp_next_tsdt_sections);
    psi_table_free(pp_current_nit_sections);
    psi_table_free(pp_next_nit_sections);
    psi_table_free(pp_current_sdt_sections);
    psi_table_free(pp_next_sdt_sections);

    for (i = 0; i < i_nb_bouquets; i++) {
        psi_table_free(pp_bouquets[i]->pp_current_bat_sections);
        psi_table_free(pp_bouquets[i]->pp_next_bat_sections);
        free(pp_bouquets[i]);
    }
    free(pp_bouquets);

    for (i = 0; i < i_nb_sids; i++) {
        psi_table_free(pp_sids[i]->pp_eit_sections);
        free(pp_sids[i]->p_current_pmt);
        free(pp_sids[i]);
    }
    free(pp_sids);

    for (i = 0; i < 8192; i++)
        psi_assemble_reset(&p_pids[i].p_psi_buffer,
                           &p_pids[i].i_psi_buffer_used);

    close (s);
    return EXIT_SUCCESS;
}
