/*
 * GRIDGPIB back end (tm5000g.exe): the gpib.h interface over the GRIDGPIB resident
 * driver, which must be loaded instead of DRVR488.
 *   - Every call returns when the bus transaction completes, so the GPIB_PACE()
 *     waits Driver488 needed compile to nothing (modules.c and module_funcs.c
 *     are built with -DGRIDGPIB).
 *   - Replies are read whole, never split across reads. Driver488 raw reads
 *     filled the whole buffer, and long replies desynchronised the next transaction.
 *   - Driver488 text commands still sent through ieee_write() go to a small
 *     interpreter (d488_command), so terminal mode and diagnostics work unchanged.
 */

#include "gpib.h"
#include "gridgpib.h"

/* Driver488 text-command interpreter behind ieee_write() / ieee_read() */
#define PEND_SIZE 1024
static char pend[PEND_SIZE];        /* reply waiting for ieee_read()    */
static int  pend_len = 0, pend_pos = 0;
static char last_err[48] = "";      /* shown by STATUS until ABORT/RESET */

static void pend_set(const char *s, int n)
{
    if (n > PEND_SIZE) n = PEND_SIZE;
    memcpy(pend, s, n);
    pend_len = n;
    pend_pos = 0;
}

static void note_error(int e)
{
    if (e < 0) e = -e;
    if (e) {
        strcpy(last_err, "ERROR ");
        strncat(last_err, gg_errstr(e), sizeof(last_err) - 8);
    }
}

static const char *skipsp(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Case-insensitive match of an uppercase keyword not followed by a letter;
   returns the text after it, or NULL. */
static const char *kw(const char *p, const char *word)
{
    while (*word) {
        if (toupper((unsigned char)*p) != *word) return NULL;
        p++; word++;
    }
    if (isalpha((unsigned char)*p)) return NULL;
    return p;
}

static int getaddr(const char **pp)
{
    const char *p = skipsp(*pp);
    int a = -1;
    if (isdigit((unsigned char)*p)) {
        a = 0;
        while (isdigit((unsigned char)*p)) a = a * 10 + (*p++ - '0');
        if (*p == '.') {              /* secondary address: skipped */
            p++;
            while (isdigit((unsigned char)*p)) p++;
        }
    }
    *pp = p;
    return a;
}

static int d488_command(const char *line)
{
    char cmd[GPIB_BUFFER_SIZE * 2];
    const char *p, *q;
    int a, e = 0, n;

    /* copy without the trailing CR/LF */
    n = strcspn(line, "\r\n");
    if (n >= (int)sizeof(cmd)) {          /* refuse rather than send a truncated OUTPUT */
        strcpy(last_err, "ERROR SYNTAX");
        return -1;
    }
    memcpy(cmd, line, n);
    cmd[n] = '\0';
    p = skipsp(cmd);
    if (*p == '\0') return 0;

    if ((q = kw(p, "OUTPUT")) != NULL) {
        a = getaddr(&q);
        q = skipsp(q);
        if (*q == ';') q++;
        q = skipsp(q);
        if (a < 0) { strcpy(last_err, "ERROR SYNTAX"); return -1; }
        e = gg_puts(a, q);
        if (e > 0) e = 0;                     /* byte count -> OK */
    } else if ((q = kw(p, "ENTER")) != NULL) {
        a = getaddr(&q);
        if (a < 0) { strcpy(last_err, "ERROR SYNTAX"); return -1; }
        pend_len = pend_pos = 0;
        e = gg_gets(a, pend, sizeof(pend));   /* straight into the reply buffer */
        if (e >= 0) { pend_len = e; e = 0; }
    } else if ((q = kw(p, "SPOLL")) != NULL) {
        char buf[8];
        unsigned char st = 0;
        a = getaddr(&q);
        if (a < 0) st = gg_srq() ? 64 : 0;      /* bare SPOLL: SRQ seen since last check */
        else e = gg_spoll(a, &st);
        if (e == 0) {
            sprintf(buf, "%u\r\n", st);
            pend_set(buf, strlen(buf));
        }
    } else if ((q = kw(p, "STATUS")) != NULL) {
        char buf[80];
        sprintf(buf, "CS21 GRIDGPIB %s\r\n", last_err[0] ? last_err : "OK");
        pend_set(buf, strlen(buf));
    } else if ((q = kw(p, "HELLO")) != NULL) {
        static const char h[] = "GRIDGPIB 1.1 (Driver488 command subset) Grid-OS\r\n";
        pend_set(h, sizeof(h) - 1);
    } else if (kw(p, "ABORT") || kw(p, "RESET")) {
        last_err[0] = '\0';
        pend_len = pend_pos = 0;
        e = gg_init();
    } else if ((q = kw(p, "CLEAR")) != NULL) {
        a = getaddr(&q);
        e = gg_clear(a < 0 ? GG_ALL : a);
    } else if ((q = kw(p, "REMOTE")) != NULL) {
        a = getaddr(&q);
        e = gg_remote(a < 0 ? GG_ALL : a);
    } else if ((q = kw(p, "LOCAL")) != NULL) {
        a = getaddr(&q);
        e = gg_local(a < 0 ? GG_ALL : a);
    } else if ((q = kw(p, "TRIGGER")) != NULL) {
        a = getaddr(&q);
        if (a < 0) { strcpy(last_err, "ERROR SYNTAX"); return -1; }
        e = gg_trigger(a);
    } else if ((q = kw(p, "TIME")) != NULL) {
        q = skipsp(q);
        if ((q = kw(q, "OUT")) != NULL) {
            double s = atof(skipsp(q));
            gg_timeout(s <= 0 ? 60000u : (unsigned)(s * 1000.0 + 0.5));
        }
    } else if (kw(p, "FILL") || kw(p, "EOL") || kw(p, "BREAK") ||
               kw(p, "BUFFERED") || kw(p, "DISARM") || kw(p, "ARM")) {
        /* accepted and ignored: GRIDGPIB always returns exact replies */
    } else {
        strcpy(last_err, "ERROR SYNTAX");
        return -1;
    }
    if (e < 0) e = -e;
    if (e) { note_error(e); return -1; }
    return 0;
}

/* Driver488-compatible entry points */
int ieee_write(const char *str)
{
    if (d488_command(str) < 0) return -1;
    return strlen(str);
}

/* Returns bytes stored (NUL-terminated), 0 if nothing is pending */
int ieee_read(char *buffer, int maxlen)
{
    int n;
    if (maxlen < 1) return 0;
    n = pend_len - pend_pos;
    if (n > maxlen - 1) n = maxlen - 1;
    if (n > 0) {
        memcpy(buffer, pend + pend_pos, n);
        pend_pos += n;
    } else {
        n = 0;
    }
    buffer[n] = '\0';
    return n;
}

void drain_input_buffer(void)
{
    pend_len = pend_pos = 0;
}

int init_gpib_system(void)
{
    unsigned rec, tmo, res;
    int e;

    printf("Looking for the GRIDGPIB driver...\n");
    if (gg_open() != GG_OK) {
        printf("GRIDGPIB is not loaded - run GRIDGPIB.COM first\n");
        printf("(it replaces DRVR488; do not load both).\n");
        return -1;
    }
    gg_stats(&rec, &tmo, &res);
    printf("GRIDGPIB on INT %02Xh, %u bytes resident.\n", gg_vector(), res);
    e = gg_init();
    if (e) {
        printf("GPIB init failed: %s\n", gg_errstr(e));
        return -1;
    }
    last_err[0] = '\0';
    drain_input_buffer();
    printf("GPIB system initialized successfully.\n");
    return 0;
}

/* TM5000 GPIB functions */
int gpib_check_srq(int address)
{
    unsigned char st;
    if (gg_spoll(address, &st)) return 0;
    return st;
}

const char gpib_driver_name[] = "GRIDGPIB";
const char gpib_driver_note[] = "";

/* On exit drop REN: every device goes local in one register write. Addressing
   each slot with GTL waited out command-byte timeouts when the bus was left in
   an odd state. */
int gpib_release_bus(void)
{
    gg_local(GG_ALL);
    return 1;
}

void gpib_driver_help(void)
{
    printf("\nMake sure:\n");
    printf("1. GRIDGPIB.COM is loaded (not together with DRVR488)\n");
    printf("2. The GPIB cable is connected\n");
}

int gpib_probe(int address, char *id, int maxlen)
{
    unsigned char st;
    int old, n;
    id[0] = '\0';
    old = gg_timeout(150);                 /* a real instrument polls in <1 ms */
    if (gg_spoll(address, &st)) {
        if (old > 0) gg_timeout((unsigned)old);
        return 0;                          /* nothing at this address */
    }
    gg_timeout(1500);
    n = gg_query(address, "ID?", id, maxlen);          /* Tektronix */
    if (n <= 0 || strncmp(id, "ID", 2) != 0) {
        n = gg_query(address, "*IDN?", id, maxlen);    /* IEEE 488.2 */
        if (n <= 0) id[0] = '\0';
    }
    if (old > 0) gg_timeout((unsigned)old);
    return 1;
}

int ieee_spoll(int address, unsigned char *status)
{
    int e = gg_spoll(address, status);
    if (e) { note_error(e); *status = 0; return -1; }
    return 0;
}

/* 0 sent, -1 failed or refused (gg_puts never truncates an over-long command) */
int gpib_write(int address, char *command)
{
    int e = gg_puts(address, command);
    if (e < 0) { note_error(e); return -1; }
    return 0;
}

/* Per-byte read timeout for instruments slow to produce a reading; 0 leaves
 * the current timeout (driver default 2 s). The wait ends as soon as the
 * reply arrives, so a long limit costs nothing. */
static unsigned read_timeout_for(int address)
{
    int i;
    if (!g_system) return 0;
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled &&
            g_system->modules[i].gpib_address == address) {
            switch (g_system->modules[i].module_type) {
            case MOD_DC5009:
            case MOD_DC5010: return dc50xx_timeout_ms(i);   /* from AVE and the input rate */
            case MOD_DM5120: return 10000u;   /* AUTOCAL pauses        */
            case MOD_DM5010: return 5000u;
            }
        }
    }
    return 0;
}

int gpib_read(int address, char *buffer, int maxlen)
{
    unsigned t = read_timeout_for(address);
    int old = t ? gg_timeout(t) : 0;
    int n = gg_gets(address, buffer, maxlen);
    if (old > 0) gg_timeout((unsigned)old);
    if (n < 0) { note_error(n); buffer[0] = '\0'; return 0; }
    return n;
}

int gpib_read_float(int address, float *value)
{
    char buffer[GPIB_BUFFER_SIZE];

    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%*[^+-]%f", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
        if (sscanf(buffer, "%e", value) == 1) return 1;
    }
    return 0;
}

void gpib_remote(int address) { note_error(gg_remote(address)); }
void gpib_local(int address)  { note_error(gg_local(address)); }
void gpib_clear(int address)  { note_error(gg_clear(address)); }

/* Serial-poll before each DM5120 write to clear a pending SRQ, as gpib.c does;
 * it costs about 0.5 ms here. lf_termination is ignored: it only changed the
 * Driver488 command line, never the bus data. */
int gpib_write_dm5120(int address, char *command)
{
    gpib_check_srq(address);
    return gpib_write(address, command);
}

/* The DM5120 (6.5 digits, AUTOCAL ON) sometimes goes quiet for over 2 s, with
 * direct chip access too, so it is the meter, not the driver. Its reads get a
 * 10 s per-byte timeout. */
#define DM5120_TIMEOUT_MS 10000u

int gpib_read_dm5120(int address, char *buffer, int maxlen)
{
    int old = gg_timeout(DM5120_TIMEOUT_MS);
    int n = gpib_read(address, buffer, maxlen);
    if (old > 0) gg_timeout((unsigned)old);
    return n;
}

int gpib_read_float_dm5120(int address, float *value)
{
    char buffer[GPIB_BUFFER_SIZE];

    if (gpib_read_dm5120(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "NDCV%e", value) == 1) return 1;
        if (sscanf(buffer, "DCV%e", value) == 1) return 1;
        if (sscanf(buffer, "NACV%e", value) == 1) return 1;
        if (sscanf(buffer, "ACV%e", value) == 1) return 1;
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
    }
    return 0;
}

void gpib_remote_dm5120(int address) { gpib_remote(address); }
void gpib_local_dm5120(int address)  { gpib_local(address); }

void gpib_clear_dm5120(int address)
{
    gpib_clear(address);               /* SDC alone clears it; no ABORT (IFC) as in gpib.c */
}

int gpib_write_dm5010(int address, char *command)
{
    gpib_check_srq(address);
    return gpib_write(address, command);
}

int gpib_read_dm5010(int address, char *buffer, int maxlen)
{
    return gpib_read(address, buffer, maxlen);
}

int gpib_read_float_dm5010(int address, float *value)
{
    char buffer[GPIB_BUFFER_SIZE];

    if (gpib_read(address, buffer, sizeof(buffer)) > 0) {
        if (sscanf(buffer, "%e", value) == 1) return 1;
        if (sscanf(buffer, "%f", value) == 1) return 1;
    }
    return 0;
}

int command_has_response(const char *cmd)
{
    if (strncasecmp(cmd, "hello", 5) == 0) return 1;
    if (strncasecmp(cmd, "status", 6) == 0) return 1;
    if (strncasecmp(cmd, "enter", 5) == 0) return 1;
    if (strncasecmp(cmd, "spoll", 5) == 0) return 1;
    if (strncasecmp(cmd, "fill", 4) == 0) return 1;
    return 0;
}

/* Query STATUS into gpib_error/g_error_msg; any ERROR gets one ABORT to clear it. */
void check_gpib_error(void)
{
    char status[GPIB_BUFFER_SIZE];

    ieee_write("status\r\n");
    if (ieee_read(status, sizeof(status)) > 0) {
        printf("GPIB Status: %s\n", status);
        if (strstr(status, "ERROR")) {
            strncpy(g_error_msg, status, 63);
            g_error_msg[63] = '\0';
            gpib_error = 1;
            ieee_write("abort\r\n");
            ieee_write("status\r\n");
            if (ieee_read(status, sizeof(status)) > 0 && !strstr(status, "ERROR"))
                gpib_error = 0;
        } else {
            gpib_error = 0;
        }
    } else {
        gpib_error = 1;
        strcpy(g_error_msg, "No status response");
    }
}
