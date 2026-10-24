/*
 * TM5000 module drivers and menus: DC5009/DC5010, FG5010, DM5120, DM5010,
 * PS5004, PS5010, plus slot setup and the live measurement loop.
 */

#include "modules.h"
#include "data.h"
#include "gpib.h"
#include "graphics.h"
#include <stdarg.h>
#include <signal.h>

/* Shared GPIB scratch buffers; far to stay out of DGROUP */
static char __far gpib_cmd_buffer[80];
static char __far gpib_response_buffer[80];

/* ========================================================================== */
/* DC5009 / DC5010 universal counters                                         */
/* ========================================================================== */
/* Tek Codes & Formats. "p. 2-20/2-21" cites the DC5009 manual (070-3888-00)
 * page, then the DC5010 manual (070-3897-02) page. One driver serves both:
 * the DC5009 lacks FIL, NULL, TER, RISE, FALL and TOT A+B/A-B, and the DC5010
 * lacks SOUR and FPTR. */

#define BIOS_TICKS (*((unsigned long far *)0x0040006CL))

static char __far dc_reply[256];        /* SET? runs to about 190 characters */

/* Function commands, p. 2-17/2-18. u: reading unit 0 Hz, 1 s, 2 ratio,
   3 counts, 4 code. m: 1 = DC5010 only, 2 = not in the function menu. */
typedef struct { char hdr[5]; char arg[4]; unsigned char u, m; const char *name; } dc_func_t;
static const dc_func_t __far dc_funcs[] = {
    { "FREQ", "A",   0, 0, "Frequency A" },
    { "PER",  "A",   1, 0, "Period A" },
    { "WID",  "A",   1, 0, "Width A (channel A slope picks the pulse)" },
    { "RAT",  "B/A", 2, 0, "Ratio B/A" },
    { "TIME", "AB",  1, 0, "Time interval A to B" },
    { "TOT",  "A",   3, 0, "Totalize A (START/STOP)" },
    { "EVE",  "BA",  3, 0, "Events B during A" },
    { "TMAN", "",    1, 0, "Manual timing, stopwatch (START/STOP)" },
    { "PROB", "A&B", 4, 0, "Probe compensation (2-digit code)" },
    { "TOT",  "A+B", 3, 1, "Totalize A+B" },
    { "TOT",  "A-B", 3, 1, "Totalize A-B" },
    { "RISE", "A",   1, 1, "Risetime A, 10-90%" },
    { "FALL", "A",   1, 1, "Falltime A, 90-10%" },
    { "TEST", "",    4, 2, "Self test" }
};
#define DC_NFUNC ((int)(sizeof(dc_funcs) / sizeof(dc_funcs[0])))
#define DC_TMAN 7
#define DC_PROB 8
#define DC_TOTAPB 9
#define DC_TOTAMB 10
#define DC_RISE 11
#define DC_FALL 12
#define DC_TEST 13

/* Table 2-3 (p. 2-43/2-45) */
typedef struct { int code; const char *txt; } dc_err_t;
static const dc_err_t __far dc_errs[] = {
    { 101, "command header error" },      { 102, "header delimiter error" },
    { 103, "command argument error" },    { 104, "argument delimiter error" },
    { 105, "nonnumeric argument" },       { 106, "missing argument" },
    { 107, "invalid message unit delimiter" },
    { 201, "not executable in local" },   { 202, "settings lost due to rtl" },
    { 203, "I/O buffers full, output dumped" },
    { 205, "argument out of range" },     { 206, "GET ignored (DT OFF)" },
    { 301, "interrupt fault" },           { 302, "system error" },
    { 401, "power on" },                  { 402, "operation complete" },
    { 403, "user request" },
    { 602, "channel A 50 ohm protect: switched to 1 Mohm" },
    { 603, "channel B 50 ohm protect: switched to 1 Mohm" },
    { 604, "no prescaler" },
    { 711, "channel A overflow" },        { 712, "channel B overflow" }
};
static const char * __far dc_dt[] = { "OFF", "TRIG", "GATE" };

static int dc_is5010(int slot) {
    return g_system->modules[slot].module_type == MOD_DC5010;
}

static dc5009_config *dc_cfg(int slot) {
    return dc_is5010(slot) ? &g_dc5010_config[slot] : &g_dc5009_config[slot];
}

/* Table entry for the slot's function. Old profiles held names such as
   "PERIOD" or a cut-off "TOTALIZ": the first three letters decide. */
static int dc_fn(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int i;
    for (i = 0; i < DC_NFUNC; i++)
        if (strncmp(c->function, dc_funcs[i].hdr, 3) == 0 &&
            (strcmp(dc_funcs[i].hdr, "TOT") || strcmp(c->channel, dc_funcs[i].arg) == 0))
            break;
    if (i == DC_NFUNC) i = strncmp(c->function, "TOT", 3) ? 0 : 5;
    if ((dc_funcs[i].m & 1) && !dc_is5010(slot)) i = 0;
    return i;
}

/* TOT and TMAN run until STOP; RES would zero them */
static int dc_running_total(int f) {
    return f == DC_TMAN || strcmp(dc_funcs[f].hdr, "TOT") == 0;
}

/* AVE as a decade exponent, -1 = auto. 1.E+8 is the DC5009 maximum, 1.E+9
   the DC5010's (p. 2-20/2-21). Old profiles stored a count. */
static int dc_avg(dc5009_config *c, int is10) {
    int n = c->averaging;
    if (n > 9) n = (int)floor(log10((double)n) + 0.5);
    if (n > (is10 ? 9 : 8)) n = is10 ? 9 : 8;
    return n < 0 ? -1 : n;
}

/* X1 or X5; old profiles said "X1"/"X10", and there is no X10 (p. 2-19/2-20) */
static int dc_att(const char *s) {
    return strchr(s, '5') ? 5 : 1;
}

/* LEV range, p. 2-25/2-27: DC5009 -3.175..3.2 V at X1, -15.875..16 V at X5;
   DC5010 +/-2 V at X1, +/-10 V at X5 */
static int dc_lev_ok(int is10, int att, double v) {
    double hi = is10 ? (att == 5 ? 10.0 : 2.0) : (att == 5 ? 16.0 : 3.2);
    double lo = is10 ? -hi : (att == 5 ? -15.875 : -3.175);
    return v >= lo && v <= hi;
}

/* Is channel ch's level set by AUTO? */
static int dc_lev_auto(dc5009_config *c, int ch) {
    return ch ? (c->auto_trigger ^ c->b_level_swap) : c->auto_trigger;
}

static const char *dc_on(int b) {
    return b ? "ON" : "OFF";
}

/* One setting per message: an error drops the rest of its message (p. 2-40) */
static void dc_sendf(int addr, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsprintf(gpib_cmd_buffer, fmt, ap);
    va_end(ap);
    gpib_write(addr, gpib_cmd_buffer);
    GPIB_PACE(50);
}

/* Query into dc_reply, line ends stripped; 0 if nothing came back */
static int dc_query(int addr, const char *cmd) {
    int n;
    dc_reply[0] = '\0';
    if (gpib_write(addr, (char *)cmd) < 0) return 0;
    GPIB_PACE(50);
    n = gpib_read(addr, dc_reply, sizeof(dc_reply));
    while (n > 0 && (dc_reply[n - 1] == '\r' || dc_reply[n - 1] == '\n')) dc_reply[--n] = '\0';
    return n > 0;
}

/* Number after the reply's header ("ERR 205;", "LEVEL -1.025;"); SEND has none */
static int dc_reply_num(double *v) {
    char *p = dc_reply, *e;
    while (*p == ' ') p++;
    while (isalpha((unsigned char)*p)) p++;
    *v = strtod(p, &e);
    return e != p;
}

/* Group Execute Trigger: Driver488's TRIGGER command, also in gpib_gg */
static void dc_get(int addr) {
    char cmd[16];
    sprintf(cmd, "TRIGGER %2d\r\n", addr);
    ieee_write(cmd);
}

static const char *dc_err_text(int code) {
    int i;
    for (i = 0; i < (int)(sizeof(dc_errs) / sizeof(dc_errs[0])); i++)
        if (dc_errs[i].code == code) return dc_errs[i].txt;
    return "see Table 2-3";
}

/* TOT/TMAN with OVER ON restart after 2^40 (DC5009) or 2^43 (DC5010) counts.
   TMAN overflows the B counter (time base), TOT the A counter; TOT A+B and
   A-B also use B, which adds or subtracts (p. 2-27/2-29, DC5010 p. 2-38). */
static void dc_ovf_note(dc5009_config *c, int f, int b) {
    if (!c->overflow_enabled) return;
    if (f == DC_TMAN) c->overflow_count += b;
    else if (strcmp(dc_funcs[f].hdr, "TOT") == 0)
        c->overflow_count += !b ? 1 : f == DC_TOTAPB ? 1 : f == DC_TOTAMB ? -1 : 0;
}

#define DC_QUIET  1
#define DC_RQSOFF 2     /* RQS OFF was just sent: read ERR? without serial polls */

/* Print and clear pending events. With RQS OFF, ERR? returns them one at a
   time, highest priority first, until 0; with RQS ON each serial poll reports
   one event and ERR? then explains it (p. 2-22, 2-43/2-45). */
static int dc_show_errors(int slot, int flags) {
    dc5009_config *c = dc_cfg(slot);
    int addr = g_system->modules[slot].gpib_address, i, n = 0, code;
    int spoll = c->srq_enabled && !(flags & DC_RQSOFF);
    unsigned char st;
    double v;
    for (i = 0; i < 10; i++) {
        if (spoll && (ieee_spoll(addr, &st) != 0 || !(st & 64))) break;
        if (!dc_query(addr, "ERR?") || !dc_reply_num(&v) || v == 0.0) break;
        code = (int)v;
        if (code == 711 || code == 712) dc_ovf_note(c, dc_fn(slot), code == 712);
        if (!(flags & DC_QUIET)) printf("  ERR %d: %s\n", code, dc_err_text(code));
        n++;
    }
    return n;
}

/* The DC5010 power-on settings (DC5010 p. 2-26), with RQS OFF */
static void dc_defaults(dc5009_config *c) {
    memset(c, 0, sizeof(*c));
    strcpy(c->function, "FREQ");
    strcpy(c->channel, "A");
    c->averaging = -1;
    strcpy(c->coupling_a, "DC");
    strcpy(c->coupling_b, "DC");
    strcpy(c->impedance_a, "HI");
    strcpy(c->impedance_b, "HI");
    strcpy(c->attenuation_a, "1");
    strcpy(c->attenuation_b, "1");
    strcpy(c->slope_a, "POS");
    strcpy(c->slope_b, "POS");
    c->auto_trigger = 1;
}

void init_dc5009_config(int slot) { dc_defaults(&g_dc5009_config[slot]); }
void init_dc5010_config(int slot) { dc_defaults(&g_dc5010_config[slot]); }

/* Expected time for one result, ms. AVE n waits for 10^n events on A; auto
   averages take about 0.3 s or one event (p. 2-20/2-21, DC5009 p. 1-8,
   DC5010 Table 1-1). The A rate comes from the last FREQ or PER reading;
   1 kHz is assumed without one. */
static unsigned long dc_expect_ms(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int f = dc_fn(slot), n = dc_avg(c, dc_is5010(slot));
    double v = c->last_measurement, fa = 0.0, t;
    if (v > 0.0) fa = f == 0 ? v : f == 1 ? 1.0 / v : 0.0;
    if (c->preset_enabled) fa /= 16.0;    /* A sees the prescaler's f/16 (p. 2-28) */
    if (n < 0) {
        t = 0.3;
        if (fa > 0.0 && 1.0 / fa > t) t = 1.0 / fa;
    } else {
        t = pow(10.0, n) / (fa > 0.0 ? fa : 1e3);
    }
    if (f == DC_RISE || f == DC_FALL || f == DC_PROB) t += 2.5;   /* autotrigger, DC5010 p. 2-27 */
    if (t > 600.0) t = 600.0;
    return (unsigned long)(t * 1000.0);
}

/* Per-byte read timeout for gpib_gg. SEND goes out only after RDY 1, so
   replies are prompt; the cap keeps a dead counter from stalling the monitor. */
unsigned dc50xx_timeout_ms(int slot) {
    unsigned long t = dc_expect_ms(slot) + 3000UL;
    return t > 12000UL ? 12000u : (unsigned)t;
}

/* What the overflows seen so far add to a TOT/TMAN reading: 2^40 or 2^43
   counts, or as many 10 ns time-base pulses for TMAN (p. 2-27/2-29) */
static double dc_overflow_ext(int slot, int f) {
    dc5009_config *c = dc_cfg(slot);
    int is10 = dc_is5010(slot);

    if (!c->overflow_enabled || !dc_running_total(f)) return 0.0;
    dc_show_errors(slot, DC_QUIET);       /* counts 711/712 via dc_ovf_note */
    return c->overflow_count * (f == DC_TMAN ? (is10 ? 87960.93022208 : 10995.11627776)
                                             : (is10 ? 8796093022208.0 : 1099511627776.0));
}

/* One reading without waiting: 1 = new result in *value, 0 = none ready yet,
   -1 = no reply. SEND is sent only after RDY 1: otherwise it waits for a
   result, which never comes while a measurement is STOPped (p. 2-30/2-32). */
int dc50xx_read(int slot, double *value) {
    int addr = g_system->modules[slot].gpib_address;
    double v;
    if (!dc_query(addr, "RDY?") || !dc_reply_num(&v)) return -1;
    if (v < 1.0) return 0;
    if (!dc_query(addr, "SEND") || !dc_reply_num(&v)) return -1;
    v += dc_overflow_ext(slot, dc_fn(slot));
    dc_cfg(slot)->last_measurement = (float)v;
    *value = v;
    return 1;
}

/* Poll RDY? until 1 (1), ms pass or ESC (0), or no reply (-1) */
static int dc_wait_rdy(int addr, unsigned long ms) {
    unsigned long t0 = BIOS_TICKS, lim = ms * 182UL / 10000UL + 1;
    double v;
    for (;;) {
        if (!dc_query(addr, "RDY?") || !dc_reply_num(&v)) return -1;
        if (v >= 1.0) return 1;
        if (BIOS_TICKS - t0 > lim || (kbhit() && getch() == 27)) return 0;
        delay(50);
    }
}

/* Wait for a result as dc_wait_rdy, then read it */
static int dc_wait(int slot, double *value, unsigned long ms) {
    int r = dc_wait_rdy(g_system->modules[slot].gpib_address, ms);
    return r == 1 ? dc50xx_read(slot, value) : r;
}

/* A new function: old overflows and the last reading (which sets the
   expected measurement time) no longer apply, and the trace unit changes */
static void dc_new_function(int slot, const char *h, const char *a) {
    dc5009_config *c = dc_cfg(slot);
    if (strcmp(c->function, h) || strcmp(c->channel, a)) {
        c->overflow_count = 0;
        c->last_measurement = 0.0f;
    }
    sprintf(c->function, "%.7s", h);
    sprintf(c->channel, "%.3s", a);
    g_traces[slot].unit_type = dc50xx_trace_unit(slot);
}

/* Take the counter's own function (FUNC?, p. 2-24/2-25): front-panel changes
   in local, and the unit the readings carry */
void dc50xx_sync_function(int slot) {
    char h[8], a[8];
    a[0] = '\0';
    if (!dc_query(g_system->modules[slot].gpib_address, "FUNC?") ||
        sscanf(dc_reply, " %7[A-Z] %7[^;]", h, a) < 1) return;
    dc_new_function(slot, h, a);
}

/* Single result for single_measurement. Phase 0 arms every counter first:
   STOP, then RES (or GET with DT TRIG) makes one result and stops again
   (DC5009 p. 2-46). Phase 1 waits for it, reads it and restarts free
   running. TOT and TMAN are only read. */
int dc50xx_single(int slot, int phase, double *value) {
    int addr = g_system->modules[slot].gpib_address, f, single, r;
    if (phase == 0) dc50xx_sync_function(slot);
    f = dc_fn(slot);
    single = !dc_running_total(f) && f != DC_PROB && f != DC_TEST;
    if (phase == 0) {
        if (single) {
            dc_sendf(addr, "STOP");
            if (dc_cfg(slot)->dt_mode == 1) dc_get(addr);
            else dc_sendf(addr, "RES");
        }
        return 0;
    }
    r = dc_wait(slot, value, dc_expect_ms(slot) * 2 + 3000UL);
    if (single) dc_sendf(addr, "START");
    return r;
}

/* Reading with its unit, 19 columns so monitor redraws line up */
void dc50xx_format(char *b, int slot, double v) {
    static const char * __far su[] = { "s ", "ms", "us", "ns" };
    int u = dc_funcs[dc_fn(slot)].u, k = 0;
    double a = fabs(v);
    switch (u) {
    case 0:
        if (a >= 1e6)      sprintf(b, "%13.7f MHz  ", v / 1e6);
        else if (a >= 1e3) sprintf(b, "%13.6f kHz  ", v / 1e3);
        else               sprintf(b, "%13.4f Hz   ", v);
        break;
    case 1:
        while (k < 3 && a != 0.0 && a < 1.0) { a *= 1e3; v *= 1e3; k++; }
        sprintf(b, "%13.6f %s   ", v, su[k]);
        break;
    case 2:  sprintf(b, "%13.7g B/A  ", v); break;
    case 3:  sprintf(b, "%13.0f cnt  ", v); break;
    default: sprintf(b, "%13.0f code ", v); break;
    }
}

int dc50xx_trace_unit(int slot) {
    int u = dc_funcs[dc_fn(slot)].u;
    return u == 0 ? UNIT_FREQUENCY : u == 1 ? UNIT_TIME : UNIT_COUNT;
}

/* Trigger levels: DC5009 AUTO (both), FPTR or LEV; DC5010 AUTO A, B or A&B
   and LEV for the rest. AUTO takes up to 2.5 s (p. 2-19/2-20), during which
   the counter holds off the bus. */
static void dc_send_levels(int addr, dc5009_config *c, int is10) {
    int ch, a0 = dc_lev_auto(c, 0), a1 = dc_lev_auto(c, 1);
    if (!is10 && c->fptrig) {
        dc_sendf(addr, "FPTR");
        return;
    }
    if (is10 ? (a0 || a1) : a0) {
        dc_sendf(addr, !is10 ? "AUTO" : a0 && a1 ? "AUTO A&B" : a0 ? "AUTO A" : "AUTO B");
        delay(2600);
        if (!is10) return;
    }
    for (ch = 0; ch < 2; ch++) {
        if (dc_lev_auto(c, ch)) continue;
        dc_sendf(addr, "CHA %c", 'A' + ch);
        dc_sendf(addr, "LEV %.3f", ch ? c->level_b : c->level_a);
    }
}

/* Send the slot's setup, then list what the counter rejected. RQS OFF first
   so ERR? queues every event (p. 2-43/2-45). */
static void dc_apply(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int addr = g_system->modules[slot].gpib_address, is10 = dc_is5010(slot);
    int f = dc_fn(slot), ch, n;
    int rf = f == DC_RISE || f == DC_FALL;

    printf("Applying settings...\n");
    gpib_remote(addr);
    dc_sendf(addr, "RQS OFF");
    dc_show_errors(slot, DC_QUIET | DC_RQSOFF);   /* older events: the list below is this Apply's */
    for (ch = 0; ch < 2; ch++) {
        /* RISE/FALL copy channel A's input settings to B (DC5010 p. 2-24, 2-33) */
        if (ch == 1 && rf) break;
        dc_sendf(addr, "CHA %c", 'A' + ch);
        dc_sendf(addr, "ATT %d", dc_att(ch ? c->attenuation_b : c->attenuation_a));
        dc_sendf(addr, "COU %s", ch ? c->coupling_b : c->coupling_a);
        dc_sendf(addr, "SLO %s", ch ? c->slope_b : c->slope_a);
        if (is10) dc_sendf(addr, "TER %s", ch ? c->impedance_b : c->impedance_a);
        else dc_sendf(addr, "SOUR %s", (ch ? c->rear_b : c->rear_a) ? "REAR" : "FRONT");
    }
    n = dc_avg(c, is10);
    if (n <= 0) dc_sendf(addr, n ? "AVE -1" : "AVE 1");
    else dc_sendf(addr, "AVE 1.E+%d", n);
    dc_sendf(addr, "PRE %s", dc_on(c->preset_enabled));
    if (is10) dc_sendf(addr, "FIL %s", dc_on(c->filter_enabled));
    /* RISE, FALL and PROB set their own levels by autotrigger */
    if (!rf && f != DC_PROB) dc_send_levels(addr, c, is10);
    dc_sendf(addr, dc_funcs[f].arg[0] ? "%s %s" : "%s", dc_funcs[f].hdr, dc_funcs[f].arg);
    if (rf || f == DC_PROB) delay(2600);
    dc_sendf(addr, "CHA A");              /* the power-on selection, for later raw commands */
    dc_sendf(addr, "OVER %s", dc_on(c->overflow_enabled));
    dc_sendf(addr, "OPC %s", dc_on(c->opc_enabled));
    dc_sendf(addr, "USER %s", dc_on(c->user_enabled));
    dc_sendf(addr, "DT %s", dc_dt[c->dt_mode]);
    /* NULL ON stores the current result, so it needs one first; the function
       command has already reset the null (DC5010 p. 2-28) */
    if (is10 && c->null_enabled) {
        printf("Waiting for a result to null...\n");
        if (dc_wait_rdy(addr, dc_expect_ms(slot) + 3000UL) == 1) dc_sendf(addr, "NULL ON");
        else printf("No result, NULL not set\n");
    } else if (is10) {
        dc_sendf(addr, "NULL OFF");
    }
    c->overflow_count = 0;
    if (dc_show_errors(slot, DC_RQSOFF) == 0) printf("Settings applied, no errors.\n");
    if (c->srq_enabled) dc_sendf(addr, "RQS ON");
}

/* Take a SET? reply into the slot's config (p. 2-32/2-34). The DC5010 page
   prints ':' between two fields, so both separators are accepted. */
static void dc_parse_settings(int slot) {
    dc5009_config *c = dc_cfg(slot);
    char *t, h[8], a[8];
    int ch = 0, first = 1, on;
    double v;

    for (t = dc_reply; *t; t++) if (*t == ':') *t = ';';
    for (t = strtok(dc_reply, ";"); t; t = strtok(NULL, ";")) {
        a[0] = '\0';
        if (sscanf(t, " %7[A-Z] %7s", h, a) < 1) continue;
        if (first) {                      /* <function>; comes first */
            dc_new_function(slot, h, a);
            first = 0;
            continue;
        }
        on = strcmp(a, "ON") == 0;
        v = atof(a);
        if (!strcmp(h, "CHA")) ch = a[0] == 'B';
        else if (!strcmp(h, "ATT")) strcpy(ch ? c->attenuation_b : c->attenuation_a, v == 5.0 ? "5" : "1");
        else if (!strcmp(h, "COU")) sprintf(ch ? c->coupling_b : c->coupling_a, "%.2s", a);
        else if (!strcmp(h, "SLO")) sprintf(ch ? c->slope_b : c->slope_a, "%.3s", a);
        else if (!strncmp(h, "TER", 3)) sprintf(ch ? c->impedance_b : c->impedance_a, "%.2s", a);
        else if (!strcmp(h, "SOUR")) {
            if (ch) c->rear_b = a[0] == 'R'; else c->rear_a = a[0] == 'R';
        } else if (!strncmp(h, "LEV", 3)) {
            /* LEV 9999 means front-panel control (DC5009 p. 2-25) */
            c->fptrig = v == 9999.0;
            c->auto_trigger = 0;
            c->b_level_swap = 0;
            if (v != 9999.0) { if (ch) c->level_b = (float)v; else c->level_a = (float)v; }
        }
        else if (!strcmp(h, "AVE")) c->averaging = v <= 0.0 ? -1 : (int)floor(log10(v) + 0.5);
        else if (!strcmp(h, "OPC")) c->opc_enabled = on;
        else if (!strcmp(h, "OVER")) c->overflow_enabled = on;
        else if (!strcmp(h, "PRE")) c->preset_enabled = on;
        else if (!strcmp(h, "FIL")) c->filter_enabled = on;
        else if (!strcmp(h, "NULL")) c->null_enabled = on;
        else if (!strcmp(h, "USER")) c->user_enabled = on;
        else if (!strcmp(h, "RQS")) c->srq_enabled = on;
        else if (!strcmp(h, "DT")) c->dt_mode = a[0] == 'T' ? 1 : a[0] == 'G' ? 2 : 0;
    }
}

/* Communication test: queries only, after RQS OFF so ERR? queues events (RQS
   ON is restored if the slot wants it). A step passes only if its reply
   parses and ERR? then reads 0. */
void dc50xx_test(int slot) {
    int addr = g_system->modules[slot].gpib_address, pass = 0, step;
    static const char * __far q[] = { "ID?", "FUNC?", "AVE?", "SET?" };
    double v;

    printf("\n=== %s communication test, GPIB %d ===\n", dc_is5010(slot) ? "DC5010" : "DC5009", addr);
    dc_sendf(addr, "RQS OFF");
    if (dc_show_errors(slot, DC_RQSOFF)) printf("  (events left from before the test)\n");
    for (step = 0; step < 4; step++) {
        int ok = dc_query(addr, q[step]);
        printf("%d. %-6s %.60s\n", step + 1, q[step], ok ? dc_reply : "no reply");
        if (step == 0 && strstr(dc_reply, "TEK/DC50") == NULL) ok = 0;
        if (ok && dc_show_errors(slot, DC_RQSOFF) == 0) pass++;
    }
    printf("5. Reading: ");
    dc50xx_sync_function(slot);
    if (dc_wait(slot, &v, dc_expect_ms(slot) + 3000UL) == 1) {
        char b[32];
        dc50xx_format(b, slot, v);
        printf("%s\n", b);
        pass++;
    } else {
        printf("none (no signal, STOPped, or still counting)\n");
    }
    printf("\nPassed %d of 5\n", pass);
    if (dc_cfg(slot)->srq_enabled) dc_sendf(addr, "RQS ON");
    printf("\nPress any key to continue...");
    getch();
}

/* One keypress out of keys (upper case); -1 for any other key */
static int dc_pick(const char *prompt, const char *keys) {
    const char *p;
    int k;
    printf("%s", prompt);
    k = toupper(getch());
    printf("%c\n", k > ' ' ? k : ' ');
    p = strchr(keys, k);
    return p && k ? (int)(p - keys) : -1;
}

static int dc_ask(const char *prompt, double *v) {
    char line[24];
    printf("%s", prompt);
    return scanf(" %23s", line) == 1 && sscanf(line, "%lf", v) == 1;
}

/* Prompt for channel ch's trigger level within the model's LEV range */
static void dc_ask_level(dc5009_config *c, int is10, int ch) {
    double v;
    int att = dc_att(ch ? c->attenuation_b : c->attenuation_a);
    printf("Channel %c level, V (X%d: %s): ", 'A' + ch, att,
           is10 ? (att == 5 ? "-10 to 10" : "-2 to 2") : (att == 5 ? "-15.875 to 16" : "-3.175 to 3.2"));
    if (!dc_ask("", &v) || !dc_lev_ok(is10, att, v)) {
        printf("Out of range, level not changed\n");
        return;
    }
    if (ch) c->level_b = (float)v; else c->level_a = (float)v;
}

/* AUTO now, then each channel's MAX?, MIN? and the level it set (p. 2-19/2-20, 2-26/2-27) */
static void dc_autotrig(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int addr = g_system->modules[slot].gpib_address, ch, i;
    static const char * __far q[] = { "MAX?", "MIN?", "LEV?" };

    dc_sendf(addr, dc_is5010(slot) ? "AUTO A&B" : "AUTO");
    printf("Auto trigger (up to 2.5 s)...\n");
    delay(2600);
    for (ch = 0; ch < 2; ch++) {
        dc_sendf(addr, "CHA %c", 'A' + ch);
        printf("Channel %c:", 'A' + ch);
        for (i = 0; i < 3; i++) printf("  %s", dc_query(addr, q[i]) ? dc_reply : "-");
        printf("\n");
    }
    dc_sendf(addr, "CHA A");
    c->auto_trigger = 1;
    c->fptrig = 0;
    c->b_level_swap = 0;
    dc_show_errors(slot, 0);
}

/* Every query in the command list (p. 2-17/2-18, 2-18/2-19) */
static void dc_show_settings(int slot) {
    int addr = g_system->modules[slot].gpib_address, is10 = dc_is5010(slot), ch, i;
    static const char * __far qa[] = { "ID?", "FUNC?", "AVE?", "CHA?", "PRE?", "OPC?", "OVER?",
                                       "RQS?", "USER?", "DT?", "RDY?", "FIL?", "NULL?" };
    static const char * __far qc[] = { "ATT?", "COU?", "SLO?", "LEV?", "MAX?", "MIN?", "TER?", "SOUR?" };

    if (dc_query(addr, "SET?")) printf("SET?  %s\n\n", dc_reply);
    for (i = 0; i < (is10 ? 13 : 11); i++)
        printf("%-6s %-22.40s%s", qa[i], dc_query(addr, qa[i]) ? dc_reply : "-", i % 2 ? "\n" : "");
    printf("\n");
    for (ch = 0; ch < 2; ch++) {
        dc_sendf(addr, "CHA %c", 'A' + ch);
        printf("Channel %c:", 'A' + ch);
        for (i = 0; i < 7; i++) {
            const char *s = qc[i == 6 && !is10 ? 7 : i];
            printf(" %s", dc_query(addr, s) ? dc_reply : "-");
        }
        printf("\n");
    }
    dc_sendf(addr, "CHA A");
    dc_show_errors(slot, 0);
}

/* ID, self test, error queue, INIT */
static void dc_utilities(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int addr = g_system->modules[slot].gpib_address, f, done = 0;
    double v;

    while (!done) {
        clrscr();
        printf("%s Utilities - GPIB %d\n", dc_is5010(slot) ? "DC5010" : "DC5009", addr);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. Self test (TEST): ROM, I/O and counter tests, 0 = pass\n");
        printf("3. Read the error/event queue (ERR?)\n");
        printf("4. INIT - power-on settings\n");
        printf("5. Clear the overflow count (%ld)\n", c->overflow_count);
        printf("0. Return\n\nSelect option: ");
        switch (getch()) {
        case '1':
            printf("\n\n%s\n", dc_query(addr, "ID?") ? dc_reply : "No reply");
            break;
        case '2':
            /* the result is output like a reading; a function command ends it (p. 2-34/2-37) */
            printf("\n\nRunning self test...\n");
            dc_sendf(addr, "TEST");
            if (dc_wait(slot, &v, 15000UL) == 1)
                printf(v == 0.0 ? "Self test passed\n" : "Self test FAILED: code %.0f (Table 2-4)\n", v);
            else
                printf("No result\n");
            f = dc_fn(slot);
            dc_sendf(addr, dc_funcs[f].arg[0] ? "%s %s" : "%s", dc_funcs[f].hdr, dc_funcs[f].arg);
            dc_show_errors(slot, 0);
            break;
        case '3':
            printf("\n\n");
            if (dc_show_errors(slot, 0) == 0) printf("No errors or events\n");
            break;
        case '4':
            printf("\n\nINIT sets the power-on state: %s\n",
                   dc_is5010(slot) ? "FREQ A, AVE -1, X1, DC, 1 Mohm, AUTO levels (p. 2-26)"
                                   : "function, averages, inputs from the front panel (p. 2-25)");
            if (dc_pick("Reset the counter's setup (Y/N)? ", "Y") == 0) {
                dc_sendf(addr, "INIT");
                delay(2600);              /* includes an autotrigger, up to 2.5 s */
                c->overflow_count = 0;
                printf("Done. D in the setup menu re-applies TM5000's settings.\n");
            }
            break;
        case '5':
            c->overflow_count = 0;
            printf("\n\nCleared\n");
            break;
        case '0':
        case 27:
            done = 1;
            break;
        }
        if (!done) {
            printf("\nPress any key...");
            getch();
        }
    }
}

static void dc50xx_menu(int slot) {
    dc5009_config *c = dc_cfg(slot);
    int addr = g_system->modules[slot].gpib_address, is10 = dc_is5010(slot);
    const char *model = is10 ? "DC5010" : "DC5009";
    int key, ch, f, i, k, wait;
    double v;
    char b[32];

    for (;;) {
        f = dc_fn(slot);
        k = dc_avg(c, is10);
        clrscr();
        printf("%s Counter - Slot %d, GPIB %d\n", model, slot, addr);
        printf("================================\n\n");
        printf("1. Function:  %s (%s %s)\n", dc_funcs[f].name, dc_funcs[f].hdr, dc_funcs[f].arg);
        if (k < 0) printf("2. Averages:  AUTO, about 0.3 s per result\n");
        else printf("2. Averages:  1.E+%d (at least %.0f events on A)\n", k, pow(10.0, k));
        for (ch = 0; ch < 2; ch++) {
            printf("%d. Channel %c: X%d, %s, %s slope, ", 3 + ch, 'A' + ch,
                   dc_att(ch ? c->attenuation_b : c->attenuation_a),
                   ch ? c->coupling_b : c->coupling_a, ch ? c->slope_b : c->slope_a);
            if (is10) printf("%s\n", (ch ? c->impedance_b : c->impedance_a)[0] == 'L' ? "50 ohm" : "1 Mohm");
            else printf("%s input\n", (ch ? c->rear_b : c->rear_a) ? "rear interface" : "front");
        }
        printf("5. Trigger levels: ");
        if (!is10 && c->fptrig) printf("front-panel knobs (FPTRIG)\n");
        else if (!is10 && c->auto_trigger) printf("AUTO, both channels\n");
        else {
            for (ch = 0; ch < 2; ch++) {
                if (dc_lev_auto(c, ch)) printf("%c AUTO   ", 'A' + ch);
                else printf("%c %.3f V   ", 'A' + ch, ch ? c->level_b : c->level_a);
            }
            printf("\n");
        }
        printf("6. Prescaler /16: %s", dc_on(c->preset_enabled));
        if (is10) printf("   7. 20 MHz filter: %s   8. Null: %s", dc_on(c->filter_enabled), dc_on(c->null_enabled));
        printf("\n9. SRQ: RQS %s, OPC %s, OVER %s, USER %s   A. Device trigger: DT %s\n",
               dc_on(c->srq_enabled), dc_on(c->opc_enabled), dc_on(c->overflow_enabled),
               dc_on(c->user_enabled), dc_dt[c->dt_mode]);
        if (c->overflow_enabled) printf("   Overflows counted: %ld\n", c->overflow_count);
        printf("\nD. Apply settings to the %s   L. Load settings from it (SET?)\n", model);
        printf("R. Read one result   S. START   T. STOP   E. RESET   G. Send GET\n");
        printf("U. Auto trigger now, show MAX/MIN   Q. Show the counter's settings\n");
        printf("P. Utilities (ID, self test, errors, INIT)   0. Return\n\nSelect option: ");
        key = toupper(getch());
        printf("\n\n");
        wait = 1;
        switch (key) {
        case '1':
            for (i = 0; i < DC_NFUNC; i++)
                if (!(dc_funcs[i].m & 2) && (is10 || !(dc_funcs[i].m & 1)))
                    printf("%c. %s\n", i < 10 ? '0' + i : 'A' + i - 10, dc_funcs[i].name);
            i = dc_pick("Select: ", is10 ? "0123456789ABC" : "012345678");
            if (i >= 0) dc_new_function(slot, dc_funcs[i].hdr, dc_funcs[i].arg);
            wait = 0;
            break;
        case '2':
            /* decades only; the counter rounds others and may refuse them (p. 2-20/2-21) */
            if (!dc_ask(is10 ? "Averages: 0 = AUTO, or 1, 10, 100 ... 1E9: " : "Averages: 0 = AUTO, or 1, 10, 100 ... 1E8: ", &v))
                break;
            if (v <= 0.0) { c->averaging = -1; wait = 0; break; }
            k = (int)floor(log10(v) + 0.5);
            if (fabs(log10(v) - k) > 1e-6 || k < 0 || k > (is10 ? 9 : 8)) {
                printf("Decades only, 1 to 1E%d\n", is10 ? 9 : 8);
                break;
            }
            c->averaging = k;
            wait = 0;
            break;
        case '3':
        case '4':
            ch = key - '3';
            i = dc_pick("Attenuation (1 = X1, 5 = X5, other = keep): ", "15");
            if (i >= 0) strcpy(ch ? c->attenuation_b : c->attenuation_a, i ? "5" : "1");
            i = dc_pick("Coupling (A = AC, D = DC, other = keep): ", "AD");
            if (i >= 0) strcpy(ch ? c->coupling_b : c->coupling_a, i ? "DC" : "AC");
            i = dc_pick("Slope (P = POS, N = NEG, other = keep): ", "PN");
            if (i >= 0) strcpy(ch ? c->slope_b : c->slope_a, i ? "NEG" : "POS");
            if (is10) {
                i = dc_pick("Termination (H = 1 Mohm, L = 50 ohm, other = keep): ", "HL");
                if (i >= 0) strcpy(ch ? c->impedance_b : c->impedance_a, i ? "LO" : "HI");
                /* 50 ohm protect switches back to HI above 2 V at X1 (DC5010 p. 2-36) */
                if (i == 1) printf("50 ohm: over 2 V at X1 the counter switches to 1 Mohm (ERR %d)\n", 602 + ch);
            } else {
                i = dc_pick("Source (F = front BNC, R = rear interface, other = keep): ", "FR");
                if (i >= 0) { if (ch) c->rear_b = i; else c->rear_a = i; }
            }
            if (!dc_lev_auto(c, ch) && !(!is10 && c->fptrig) &&
                !dc_lev_ok(is10, dc_att(ch ? c->attenuation_b : c->attenuation_a), ch ? c->level_b : c->level_a))
                printf("The trigger level is outside this attenuation's range: set it again (5)\n");
            break;
        case '5':
            if (!is10) {
                i = dc_pick("Levels (A = AUTO, F = front-panel knobs, M = manual): ", "AFM");
                if (i < 0) break;
                c->auto_trigger = i == 0;
                c->fptrig = i == 1;
                c->b_level_swap = 0;
                if (i == 2) { dc_ask_level(c, is10, 0); dc_ask_level(c, is10, 1); }
            } else {
                int a[2];
                for (ch = 0; ch < 2; ch++) {
                    printf("Channel %c ", 'A' + ch);
                    a[ch] = dc_pick("level (A = AUTO, M = manual, other = keep): ", "AM");
                    if (a[ch] == 1) dc_ask_level(c, is10, ch);
                    if (a[ch] < 0) a[ch] = !dc_lev_auto(c, ch);
                }
                c->auto_trigger = a[0] == 0;
                c->b_level_swap = (a[0] == 0) != (a[1] == 0);
            }
            wait = 0;
            break;
        case '6':
            c->preset_enabled = !c->preset_enabled;
            /* x16 for a divide-by-16 prescaler on A; without one ERR 604 (p. 2-28/2-30) */
            if (c->preset_enabled) printf("Only with a /16 prescaler on channel A (else ERR 604)\n");
            else wait = 0;
            break;
        case '7':
            if (is10) c->filter_enabled = !c->filter_enabled;
            wait = 0;
            break;
        case '8':
            if (!is10) { wait = 0; break; }
            c->null_enabled = !c->null_enabled;
            if (c->null_enabled) printf("On Apply, NULL ON waits for a result, stores it and subtracts it from later ones\n");
            else wait = 0;
            break;
        case '9':
            i = dc_pick("Toggle R = RQS, O = OPC, V = OVER, U = USER: ", "ROVU");
            if (i == 0) c->srq_enabled = !c->srq_enabled;
            if (i == 1) c->opc_enabled = !c->opc_enabled;
            if (i == 2) c->overflow_enabled = !c->overflow_enabled;
            if (i == 3) c->user_enabled = !c->user_enabled;
            /* with RQS ON, ERR? only explains the last serial-polled event (p. 2-22/2-23) */
            if (i == 0 && c->srq_enabled) printf("RQS ON: TM5000 then reads overflow events by serial poll\n");
            else wait = 0;
            break;
        case 'A':
            c->dt_mode = (c->dt_mode + 1) % 3;
            wait = 0;
            break;
        case 'D':
            dc_apply(slot);
            break;
        case 'L':
            if (dc_query(addr, "SET?")) {
                printf("%s\n", dc_reply);
                dc_parse_settings(slot);
                printf("\nLoaded. Levels read back as manual values.\n");
            } else {
                printf("No reply\n");
            }
            break;
        case 'R':
            dc50xx_single(slot, 0, &v);
            i = dc50xx_single(slot, 1, &v);
            if (i == 1) { dc50xx_format(b, slot, v); printf("Reading: %s\n", b); }
            else printf(i ? "No reply\n" : "No result (no signal, or TOT/TMAN still counting)\n");
            break;
        case 'S':
        case 'T':
        case 'E':
            dc_sendf(addr, key == 'S' ? "START" : key == 'T' ? "STOP" : "RES");
            if (key == 'E') c->overflow_count = 0;
            printf("Sent %s\n", key == 'S' ? "START" : key == 'T' ? "STOP" : "RES");
            dc_show_errors(slot, 0);
            break;
        case 'G':
            dc_get(addr);
            printf("GET sent (DT %s)\n", dc_dt[c->dt_mode]);
            dc_show_errors(slot, 0);
            break;
        case 'U':
            dc_autotrig(slot);
            break;
        case 'Q':
            dc_show_settings(slot);
            break;
        case 'P':
            dc_utilities(slot);
            wait = 0;
            break;
        case '0':
        case 27:
            return;
        default:
            wait = 0;
        }
        if (wait) {
            printf("\nPress any key...");
            getch();
        }
    }
}

void configure_dc5009_advanced(int slot) { dc50xx_menu(slot); }
void configure_dc5010_advanced(int slot) { dc50xx_menu(slot); }

/* ========================================================================== */
/* FG5010 Function Generator (manual 070-3467-01, section 3)                  */
/* ========================================================================== */

#ifdef GRIDGPIB
#include "gridgpib.h"
#endif

static const char* __far fg5010_funcs[] = { "SINE", "SQUARE", "TRIANGLE" };
/* MODE? answers LOCK for PHLOCK (manual p. 3-14) */
static const char* __far fg5010_modes[] = { "CONT", "TRIG", "GATE", "BURST", "PHLOCK" };
static const char* __far fg5010_dts[] = { "OFF", "SET", "TRIG", "GATE" };

/* ON/OFF settings; bit n of cfg->toggles is fg5010_toggles[n] */
#define FG5010_COMP 0x01
#define FG5010_AM   0x02
#define FG5010_FM   0x04
#define FG5010_VCF  0x08
#define FG5010_HOLD 0x10
#define FG5010_GATE 0x20
static const char* __far fg5010_toggles[] = {
    "COMP", "AM", "FM", "VCF", "HOLD", "GATE", "PLI", "USER"
};
static const char* __far fg5010_toggle_help[] = {
    "invert waveform (p. 3-7)", "AM from AM INPUT (p. 3-6)",
    "FM from FM/VCF INPUT (p. 3-9)", "VCF from FM/VCF INPUT (p. 3-25)",
    "hold output voltage, 200 Hz max (p. 3-11)", "gate on, MODE GATE (p. 3-10)",
    "SRQ on phase lock change (p. 3-17)", "SRQ on INST ID button (p. 3-24)"
};

/* Other settings with a query form; with fg5010_toggles, "query one setting" */
static const char* __far fg5010_queries[] = {
    "FUNC", "MODE", "SLOPE", "FREQ", "AMPL", "OFFS", "SYM", "PHAS", "NBUR", "OUT",
    "DT", "RQS"
};

/* Numeric settings: SET? header, prompt, range, power-on value (p. 3-33).
   Index order is fg5010_get_num's. */
static const struct { const char *hdr, *prompt; float lo, hi, def; } __far fg5010_nums[] = {
    { "FREQ",  "Frequency in Hz (0.002 to 20E6, 0 with VCF, p. 3-9)", 0.002, 20.0e6, 1000.0 },
    { "AMPL",  "Amplitude, V p-p open circuit (0, 0.02 to 20, p. 3-6)", 0.02, 20.0, 0.5 },
    { "OFFS",  "Offset, V open circuit (-7.5 to 7.5, p. 3-15)", -7.5, 7.5, 0.0 },
    { "SYM",   "Symmetry, % (10 to 90, p. 3-21)", 10.0, 90.0, 50.0 },
    { "PHASE", "Phase, degrees (-90 to 90, p. 1-6)", -90.0, 90.0, 0.0 },
    { "NBUR",  "Cycles per burst (1 to 9999, p. 3-15)", 1.0, 9999.0, 10.0 }
};

static float fg5010_get_num(fg5010_config *cfg, int n) {
    switch (n) {
        case 0:  return cfg->frequency;
        case 1:  return cfg->amplitude;
        case 2:  return cfg->offset;
        case 3:  return cfg->duty_cycle;
        case 4:  return cfg->phase;
        default: return cfg->burst_count;
    }
}

/* SYM, PHAS and NBUR take whole numbers (1 % and 1 degree resolution) */
static void fg5010_set_num(fg5010_config *cfg, int n, float v) {
    if (n >= 3) v = (int)(v + (v < 0 ? -0.5 : 0.5));
    switch (n) {
        case 0:  cfg->frequency = v; break;
        case 1:  cfg->amplitude = v; break;
        case 2:  cfg->offset = v; break;
        case 3:  cfg->duty_cycle = v; break;
        case 4:  cfg->phase = v; break;
        default: cfg->burst_count = (int)v; break;
    }
}

/* Why value n is refused with the slot's other settings, or NULL */
static const char *fg5010_num_error(fg5010_config *cfg, int n, float v) {
    float ampl, offs, sym, freq;

    if (n >= 3) v = (int)(v + (v < 0 ? -0.5 : 0.5));   /* as fg5010_set_num stores it */
    ampl = n == 1 ? v : cfg->amplitude;
    offs = n == 2 ? v : cfg->offset;
    sym = n == 3 ? v : cfg->duty_cycle;
    freq = n == 0 ? v : cfg->frequency;
    if (!(v >= fg5010_nums[n].lo && v <= fg5010_nums[n].hi) &&
        !(v == 0.0 && (n == 1 || (n == 0 && (cfg->toggles & FG5010_VCF)))))  /* AMPL 0; FREQ 0 in VCF */
        return "out of range";
    if ((n == 1 || n == 2) && ampl / 2.0 + fabs(offs) > 15.0)
        return "peak plus offset over 15 V (p. 3-6)";
    /* Each triangle ramp must last 25 ns or more: 10-90 % up to 4 MHz, only
       50 % at 20 MHz (p. 3-21, Fig. 3-2) */
    if ((n == 0 || n == 3) && (sym < 50.0 ? sym : 100.0 - sym) + 0.01 < freq * 2.5e-6)
        return "symmetry too far from 50 % at this frequency (error 251, Fig. 3-2)";
    return NULL;
}

/* ERR? codes, manual Table 3-1 (p. 3-31, 3-32); 340-394 show only on the
   front panel */
static const struct { int code; const char *text; } __far fg5010_errors[] = {
    {   0, "No errors or events" },
    { 101, "Invalid command header" },
    { 102, "Header delimiter error" },
    { 103, "Argument error" },
    { 104, "Argument delimiter error" },
    { 106, "Missing argument" },
    { 107, "Invalid message unit delimiter" },
    { 108, "Binary block checksum error" },
    { 109, "Binary block byte-count error" },
    { 201, "Not executable in local state" },
    { 202, "Settings lost because of rtl" },
    { 203, "I/O buffers full, output dumped" },
    { 205, "Argument out of range" },
    { 206, "GET ignored" },
    { 251, "Frequency-symmetry conflict" },
    { 252, "Amplitude-offset conflict" },
    { 254, "Hold-phase lock mode conflict" },
    { 255, "Frequency-hold mode conflict" },
    { 256, "FM-phase lock mode conflict" },
    { 257, "VCF-phase lock mode conflict" },
    { 258, "Gate-mode conflict" },
    /* Internal errors: also TEST's result codes (p. 3-23) */
    { 301, "Interrupt fault" },
    { 302, "System error" },
    { 311, "Period measurement failed to complete" },
    { 312, "Period measurement overflow" },
    { 313, "Shift register (VIA) failed to function" },
    { 314, "Mag-latch relay strobe interrupt failed" },
    { 315, "Phase lock status over and under range at once" },
    { 316, "Automatic frequency correction range exceeded" },
    { 320, "VIA fault on CPU board" },
    { 321, "Trig/gate control error on CPU board" },
    { 322, "4 MHz reference clock or counter fault" },
    { 323, "Frequency control logic fault, Loop 2 board" },
    { 324, "Loop cycle counter fault, Loop 2 board" },
    { 325, "Frequency prescaler fault, Loop 2 board" },
    { 326, "Low-frequency generator fault" },
    { 327, "No signal from Loop 1 board" },
    { 328, "Inadequate frequency range (2 kHz range)" },
    { 329, "Inadequate frequency range (20 kHz range)" },
    { 330, "Inadequate frequency range (200 kHz range)" },
    { 331, "Inadequate frequency range (2 MHz range)" },
    { 332, "Inadequate frequency range (20 MHz range)" },
    { 333, "Burst counter fault" },
    { 401, "Power on" },
    { 403, "User request (INST ID button)" },
    { 731, "Went out of phase lock" },
    { 732, "Went into phase lock" }
};

static const char *fg5010_error_text(int code) {
    int i;
    for (i = 0; i < sizeof(fg5010_errors) / sizeof(fg5010_errors[0]); i++)
        if (fg5010_errors[i].code == code) return fg5010_errors[i].text;
    if (code >= 300 && code < 400) return "Internal error (hardware or firmware)";
    return "Unknown code";
}

/* SET? runs to about 200 characters (p. 3-19); a reply longer than the read
   would be left in the driver and misread as the next one */
static char __far fg5010_reply[256];

/* Send a query and read its reply into fg5010_reply, CR/LF removed. 0 if there
   was none: a rejected query leaves only the one-byte all-ones "no output"
   message (p. 3-29), which must not pass for an answer. */
static int fg5010_query(int address, const char *query) {
    int n;

    fg5010_reply[0] = '\0';
    if (gpib_write(address, (char *)query) != 0) return 0;
    GPIB_PACE(50);
    n = gpib_read(address, fg5010_reply, sizeof(fg5010_reply));
    if (n <= 0 || (unsigned char)fg5010_reply[0] == 0xFF) {
        fg5010_reply[0] = '\0';
        return 0;
    }
    fg5010_reply[strcspn(fg5010_reply, "\r\n")] = '\0';
    return 1;
}

/* Value text of a header in a reply such as "FREQ 1.0E+3;AMPL 500.0E-3;...":
   the header must start the reply or follow a ';'. NULL if absent. */
static const char *fg5010_field(const char *reply, const char *hdr) {
    int n = strlen(hdr);
    const char *p = reply;

    while (p) {
        while (*p == ';' || *p == ' ') p++;
        if (strncmp(p, hdr, n) == 0 && p[n] == ' ') return p + n + 1;
        p = strchr(p, ';');
    }
    return NULL;
}

static int fg5010_on(const char *reply, const char *hdr) {
    const char *p = fg5010_field(reply, hdr);
    return p && strncmp(p, "ON", 2) == 0;
}

/* Query, then the number after the reply's header ("AMPL 2.000;") */
static int fg5010_query_value(int address, const char *query, const char *hdr, float *v) {
    const char *p;
    return fg5010_query(address, query) && (p = fg5010_field(fg5010_reply, hdr)) != NULL &&
           sscanf(p, "%f", v) == 1;
}

/* Copy the word at p (up to ';') */
static void fg5010_word(const char *p, char *w, int size) {
    int i = 0;
    if (p) while (i < size - 1 && p[i] && p[i] != ';' && p[i] != ' ') { w[i] = p[i]; i++; }
    w[i] = '\0';
}

/* ERR? code, -1 if there was no reply. With RQS ON it only explains the last
   serial-polled status byte, so callers turn RQS off first (p. 3-8). */
static int fg5010_query_error(int address) {
    float code;
    return fg5010_query_value(address, "ERR?", "ERR", &code) ? (int)code : -1;
}

/* Print ERR? until it reports 0 (p. 3-32). Returns the number of errors, not
   counting the 400/700 events (power on, INST ID, phase lock), or -1 if ERR?
   went unanswered. */
static int fg5010_show_errors(int address) {
    int i, code, errors = 0;

    for (i = 0; i < 16; i++) {
        code = fg5010_query_error(address);
        if (code < 0) {
            printf("  No reply to ERR?\n");
            return -1;
        }
        if (code == 0) break;
        printf("  ERR %d: %s\n", code, fg5010_error_text(code));
        if (code < 400) errors++;
    }
    return errors;
}

/* REMOTE (in local every setting is error 201, p. 3-30), then RQS OFF so
   errors queue for ERR? instead of each raising SRQ. Returns RQS to restore. */
static int fg5010_begin(int address) {
    int rqs_was_on;

    gpib_remote(address);
    GPIB_PACE(100);
    rqs_was_on = !fg5010_query(address, "RQS?") || fg5010_on(fg5010_reply, "RQS");
    gpib_write(address, "RQS OFF");
    GPIB_PACE(20);
    return rqs_was_on;
}

/* Call after the error queue is drained: RQS ON raises SRQ for every event
   still unreported (p. 3-18) */
static void fg5010_end(int address, int rqs_was_on) {
    if (rqs_was_on) gpib_write(address, "RQS ON");
    GPIB_PACE(20);
}

/* One command with its errors read back; 1 if the FG5010 reported none */
static int fg5010_send_checked(int address, const char *cmd) {
    int rqs = fg5010_begin(address);
    int errors;

    gpib_write(address, (char *)cmd);
    GPIB_PACE(60);  /* longest settling, recall: 58 ms (Table 1-6, p. 1-9) */
    errors = fg5010_show_errors(address);
    fg5010_end(address, rqs);
    return errors == 0;
}

/* Power-on settings, manual p. 3-33 */
void init_fg5010_config(int slot) {
    fg5010_config *cfg;
    int n;

    if (slot < 0 || slot >= 10) return;
    cfg = &g_fg5010_config[slot];
    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->waveform, "SINE");
    strcpy(cfg->trigger_slope, "POS");
    for (n = 0; n < 6; n++) fg5010_set_num(cfg, n, fg5010_nums[n].def);
    cfg->layout = FG5010_LAYOUT;
}

/* Into range after an older profile or a hand-edited .cfg. Older layouts kept
   sweep/trigger/modulation fields the FG5010 lacks: their mode and flag bits
   mean nothing here and are dropped. */
void fg5010_fix_config(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    int i;

    if (cfg->layout != FG5010_LAYOUT) {
        cfg->mode = 0;
        cfg->toggles = 0;
        cfg->dt_mode = 0;
        cfg->reserved = 0;
        cfg->layout = FG5010_LAYOUT;
    }
    for (i = 0; i < 3 && strcmp(cfg->waveform, fg5010_funcs[i]) != 0; i++) ;
    if (i == 3) strcpy(cfg->waveform, "SINE");
    if (strcmp(cfg->trigger_slope, "NEG") != 0) strcpy(cfg->trigger_slope, "POS");
    for (i = 0; i < 6; i++) {
        float v = fg5010_get_num(cfg, i);
        fg5010_set_num(cfg, i, fg5010_num_error(cfg, i, v) ? fg5010_nums[i].def : v);
    }
    if (cfg->mode < 0 || cfg->mode > 4) cfg->mode = 0;
}

/* Programmed state from SET?, one line for the monitor and single measurement
   (out holds 60). No printf: the monitor loop calls this. 1 if it answered. */
int fg5010_state_text(int address, char *out) {
    const char *p;
    float f, a = 0.0, o = 0.0;
    char func[4], mode[6], relay[4];

    if (!fg5010_query(address, "SET?") || (p = fg5010_field(fg5010_reply, "FREQ")) == NULL ||
        sscanf(p, "%f", &f) != 1) {
        strcpy(out, "No reply to SET?");
        return 0;
    }
    if ((p = fg5010_field(fg5010_reply, "AMPL")) != NULL) sscanf(p, "%f", &a);
    if ((p = fg5010_field(fg5010_reply, "OFFS")) != NULL) sscanf(p, "%f", &o);
    fg5010_word(fg5010_field(fg5010_reply, "FUNC"), func, sizeof(func));
    fg5010_word(fg5010_field(fg5010_reply, "MODE"), mode, sizeof(mode));
    fg5010_word(fg5010_field(fg5010_reply, "OUT"), relay, sizeof(relay));
    if (f >= 1.0e6)      sprintf(out, "%8.4f MHz", f / 1.0e6);
    else if (f >= 1.0e3) sprintf(out, "%8.3f kHz", f / 1.0e3);
    else                 sprintf(out, "%8.3f Hz ", f);
    out[12] = '\0';     /* a garbled reply must not run past the 60 bytes */
    snprintf(out + strlen(out), 60 - strlen(out), " %6.3fVpp %+5.2fV %-3s %-5s OUT %s",
             a, o, func, mode, relay);
    return 1;
}

/* Send the slot's settings and read back the errors. OUT is never sent: the
   relay moves only on the menu's own Output item. */
static void fg5010_apply(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    int i, rqs, on, errors;
    char msg[100];

    printf("Applying settings to the FG5010 (the output relay is left as it is)...\n");
    rqs = fg5010_begin(address);
    /* Under DT SET new settings wait for GET (p. 3-8): off while these go in,
       the slot's DT goes last */
    gpib_write(address, "DT OFF");

    /* Each group is one message: the FG5010 checks a message's settings together,
       so FREQ/SYM and AMPL/OFFS need no particular order, and an error discards
       only that message (p. 3-28). OFFs go first: FM, VCF and HOLD block PHLOCK,
       HOLD blocks FREQ over 200 Hz. VCF goes off in any case: VCF mode allows no
       range change, and VCF ON takes its top frequency from the range FREQ
       selected (p. 3-25). */
    for (on = 0; on <= 1; on++) {
        if (on) {
            sprintf(msg, "MODE %s;SLOPE %s;%s", fg5010_modes[cfg->mode], cfg->trigger_slope,
                    cfg->waveform);         /* FUNC header is optional (p. 3-10) */
            gpib_write(address, msg);
            /* NR2 (fixed point) throughout: 5 decimals hold 4 digits down to 0.002 Hz.
               FREQ 0 is legal only once VCF is on: sent after the toggles. */
            if (cfg->frequency == 0.0)
                sprintf(msg, "SYM %d", (int)cfg->duty_cycle);
            else
                sprintf(msg, "FREQ %.5f;SYM %d", cfg->frequency, (int)cfg->duty_cycle);
            gpib_write(address, msg);
            sprintf(msg, "AMPL %.4f;OFFS %.2f", cfg->amplitude, cfg->offset);
            gpib_write(address, msg);
            sprintf(msg, "PHAS %d;NBUR %d", (int)cfg->phase, cfg->burst_count);
            gpib_write(address, msg);
        }
        msg[0] = '\0';
        for (i = 0; i < 8; i++) {
            if (!(cfg->toggles & (1 << i)) == !on || (!on && (1 << i) == FG5010_VCF)) {
                if (msg[0]) strcat(msg, ";");
                strcat(msg, fg5010_toggles[i]);
                strcat(msg, on ? " ON" : " OFF");
            }
        }
        if (msg[0]) gpib_write(address, msg);
    }
    if (cfg->frequency == 0.0) gpib_write(address, "FREQ 0");  /* VCF is on by now */
    sprintf(msg, "DT %s", fg5010_dts[cfg->dt_mode]);
    gpib_write(address, msg);
    GPIB_PACE(60);  /* longest settling, recall: 58 ms (Table 1-6, p. 1-9) */

    errors = fg5010_show_errors(address);
    fg5010_end(address, rqs);
    if (errors == 0) printf("Settings applied\n");
    else if (errors > 0) printf("The FG5010 reported the errors above\n");
}

/* Load the slot's settings from the FG5010's SET? reply */
static void fg5010_read_back(int slot) {
    fg5010_config *cfg = &g_fg5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    char word[10];
    const char *p;
    int i, col = 0;
    float v;

    if (!fg5010_query(address, "SET?")) {
        printf("No reply to SET?\n");
        return;
    }
    /* One query response per ';', three to a line */
    for (p = fg5010_reply; *p; ) {
        int n = strcspn(p, ";");
        if (n > 0) {
            printf("%-26.*s", n > 25 ? 25 : n, p);
            if (++col % 3 == 0) printf("\n");
        }
        p += n;
        if (*p == ';') p++;
    }
    if (col % 3) printf("\n");

    printf("\nCopy these into the slot's settings (Y/N)? ");
    if (toupper(getch()) != 'Y') {
        printf("\n");
        return;
    }
    /* SET? spells out PHASE (p. 3-19) */
    for (i = 0; i < 6; i++)
        if ((p = fg5010_field(fg5010_reply, fg5010_nums[i].hdr)) && sscanf(p, "%f", &v) == 1)
            fg5010_set_num(cfg, i, v);
    fg5010_word(fg5010_field(fg5010_reply, "FUNC"), word, sizeof(word));
    if (word[0]) strcpy(cfg->waveform, word);
    fg5010_word(fg5010_field(fg5010_reply, "SLOPE"), word, sizeof(cfg->trigger_slope));
    if (word[0]) strcpy(cfg->trigger_slope, word);
    fg5010_word(fg5010_field(fg5010_reply, "MODE"), word, sizeof(word));
    for (i = 0; i < 5; i++)
        if (strcmp(word, fg5010_modes[i]) == 0 || (i == 4 && strcmp(word, "LOCK") == 0)) cfg->mode = i;
    fg5010_word(fg5010_field(fg5010_reply, "DT"), word, sizeof(word));
    for (i = 0; i < 4; i++)
        if (strcmp(word, fg5010_dts[i]) == 0) cfg->dt_mode = i;
    for (i = 0; i < 8; i++) {
        if (fg5010_field(fg5010_reply, fg5010_toggles[i]) == NULL) continue;
        if (fg5010_on(fg5010_reply, fg5010_toggles[i])) cfg->toggles |= 1 << i;
        else cfg->toggles &= ~(1 << i);
    }
    fg5010_fix_config(slot);
    printf("\nSettings copied\n");
}

/* After anything that can move the relay: never guess its state */
static void fg5010_show_relay(int address) {
    if (fg5010_query(address, "OUT?") && fg5010_field(fg5010_reply, "OUT"))
        printf("Output relay is now %s\n", fg5010_on(fg5010_reply, "OUT") ? "ON" : "OFF");
    else
        printf("No reply to OUT?: check the OUTPUT ON light\n");
}

/* Show and optionally switch the output relay: the only item that sends OUT */
static void fg5010_output(int address) {
    int on, rqs;

    if (!fg5010_query(address, "OUT?") || fg5010_field(fg5010_reply, "OUT") == NULL) {
        printf("No reply to OUT?\n");
        return;
    }
    on = fg5010_on(fg5010_reply, "OUT");
    printf("Output relay is %s.\n", on ? "ON (closed)" : "OFF (open)");
    if (on) printf("Open it (OUT OFF)? (Y/N) ");
    else printf("Close it (OUT ON)? The generator then drives whatever is\nconnected to OUTPUT. (Y/N) ");
    if (toupper(getch()) != 'Y') {
        printf("\nNot changed\n");
        return;
    }
    printf("\n");
    rqs = fg5010_begin(address);
    gpib_write(address, on ? "OUT OFF" : "OUT ON");
    GPIB_PACE(20);  /* relay: under 8 ms (Table 1-6, p. 1-9) */
    fg5010_show_errors(address);
    fg5010_end(address, rqs);
    fg5010_show_relay(address);
}

/* TEST's run time is not in the manual: allow its reply 30 s */
static void fg5010_long_timeout(int on) {
#ifdef GRIDGPIB
    static int old;
    if (on) old = gg_timeout(30000u);
    else if (old > 0) gg_timeout((unsigned)old);
#else
    ieee_write(on ? "TIME OUT 30\r\n" : "TIME OUT 10\r\n");
#endif
}

/* Binary block replies "LLSET %<count><data><checksum>;" and "STORE n:%...;"
   (p. 3-4, 3-12, 3-18), held for writing back with LLSET or STOR n:. The
   first 8 bytes stay free for that command's header. blk_at is the '%'
   offset, -1 unless a whole block with a good checksum is held. */
static unsigned char __far fg5010_blk[512];
static int fg5010_blk_at = -1, fg5010_blk_len, fg5010_blk_llset;

static void fg5010_dump_block(int address, const char *cmd) {
    unsigned char *b = fg5010_blk + 8;
    int n, i, start, count, sum;
#ifdef GRIDGPIB
    unsigned got;
    int endr;
#endif

    fg5010_blk_at = -1;
    if (gpib_write(address, (char *)cmd) != 0) {
        printf("Not sent\n");
        return;
    }
    GPIB_PACE(50);
#ifdef GRIDGPIB
    /* Raw up to EOI: the data may hold LF bytes */
    n = gg_read(address, b, sizeof(fg5010_blk) - 9, &got, &endr, 0) == GG_OK ? (int)got : 0;
#else
    n = gpib_read(address, (char *)b, sizeof(fg5010_blk) - 8);
#endif
    if (n <= 0 || b[0] == 0xFF) {
        printf("No reply\n");
        return;
    }
    for (start = 0; start < n && b[start] != '%'; start++) ;
    printf("Reply header: %.*s\n", start, (char *)b);
    if (start + 3 > n) {
        printf("No binary block in the reply\n");
        return;
    }
    /* Count: data bytes + checksum, MSB first; the checksum makes the sum of
       count, data and checksum bytes 0 modulo 256 */
    count = (b[start + 1] << 8) | b[start + 2];
    sum = b[start + 1] + b[start + 2];
    for (i = 0; i < count && start + 3 + i < n; i++) sum += b[start + 3 + i];
    printf("%d data bytes", count - 1);
    if (i < count) {
        printf(", %d received: the read ended early (an LF byte in the data)\n", i);
    } else {
        printf(", checksum %s\n", (sum & 0xFF) == 0 ? "OK" : "BAD");
        if ((sum & 0xFF) == 0) {
            fg5010_blk_at = 8 + start;
            fg5010_blk_len = 3 + count;
            fg5010_blk_llset = (cmd[0] == 'L');
        }
    }
    for (n = 0; n < i; n++) printf("%02X%s", b[start + 3 + n], (n % 16 == 15) ? "\n" : " ");
    printf("\n");
    if (fg5010_blk_at >= 0) printf("Held: W writes it back\n");
}

/* A location 0-9 for STOR/REC/SEND, -1 if none */
static int fg5010_ask_location(void) {
    int c;
    printf("Location (0-9): ");
    c = getch();
    printf("%c\n", isprint(c) ? c : ' ');
    return (c >= '0' && c <= '9') ? c - '0' : -1;
}

/* The held block back as LLSET <binblk> (every setting, the relay too) or
   STOR n:<binblk>. Only blocks this FG5010 sent are written (CAUTION, p. 3-12,
   3-21). With the LF/EOI terminator switch an LF byte in the block ends the
   message early: the FG5010 then reports error 109 and changes nothing. */
static void fg5010_write_block(int address) {
#ifdef GRIDGPIB
    char hdr[8];
    unsigned char *p;
    int loc, plen, rqs;

    if (fg5010_blk_at < 0) {
        printf("No block held: read one with L or S first\n");
        return;
    }
    if (fg5010_blk_llset) {
        printf("LLSET sets every setting, the output relay included. Send (Y/N)? ");
        if (toupper(getch()) != 'Y') return;
        strcpy(hdr, "LLSET ");
    } else {
        if ((loc = fg5010_ask_location()) < 0) return;
        printf("Overwrite stored setup %d with the held block (Y/N)? ", loc);
        if (toupper(getch()) != 'Y') return;
        sprintf(hdr, "STOR %d:", loc);
    }
    printf("\n");
    plen = strlen(hdr);
    p = fg5010_blk + fg5010_blk_at - plen;
    memcpy(p, hdr, plen);
    p[plen + fg5010_blk_len] = ';';
    rqs = fg5010_begin(address);
    if (gg_write(address, p, plen + fg5010_blk_len + 1, 1) != GG_OK) printf("Not sent\n");
    if (fg5010_show_errors(address) == 0) printf("Block accepted\n");
    fg5010_end(address, rqs);
    if (fg5010_blk_llset) fg5010_show_relay(address);
#else
    printf("Writing a block back needs tm5000g.exe (GRIDGPIB): Driver488 OUTPUT\n");
    printf("sends text, and a block can hold NUL, CR and LF bytes.\n");
#endif
}

/* ID, self test, error queue, single queries, status, display, INIT, setups */
static void fg5010_utilities_menu(int address) {
    static const char* __far disp_args[] = { "FREQ", "AMPL", "OFFS", "NBURST", "PHASE", "SYM" };
    static const char* __far lock_text[] = { "phase lock mode not selected",
        "phase lock mode, not locked", "phase lock mode, locked" };
    static const char* __far trig_text[] = { "trigger status not available",
        "input below threshold", "input toggling faster than 3 Hz", "input above threshold" };
    char cmd[16];
    int choice, done = 0, loc, i;
    float v;

    while (!done) {
        clrscr();
        printf("FG5010 Utilities - GPIB %d\n", address);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. Self test (TEST) - no output while it runs\n");
        printf("3. Error and event queue (ERR?)\n");
        printf("4. Query one setting\n");
        printf("5. Phase lock and trigger input status (LOCK?, TRIG?)\n");
        printf("6. Front-panel display (DISP)\n");
        printf("7. INIT - power-on settings, output relay open\n");
        printf("8. Store the present settings (STOR 0-9)\n");
        printf("9. Recall stored settings (REC 0-9) - can switch the output\n");
        printf("L. Low-level settings block (LLSET?)\n");
        printf("S. Stored settings block (SEND 0-9)\n");
        printf("W. Write the block from L or S back (LLSET, or STOR 0-9)\n");
        printf("0. Return\n\nSelect option: ");
        choice = toupper(getch());
        printf("\n\n");

        switch (choice) {
            case '1':
                printf("%s\n", fg5010_query(address, "ID?") ? fg5010_reply : "No reply");
                break;
            case '2':
                printf("TEST cuts the signal while it runs, then restores the settings\n");
                printf("(p. 3-23). Run it (Y/N)? ");
                if (toupper(getch()) != 'Y') break;
                printf("\nRunning TEST (ROM, serial I/O and AFC tests)...\n");
                fg5010_long_timeout(1);
                i = fg5010_query_value(address, "TEST", "TEST", &v);
                fg5010_long_timeout(0);
                if (!i) printf("No reply\n");
                else if ((int)v == 0) printf("TEST 0: self test passed\n");
                else printf("TEST %d: %s\n", (int)v, fg5010_error_text((int)v));
                break;
            case '3':
                i = fg5010_begin(address);
                if (fg5010_show_errors(address) == 0) printf("No errors pending\n");
                fg5010_end(address, i);
                break;
            case '4':
                for (i = 0; i < 20; i++)
                    printf("%c. %-6s%s", 'A' + i, i < 12 ? fg5010_queries[i] : fg5010_toggles[i - 12],
                           i % 5 == 4 ? "\n" : "  ");
                printf("\nQuery: ");
                i = toupper(getch()) - 'A';
                printf("\n");
                if (i < 0 || i >= 20) break;
                sprintf(cmd, "%s?", i < 12 ? fg5010_queries[i] : fg5010_toggles[i - 12]);
                printf("%s -> %s\n", cmd, fg5010_query(address, cmd) ? fg5010_reply : "No reply");
                break;
            case '5':
                if (fg5010_query_value(address, "LOCK?", "LOCK", &v) && v >= -1 && v <= 1)
                    printf("LOCK %d: %s\n", (int)v, lock_text[(int)v + 1]);
                else
                    printf("No reply to LOCK?\n");
                if (fg5010_query_value(address, "TRIG?", "TRIG", &v) && v >= 0 && v <= 3)
                    printf("TRIG %d: %s\n", (int)v, trig_text[(int)v]);
                else
                    printf("No reply to TRIG?\n");
                break;
            case '6':
                printf("1. FREQ  2. AMPL  3. OFFS  4. NBURST  5. PHASE  6. SYM\nShow: ");
                i = getch() - '1';
                printf("\n");
                if (i < 0 || i > 5) break;
                sprintf(cmd, "DISP %s", disp_args[i]);
                if (fg5010_send_checked(address, cmd)) printf("Display shows %s\n", disp_args[i]);
                break;
            case '7':
                printf("INIT returns every setting except the stored setups to its power-on\n");
                printf("value (p. 3-12, 3-33) and opens the output relay. Continue (Y/N)? ");
                if (toupper(getch()) != 'Y') break;
                printf("\n");
                /* INIT puts RQS back ON (p. 3-33); off again so its errors queue */
                if (fg5010_send_checked(address, "INIT;RQS OFF"))
                    printf("FG5010 at its power-on settings. A in the setup menu re-applies TM5000's.\n");
                break;
            case '8':
                if ((loc = fg5010_ask_location()) < 0) break;
                printf("Overwrite stored setup %d with the present settings (Y/N)? ", loc);
                if (toupper(getch()) != 'Y') break;
                printf("\n");
                sprintf(cmd, "STOR %d", loc);
                if (fg5010_send_checked(address, cmd)) printf("Stored in %d (kept until power-off)\n", loc);
                break;
            case '9':
                if ((loc = fg5010_ask_location()) < 0) break;
                /* A setup holds the OUT state (STOR leaves out only step size, DT, PLI,
                   RQS and USER, p. 3-21), so recalling can close the relay */
                printf("Recall setup %d? It includes the output relay state (Y/N) ", loc);
                if (toupper(getch()) != 'Y') break;
                printf("\n");
                sprintf(cmd, "REC %d", loc);
                fg5010_send_checked(address, cmd);
                fg5010_show_relay(address);
                break;
            case 'L':
                fg5010_dump_block(address, "LLSET?");
                break;
            case 'S':
                if ((loc = fg5010_ask_location()) < 0) break;
                sprintf(cmd, "SEND %d", loc);
                fg5010_dump_block(address, cmd);
                break;
            case 'W':
                fg5010_write_block(address);
                break;
            case '0':
            case 27:
                done = 1;
                break;
        }
        if (!done) {
            printf("\nPress any key...");
            getch();
        }
    }
}

/* Prompt for a number; 0 if the entry was not one */
static int fg5010_ask(const char *prompt, float *v) {
    char tok[24];

    printf("%s: ", prompt);
    /* Leading space skips the newline an earlier scanf left behind */
    if (scanf(" %23s", tok) != 1) return 0;
    if (sscanf(tok, "%f", v) == 1) return 1;
    printf("Not a number\n");
    return 0;
}

/* Every FG5010 setting, kept in the slot and sent with Apply; the output relay,
   triggers and setups act at once and ask first where they change the output. */
void configure_fg5010_advanced(int slot) {
    fg5010_config *cfg;
    int address;
    int choice, i, done = 0, pause;
    float v;
    char cmd[16];
    const char *msg;

    if (slot < 0 || slot >= 10) return;
    cfg = &g_fg5010_config[slot];
    address = g_system->modules[slot].gpib_address;
    fg5010_fix_config(slot);

    while (!done) {
        clrscr();
        printf("FG5010 Function Generator - Slot %d (GPIB %d)\n", slot, address);
        printf("=============================================\n\n");
        printf("1. Function:  %-10s          7. Mode:  %s\n", cfg->waveform, fg5010_modes[cfg->mode]);
        printf("2. Frequency: %-12.6g Hz       8. Slope: %s\n", cfg->frequency, cfg->trigger_slope);
        printf("3. Amplitude: %.4g V p-p open circuit (%.4g V p-p into 50 ohm)\n",
               cfg->amplitude, cfg->amplitude / 2.0);
        printf("4. Offset:    %+.2f V open circuit\n", cfg->offset);
        printf("5. Symmetry:  %d %%                  9. Burst count (NBUR): %d\n",
               (int)cfg->duty_cycle, cfg->burst_count);
        printf("6. Phase:     %+d deg               D. GET response (DT): %s\n",
               (int)cfg->phase, fg5010_dts[cfg->dt_mode]);
        printf("T. On/off:");
        for (i = 0; i < 8; i++)
            printf(" %s %s", fg5010_toggles[i], (cfg->toggles & (1 << i)) ? "ON" : "off");
        printf("\n\nA. Apply settings to the FG5010   R. Read settings back (SET?)\n");
        printf("O. Output relay ON/OFF (asks)     M. Manual trigger (MTRIG)\n");
        printf("G. Send GET (needs DT TRIG/GATE)  U. Utilities (ID, test, setups...)\n");
        printf("C. Communication test             0. Return\n");
        printf("\nSettings go to the generator only with A; A never switches the relay.\n");
        printf("\nSelect option: ");
        choice = toupper(getch());
        printf("\n\n");
        pause = 1;

        switch (choice) {
            case '1':
                printf("1. SINE  2. SQUARE  3. TRIANGLE: ");
                i = getch() - '1';
                if (i >= 0 && i < 3) strcpy(cfg->waveform, fg5010_funcs[i]);
                pause = 0;
                break;

            case '2': case '3': case '4': case '5': case '6': case '9':
                i = choice == '9' ? 5 : choice - '2';
                if (!fg5010_ask(fg5010_nums[i].prompt, &v)) break;
                if ((msg = fg5010_num_error(cfg, i, v)) != NULL) {
                    printf("Not set: %s\n", msg);
                    break;
                }
                fg5010_set_num(cfg, i, v);
                if (i == 0 && (cfg->toggles & FG5010_HOLD) && v > 200.0)
                    printf("HOLD is on: the FG5010 refuses over 200 Hz (error 255)\n");
                else if (i == 0 && v == 0.0)
                    printf("VCF then sweeps from 0 Hz up to the top of the range the FG5010\n"
                           "is in when Apply turns VCF on (p. 3-25).\n");
                else if (i == 4)
                    printf("PHLOCK: offset from the TRIG/GATE input. TRIG/GATE/BURST: start of\n"
                           "the first cycle. Above 1 MHz the range narrows (p. 1-6).\n");
                else pause = 0;
                break;

            case '7':
                printf("1. CONT  2. TRIG  3. GATE  4. BURST  5. PHLOCK: ");
                i = getch() - '1';
                printf("\n");
                if (i < 0 || i > 4) break;
                cfg->mode = i;
                /* Leaving GATE mode turns the gate off (p. 3-10) */
                if (i != 2) cfg->toggles &= ~FG5010_GATE;
                if (i == 4 && (cfg->toggles & (FG5010_FM | FG5010_VCF | FG5010_HOLD)))
                    printf("PHLOCK refuses FM, VCF and HOLD (p. 3-14): turn them off\n");
                else pause = 0;
                break;

            case '8':
                strcpy(cfg->trigger_slope, strcmp(cfg->trigger_slope, "POS") == 0 ? "NEG" : "POS");
                pause = 0;
                break;

            case 'D':
                printf("Response to GET (p. 3-8):\n");
                printf("0. OFF   ignored\n");
                printf("1. SET   hold new settings until GET\n");
                printf("2. TRIG  one cycle (TRIG) or a burst (BURST) per GET\n");
                printf("3. GATE  toggle the gate (GATE mode)\nSelect: ");
                i = getch() - '0';
                if (i >= 0 && i <= 3) cfg->dt_mode = i;
                pause = 0;
                break;

            case 'T':
                for (i = 0; i < 8; i++)
                    printf("%d. %-4s %-3s %s\n", i + 1, fg5010_toggles[i],
                           (cfg->toggles & (1 << i)) ? "ON" : "off", fg5010_toggle_help[i]);
                printf("Toggle: ");
                i = getch() - '1';
                printf("\n");
                if (i < 0 || i > 7) break;
                cfg->toggles ^= 1 << i;
                /* FM ON turns VCF off and VCF ON turns FM off (p. 3-9, 3-25) */
                if ((cfg->toggles & (1 << i)) && i == 2) cfg->toggles &= ~FG5010_VCF;
                if ((cfg->toggles & (1 << i)) && i == 3) cfg->toggles &= ~FG5010_FM;
                if (!(cfg->toggles & FG5010_VCF) && cfg->frequency == 0.0) cfg->frequency = 1000.0;
                if ((cfg->toggles & FG5010_GATE) && cfg->mode != 2) {
                    cfg->toggles &= ~FG5010_GATE;
                    printf("GATE ON needs MODE GATE (p. 3-10)\n");
                } else if ((cfg->toggles & FG5010_HOLD) && (cfg->frequency > 200.0 || cfg->mode == 4)) {
                    printf("HOLD needs 200 Hz or less and no PHLOCK (p. 3-11)\n");
                } else if (cfg->mode == 4 && (cfg->toggles & (FG5010_FM | FG5010_VCF))) {
                    printf("PHLOCK refuses FM and VCF (p. 3-9, 3-25)\n");
                } else {
                    pause = 0;
                }
                break;

            case 'A':
                fg5010_apply(slot);
                break;

            case 'R':
                fg5010_read_back(slot);
                break;

            case 'O':
                fg5010_output(address);
                break;

            case 'M':
                /* One cycle in TRIG, NBUR cycles in BURST (p. 3-14); MAN is the same command */
                printf("MTRIG: one cycle in TRIG mode, a burst in BURST mode. Send (Y/N)? ");
                if (toupper(getch()) != 'Y') break;
                printf("\n");
                if (fg5010_send_checked(address, "MTRIG")) printf("Triggered\n");
                break;

            case 'G':
                printf("GET acts as DT is set on the FG5010 (p. 3-8). Send (Y/N)? ");
                if (toupper(getch()) != 'Y') break;
                printf("\n");
                i = fg5010_begin(address);
                sprintf(cmd, "TRIGGER %d\r\n", address);
                ieee_write(cmd);
                GPIB_PACE(60);
                if (fg5010_show_errors(address) == 0) printf("GET sent\n");
                fg5010_end(address, i);
                break;

            case 'U':
                fg5010_utilities_menu(address);
                pause = 0;
                break;

            case 'C':
                test_fg5010_comm(address);
                pause = 0;
                break;

            case '0':
            case 27:
                done = 1;
                pause = 0;
                break;

            default:
                pause = 0;
                break;
        }
        if (pause) {
            printf("\nPress any key to continue...");
            getch();
        }
    }
}

/* ID, settings round trip, output relay and error queue. Asks before INIT and
   the relay; whatever happens the relay is opened again and checked. */
int test_fg5010_comm(int address) {
    int failed = 0, rqs, i, k, off_ok, skip;
    float v;
    void (*old_sigint)(int);
    void (*old_sigbreak)(int);

    clrscr();
    printf("FG5010 Function Generator Communication Test\n");
    printf("============================================\n\n");
    printf("Testing FG5010 at GPIB address %d...\n\n", address);

    printf("Test 1: Identification (ID?)... ");
    if (!fg5010_query(address, "ID?") || strncmp(fg5010_reply, "ID TEK/FG5010", 13) != 0) {
        printf("FAILED\n   %s\n", fg5010_reply[0] ? fg5010_reply : "No reply");
        printf("\nNo FG5010 answered: nothing else was sent.\n");
        printf("Press any key to continue...");
        getch();
        return -1;
    }
    printf("PASSED\n   %s\n\n", fg5010_reply);

    /* The test overwrites the user's settings and drives whatever is wired to the output */
    printf("WARNING: this test sends INIT (all settings return to power-on\n");
    printf("values), sets 1 kHz SINE at 2 V p-p open circuit (1 V p-p into\n");
    printf("50 ohm), closes the output relay briefly and opens it again.\n");
    printf("Continue? (y/n): ");
    if (toupper(getch()) != 'Y') {
        printf("\n\nFG5010 test not run.\n");
        printf("Press any key to continue...");
        getch();
        return 1;
    }
    printf("\n\n");

    rqs = fg5010_begin(address);
    printf("Test 2: Earlier events (ERR?)...\n");
    i = fg5010_show_errors(address);
    if (i < 0) {
        failed = 1;
    } else {
        printf("   %s\n", i ? "Earlier errors cleared" : "PASSED");
    }

    printf("\nTest 3: INIT, then 1 kHz, 2 V p-p, SINE... ");
    /* INIT puts RQS back ON (p. 3-33); off again in the same message */
    gpib_write(address, "INIT;RQS OFF");
    GPIB_PACE(100);
    gpib_write(address, "FREQ 1000;AMPL 2.0;FUNC SINE");
    GPIB_PACE(60);  /* frequency with range change: under 50 ms (Table 1-6, p. 1-9) */
    i = fg5010_query_value(address, "FREQ?", "FREQ", &v) && fabs(v - 1000.0) < 1.0;
    printf("\n   FREQ? %s", fg5010_reply[0] ? fg5010_reply : "no reply");
    if (!(fg5010_query_value(address, "AMPL?", "AMPL", &v) && fabs(v - 2.0) < 0.01)) i = 0;
    printf("\n   AMPL? %s", fg5010_reply[0] ? fg5010_reply : "no reply");
    if (!(fg5010_query(address, "FUNC?") && strncmp(fg5010_reply, "FUNC SINE", 9) == 0)) i = 0;
    printf("\n   FUNC? %s", fg5010_reply[0] ? fg5010_reply : "no reply");
    if (!(fg5010_query(address, "OUT?") && strncmp(fg5010_reply, "OUT OFF", 7) == 0)) i = 0;
    printf("\n   OUT?  %s\n", fg5010_reply[0] ? fg5010_reply : "no reply");
    printf("   %s\n", i ? "PASSED" : "FAILED");

    /* Settings not confirmed: the relay is not closed on an unknown waveform,
       but OUT OFF is still sent and checked below */
    printf("\nTest 4: Output relay (OUT ON, OUT OFF)... ");
    skip = !i;
    /* Ctrl-C/Ctrl-Break here would exit with the output live */
    old_sigint = signal(SIGINT, SIG_IGN);
    old_sigbreak = signal(SIGBREAK, SIG_IGN);
    if (!skip) {
        gpib_write(address, "OUT ON");
        GPIB_PACE(20);  /* relay: under 8 ms (Table 1-6, p. 1-9) */
        i = fg5010_query(address, "OUT?") && fg5010_on(fg5010_reply, "OUT");
    }
    /* Open it whatever OUT? said, and confirm: a failed read does not mean OUT ON
       was ignored */
    off_ok = 0;
    for (k = 0; k < 2 && !off_ok; k++) {
        gpib_write(address, "OUT OFF");
        GPIB_PACE(20);
        off_ok = fg5010_query(address, "OUT?") && strncmp(fg5010_reply, "OUT OFF", 7) == 0;
    }
    signal(SIGINT, old_sigint);
    signal(SIGBREAK, old_sigbreak);
    printf("%s\n", skip ? "SKIPPED (OUT OFF sent)" : i && off_ok ? "PASSED" : "FAILED");
    if (!i || !off_ok) failed = 1;
    if (!off_ok) {
        printf("\n*** OUTPUT MAY STILL BE ON: turn it off at the FG5010 front panel. ***\n");
        printf("Press any key to continue...");
        getch();
    }

    printf("\nTest 5: Errors from this test (ERR?)...\n");
    i = fg5010_show_errors(address);
    if (i != 0) failed = 1;
    else printf("   PASSED\n");
    fg5010_end(address, rqs);

    printf("\n============================================\n");
    if (!failed) {
        printf("ALL TESTS PASSED - FG5010 communication OK\n");
    } else {
        printf("SOME TESTS FAILED - Check connections and settings\n");
    }
    printf("============================================\n\n");

    printf("Press any key to continue...");
    getch();

    return failed ? -1 : 0;
}

/* True if slot holds a computed (FFT/math) trace: flagged, or MOD_NONE with data.
   The derived case survives a .tm5/.cfg reload, which drops is_result. (review #1/#2/#10) */
int module_is_result(int slot) {
    return g_system->modules[slot].is_result ||
           (g_system->modules[slot].module_type == MOD_NONE &&
            g_system->modules[slot].module_data &&
            g_system->modules[slot].module_data_count > 0);
}

/* Disable "phantom" slots: enabled but no type, bad address, or no description */
void validate_enabled_modules(void) {
    int i;
    for (i = 0; i < 10; i++) {
        /* Computed traces are legitimately enabled with no GPIB address */
        if (g_system->modules[i].enabled && !module_is_result(i)) {
            if (g_system->modules[i].module_type == MOD_NONE ||
                g_system->modules[i].gpib_address < 1 ||
                g_system->modules[i].gpib_address > 30 ||
                strlen(g_system->modules[i].description) == 0) {

                g_system->modules[i].enabled = 0;
                g_system->modules[i].module_type = MOD_NONE;
                g_system->modules[i].is_result = 0;
                g_system->modules[i].gpib_address = 0;
                strcpy(g_system->modules[i].description, "");
                free_module_buffer(i);
                forget_trace_slot(i);
            }
        }
    }
}

/* ---- Auto-detect modules from a GPIB bus scan ----------------------- */
/* Indexed by MOD_xxx */
static const char *auto_models[] = { "", "DC5009", "DM5010", "DM5120", "PS5004",
                                     "PS5010", "DC5010", "FG5010" };

/* MOD_xxx for an instrument ID such as "ID TEK/DC5009,V79.1,F1.2" */
static int model_to_type(const char *id) {
    int t;
    for (t = MOD_DC5009; t <= MOD_FG5010; t++)
        if (strstr(id, auto_models[t])) return t;
    return MOD_NONE;
}

/* Set up one slot: the manual configuration's steps, without the prompts */
static void setup_module(int slot, int module_type, int address, const char *desc) {
    tm5000_module *m = &g_system->modules[slot];
    m->enabled = 1;
    m->module_type = module_type;
    m->is_result = 0;
    m->slot_number = slot;
    m->gpib_address = address;
    m->last_reading = 0.0;
    strncpy(m->description, desc, sizeof(m->description) - 1);
    m->description[sizeof(m->description) - 1] = '\0';
    switch (module_type) {
        case MOD_DC5009: init_dc5009_config(slot); break;
        case MOD_DM5010: init_dm5010_config(slot); break;
        case MOD_DM5120: init_dm5120_config(slot); init_dm5120_config_enhanced(slot); break;
        case MOD_PS5004: init_ps5004_config(slot); ps5004_clear_srq(address); break;
        case MOD_PS5010: init_ps5010_config(slot); break;
        case MOD_DC5010: init_dc5010_config(slot); break;
        case MOD_FG5010: init_fg5010_config(slot); break;
    }
    gpib_remote(address);
    GPIB_PACE(200);
    allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE);
}

/* Scan GPIB addresses 1-30 (skipping the controller at 21), identify the
 * instruments and add unconfigured TM5000 modules to free slots.  Existing
 * slots are never changed; a type mismatch is only reported. */
void auto_detect_modules(void) {
    static struct { unsigned char addr, type, slot, action; char id[36]; } f[30];
    char id[80];
    int addr, i, k, n = 0, n_new = 0, used[10];
    char *p;
    
    clrscr();
    printf("Auto-detect modules\n");
    printf("===================\n\n");
    printf("Scanning GPIB addresses 1-30 (controller = 21)  ESC cancels\n");
    for (addr = 1; addr <= 30; addr++) {
        if (addr == 21) continue;
        printf("\r  address %2d ", addr);
        fflush(stdout);
        if (kbhit() && getch() == 27) {
            printf("\nCancelled.  Press any key...");
            getch();
            return;
        }
        if (!gpib_probe(addr, id, sizeof(id))) continue;
        for (p = id; *p; p++) if (*p == '\r' || *p == '\n') { *p = '\0'; break; }
        f[n].addr = (unsigned char)addr;
        f[n].type = (unsigned char)model_to_type(id);
        strncpy(f[n].id, id, sizeof(f[n].id) - 1);
        f[n].id[sizeof(f[n].id) - 1] = '\0';
        n++;
    }
    printf("\r                \n");
    if (n == 0) {
        printf("No instruments answered.  Is the GPIB driver loaded and the bus\n");
        printf("cable connected?\n\nPress any key...");
        getch();
        return;
    }
    
    /* action: 0 = not a TM5000 module / no free slot, 1 = already set up,
       2 = that address's slot is set up as another type (left alone),
       3 = add to a free slot */
    for (i = 0; i < 10; i++)
        used[i] = g_system->modules[i].enabled || g_system->modules[i].is_result;
    for (k = 0; k < n; k++) {
        f[k].action = 0;
        f[k].slot = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && g_system->modules[i].gpib_address == f[k].addr) {
                f[k].slot = (unsigned char)i;
                f[k].action = (g_system->modules[i].module_type == f[k].type) ? 1 : 2;
                break;
            }
        }
        if (i == 10 && f[k].type != MOD_NONE) {
            for (i = 0; i < 10 && used[i]; i++) ;
            if (i < 10) { used[i] = 1; f[k].slot = (unsigned char)i; f[k].action = 3; n_new++; }
        }
    }
    
    printf("Addr  Instrument                            TM5000\n");
    printf("----  ------------------------------------  ----------------------\n");
    for (k = 0; k < n; k++) {
        printf(" %2d   %-36s  ", f[k].addr, f[k].id[0] ? f[k].id : "(answers, no ID reply)");
        switch (f[k].action) {
            case 1:  printf("slot %d, already set up\n", f[k].slot); break;
            case 2:  printf("slot %d set up as other\n", f[k].slot); break;
            case 3:  printf("-> slot %d (new)\n", f[k].slot); break;
            default: printf(f[k].type != MOD_NONE ? "no free slot\n" : "not a TM5000 module\n");
        }
    }
    if (n_new == 0) {
        printf("\nNothing new to add.  Press any key...");
        getch();
        return;
    }
    printf("\nAdd %d module(s) as shown? (Y/N): ", n_new);
    if (toupper(getch()) != 'Y') {
        printf("N\nNothing changed.  Press any key...");
        getch();
        return;
    }
    printf("Y\n");
    for (k = 0; k < n; k++) {
        if (f[k].action != 3) continue;
        printf("  slot %d: %s at GPIB %d\n", f[k].slot, auto_models[f[k].type], f[k].addr);
        setup_module(f[k].slot, f[k].type, f[k].addr, auto_models[f[k].type]);
    }
    printf("\nDone - use 0-9 to change a slot's settings.  Press any key...");
    getch();
}

/* Slot setup menu: module type, GPIB address, description; A runs auto-detect */
void configure_modules(void) {
    int choice, slot, address, module_type;
    int done = 0;
    char description[20];
    
    while (!done) {
        clrscr();
        printf("Module Configuration\n");
        printf("====================\n\n");
        
        printf("Current Configuration:\n");
        for (slot = 0; slot < 10; slot++) {
            printf("Slot %d: ", slot);
            if (g_system->modules[slot].enabled) {
                printf("%s (Type %d) at GPIB %d\n", 
                       g_system->modules[slot].description,
                       g_system->modules[slot].module_type,
                       g_system->modules[slot].gpib_address);
            } else {
                printf("Empty\n");
            }
        }
        
        printf("\nOptions:\n");
        printf("0-9: Configure slot\n");
        printf("A:   Auto-detect modules on the bus\n");
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = getch();
        
        if (choice == 27) {  /* ESC */
            done = 1;
        } else if (choice == 'A' || choice == 'a') {
            auto_detect_modules();
        } else if (choice >= '0' && choice <= '9') {
            slot = choice - '0';
            
            printf("\n\nConfiguring Slot %d\n", slot);
            printf("==================\n\n");
            
            printf("Module Types:\n");
            printf("0 = Empty\n");
            printf("1 = DC5009 Universal Counter\n");
            printf("2 = DM5010 Multimeter\n");
            printf("3 = DM5120 Multimeter\n");
            printf("4 = PS5004 Power Supply\n");
            printf("5 = PS5010 Dual Power Supply\n");
            printf("6 = DC5010 Universal Counter\n");
            printf("7 = FG5010 Function Generator\n");
            printf("\nEnter module type: ");
            scanf("%d", &module_type);
            
            if (module_type == 0) {
                /* Remove module */
                g_system->modules[slot].enabled = 0;
                g_system->modules[slot].module_type = MOD_NONE;
                g_system->modules[slot].is_result = 0;
                g_system->modules[slot].gpib_address = 0;
                strcpy(g_system->modules[slot].description, "");
                free_module_buffer(slot);
                forget_trace_slot(slot);
            } else if (module_type >= 1 && module_type <= 7) {
                printf("Enter GPIB address (1-30): ");
                scanf("%d", &address);
                
                if (address >= 1 && address <= 30) {
                    /* Check for address conflicts */
                    int conflict = 0;
                    int i;
                    for (i = 0; i < 10; i++) {
                        if (i != slot && g_system->modules[i].enabled && 
                            g_system->modules[i].gpib_address == address) {
                            printf("\nError: Address %d already used by slot %d!\n", address, i);
                            conflict = 1;
                            break;
                        }
                    }
                    
                    if (!conflict) {
                        printf("Enter description: ");
                        scanf("%s", description);

                        g_system->modules[slot].enabled = 1;
                        g_system->modules[slot].module_type = module_type;
                        g_system->modules[slot].is_result = 0;
                        g_system->modules[slot].slot_number = slot;
                        g_system->modules[slot].gpib_address = address;
                        g_system->modules[slot].last_reading = 0.0;
                        strcpy(g_system->modules[slot].description, description);

                        switch (module_type) {
                            case MOD_DC5009:
                                init_dc5009_config(slot);
                                break;
                            case MOD_DM5010:
                                init_dm5010_config(slot);
                                break;
                            case MOD_DM5120:
                                init_dm5120_config(slot);
                                init_dm5120_config_enhanced(slot);
                                break;
                            case MOD_PS5004:
                                init_ps5004_config(slot);
                                break;
                            case MOD_PS5010:
                                init_ps5010_config(slot);
                                break;
                            case MOD_DC5010:
                                init_dc5010_config(slot);
                                break;
                            case MOD_FG5010:
                                init_fg5010_config(slot);
                                break;
                        }
                        
                        printf("Initializing device at GPIB address %d...\n", 
                               g_system->modules[slot].gpib_address);
                        
                        gpib_remote(g_system->modules[slot].gpib_address);
                        GPIB_PACE(200);
                        
                        /* LF-termination prompt is DM5120-only, as in v2.9 */
                        if (module_type == MOD_DM5120) {
                            delay(300);
                            
                            printf("\nUse Line Feed termination? (Y/N): ");
                            fflush(stdout);
                            {
                                char input_char = getch();
                                printf("%c\n", input_char);
                                if (input_char == 'Y' || input_char == 'y') {
                                    g_dm5120_config[slot].lf_termination = 1;
                                    printf("Line Feed termination enabled.\n");
                                } else {
                                    g_dm5120_config[slot].lf_termination = 0;
                                    printf("Standard CRLF termination enabled.\n");
                                }
                            }
                        }
                        
                        allocate_module_buffer(slot, MAX_SAMPLES_PER_MODULE);

                        printf("\nTest communication? (y/n): ");
                        if (toupper(getch()) == 'Y') {
                            printf("\nTesting communication...\n");
                            
                            gpib_remote(address);
                            GPIB_PACE(200);
                            
                            switch (module_type) {
                                case MOD_DC5009:
                                case MOD_DC5010:
                                    dc50xx_test(slot);
                                    break;
                                case MOD_DM5010:
                                    test_dm5010_comm(address);
                                    break;
                                case MOD_FG5010:
                                    test_fg5010_comm(address);
                                    break;
                                case MOD_DM5120:
                                    test_dm5120_comm(address);
                                    break;
                                case MOD_PS5004:
                                    test_ps5004_comm(address);
                                    break;
                                case MOD_PS5010:
                                    test_ps5010_comm(address);
                                    break;
                            }
                        }
                        
                        printf("\nModule configured successfully!\n");
                    }
                } else {
                    printf("\nInvalid address! Must be 1-30.\n");
                }
            } else {
                printf("\nInvalid module type!\n");
            }
            
            printf("Press any key to continue...");
            getch();
        }
    }
}

void display_trace_selection_menu(void) {
    int choice, done = 0;
    int i, active_traces = 0;
    
    while (!done) {
        clrscr();
        printf("Trace Selection for Display\n");
        printf("===========================\n\n");
        
        printf("Available Traces:\n");
        active_traces = 0;
        for (i = 0; i < 10; i++) {
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                printf("%d: %s (%d points) - %s\n", i, 
                       g_traces[i].label,
                       g_traces[i].data_count,
                       g_traces[i].enabled ? "ACTIVE" : "inactive");
                if (g_traces[i].enabled) active_traces++;
            }
        }
        
        printf("\nActive traces: %d\n", active_traces);
        
        printf("\nOptions:\n");
        printf("0-9: Toggle trace display\n");
        printf("A: Show All traces\n");
        printf("N: Show No traces\n");
        printf("ESC: Exit\n\n");
        printf("Choice: ");
        
        choice = toupper(getch());
        
        if (choice == 27) {  /* ESC */
            done = 1;
        } else if (choice >= '0' && choice <= '9') {
            i = choice - '0';
            if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                g_traces[i].enabled = !g_traces[i].enabled;
            }
        } else if (choice == 'A') {
            for (i = 0; i < 10; i++) {
                if (g_system->modules[i].enabled && g_traces[i].data_count > 0) {
                    g_traces[i].enabled = 1;
                }
            }
        } else if (choice == 'N') {
            for (i = 0; i < 10; i++) {
                g_traces[i].enabled = 0;
            }
        }
    }
}

/* A slot's trace is shown the first time it has data; after that the user's choice is
   kept, even across a sync where its buffer was freed (pool resize). A slot that gets a
   new occupant (module removed, file loaded) must be forgotten with forget_trace_slot. */
static unsigned char shown_before[10];
static unsigned char hidden_no_data[10];  /* enabled was forced 0, not the user's choice */
static unsigned char user_enabled[10];

void forget_trace_slot(int slot) {
    if (slot < 0 || slot >= 10) return;
    shown_before[slot] = 0;
    hidden_no_data[slot] = 0;
    user_enabled[slot] = 0;
}

void sync_traces_with_modules(void) {
    int i;

    for (i = 0; i < 10; i++) {
        if (shown_before[i] && !hidden_no_data[i]) {
            user_enabled[i] = g_traces[i].enabled;
        }
        g_traces[i].data = NULL;
        g_traces[i].data_count = 0;
    }
    
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            g_traces[i].slot = i;
            g_traces[i].color = get_module_color(g_system->modules[i].module_type);
            strcpy(g_traces[i].label, g_system->modules[i].description);
            
            if (g_system->modules[i].module_data && 
                g_system->modules[i].module_data_count > 0) {
                g_traces[i].data = g_system->modules[i].module_data;
                g_traces[i].data_count = g_system->modules[i].module_data_count;
                
                if (module_is_result(i)) {
                    /* Computed trace: keep the unit_type/x_scale/x_offset set at creation. (review #3) */
                } else {
                    switch (g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            /* Hz, s or a count, by the counter's function */
                            g_traces[i].unit_type = dc50xx_trace_unit(i);
                            g_traces[i].x_scale = 1.0;  /* Sample-based for counters */
                            g_traces[i].x_offset = 0.0;
                            break;
                        case MOD_DM5120:
                            {
                                const char *u = dm5120_unit(i);
                                g_traces[i].unit_type = u[0] == 'd' ? UNIT_DB :
                                                        u[0] == 'o' ? UNIT_RESISTANCE :
                                                        u[0] == 'A' ? UNIT_CURRENT : UNIT_VOLTAGE;
                            }
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                        case MOD_DM5010:
                            {
                                const char *u = dm5010_unit(i);
                                g_traces[i].unit_type = u[0] == 'd' ? UNIT_DB :
                                                        u[0] == 'o' ? UNIT_RESISTANCE : UNIT_VOLTAGE;
                            }
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                        case MOD_PS5004:
                            /* SEND gives amperes in the current and limit displays (manual p. 3-12) */
                            g_traces[i].unit_type = g_ps5004_config[i].display_mode == 1 ||
                                                    g_ps5004_config[i].display_mode == 2 ? UNIT_CURRENT : UNIT_VOLTAGE;
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                        default:
                            /* Default to voltage for multimeters and power supplies */
                            g_traces[i].unit_type = UNIT_VOLTAGE;
                            g_traces[i].x_scale = 1.0;
                            g_traces[i].x_offset = 0.0;
                            break;
                    }
                }
                
                if (!shown_before[i]) {
                    shown_before[i] = 1;
                    user_enabled[i] = 1;
                }
                g_traces[i].enabled = user_enabled[i];
                hidden_no_data[i] = 0;
            } else {
                g_traces[i].enabled = 0;
                hidden_no_data[i] = 1;
            }
        } else {
            g_traces[i].enabled = 0;
            shown_before[i] = 0;
            hidden_no_data[i] = 0;
        }
    }
}

/* DM5120 multimeter */
void init_dm5120_config(int slot) {
    strcpy(g_dm5120_config[slot].function, "DCV");
    g_dm5120_config[slot].range_mode = 0;  /* AUTO */
    g_dm5120_config[slot].filter_enabled = 0;
    g_dm5120_config[slot].filter_value = 10;
    g_dm5120_config[slot].trigger_source = 0;  /* TALK, the manual's recommended default */
    g_dm5120_config[slot].trigger_mode = 0;  /* CONT */
    g_dm5120_config[slot].digits = 5;
    g_dm5120_config[slot].nullval = 0.0;
    g_dm5120_config[slot].null_enabled = 0;
    g_dm5120_config[slot].data_format = 1;  /* Scientific notation */
    g_dm5120_config[slot].buffer_enabled = 0;
    g_dm5120_config[slot].buffer_size = DM5120_DEFAULT_BUFFER_SIZE;  /* half the 500-reading hardware limit */
    g_dm5120_config[slot].min_max_enabled = 0;
    g_dm5120_config[slot].min_value = 1e30;
    g_dm5120_config[slot].max_value = -1e30;
    g_dm5120_config[slot].sample_count = 0;
    g_dm5120_config[slot].burst_mode = 0;
    g_dm5120_config[slot].sample_rate = 1.0;
    g_dm5120_config[slot].lf_termination = 0;  /* Default to CRLF */
    g_dm5120_config[slot].autocal_off = 0;
    g_dm5120_config[slot].intfilt_off = 0;
    g_dm5120_config[slot].buffer_state = 0;
}
void init_dm5120_config_enhanced(int slot) {
    init_dm5120_config(slot);
    g_dm5120_config[slot].buffer_enabled = 1;
    g_dm5120_config[slot].buffer_size = DM5120_MAX_BUFFER_SIZE;  /* hardware limit */
    g_dm5120_config[slot].min_max_enabled = 1;
    g_dm5120_config[slot].burst_mode = 1;
    g_dm5120_config[slot].sample_rate = 10.0;  /* 10 readings/sec */
}
/* Settings an older menu could save: DCI/ACI, which the meter rejects, and trigger
   choices 2 (TALK) / 3 (EXT) stored in the CONT/ONE field. Also drops a fill state
   a profile captured mid-fill. */
void dm5120_fix_legacy_config(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (strcmp(cfg->function, "DCI") == 0) strcpy(cfg->function, "DCA");
    else if (strcmp(cfg->function, "ACI") == 0) strcpy(cfg->function, "ACA");
    if (cfg->trigger_mode == 2 || cfg->trigger_mode == 3) {
        cfg->trigger_source = cfg->trigger_mode - 2;
        cfg->trigger_mode = 0;
    } else if (cfg->trigger_mode != 0) {
        cfg->trigger_mode = 1;
    }
    cfg->buffer_state = 0;
}

void dm5120_set_function(int address, char *function) {
    sprintf(gpib_cmd_buffer, "FUNCT %s", function);
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(100);
}
void dm5120_set_range(int address, int range) {
    if (range == 0) {
        gpib_write_dm5120(address, "RANGE AUTO");
    } else {
        sprintf(gpib_cmd_buffer, "RANGE %d", range);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(50);
}
void dm5120_set_filter(int address, int enabled, int value) {

    if (enabled) {
        /* FILTER only takes ON/OFF; the count is FILTERVAL, stored per function */
        if (value > 0 && value <= 99) {
            sprintf(gpib_cmd_buffer, "FILTERVAL %d", value);
            gpib_write_dm5120(address, gpib_cmd_buffer);
            GPIB_PACE(50);
        }
        gpib_write_dm5120(address, "FILTER ON");
        GPIB_PACE(50);
    } else {
        gpib_write_dm5120(address, "FILTER OFF");
    }
}
void dm5120_set_trigger(int address, char *source, char *mode) {

    if (!source || !mode) {
        printf("ERROR: dm5120_set_trigger requires both source and mode\n");
        return;
    }
    
    /* Validate source (TALK or EXT) */
    if (strcmp(source, "TALK") != 0 && strcmp(source, "EXT") != 0) {
        printf("ERROR: Invalid trigger source '%s'. Must be TALK or EXT\n", source);
        return;
    }
    
    /* Validate mode (CONT or ONE) */
    if (strcmp(mode, "CONT") != 0 && strcmp(mode, "ONE") != 0) {
        printf("ERROR: Invalid trigger mode '%s'. Must be CONT or ONE\n", mode);
        return;
    }
    
    sprintf(gpib_cmd_buffer, "TRIGGER %s,%s", source, mode);  /* manual syntax: TRIGGER <source>,<mode> */
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void dm5120_set_digits(int address, int digits) {
    if (digits >= 3 && digits <= 6) {
        /* DIGIT, not DIGITS: the DM5120 accepts no other spelling (error 101) */
        sprintf(gpib_cmd_buffer, "DIGIT %d", digits);
        gpib_write_dm5120(address, gpib_cmd_buffer);
        GPIB_PACE(50);
    }
}
void dm5120_set_null(int address, int enabled, float value) {
    if (enabled) {
        /* NULLVAL, then ON: "NULL <value>" with a value of 0 turns null off */
        sprintf(gpib_cmd_buffer, "NULLVAL %.6e", value);
        gpib_write_dm5120(address, gpib_cmd_buffer);
        GPIB_PACE(50);
        gpib_write_dm5120(address, "NULL ON");
    } else {
        gpib_write_dm5120(address, "NULL OFF");
    }
    delay(50);
}
void dm5120_set_data_format(int address, int on) {
    gpib_write_dm5120(address, on ? "DATFOR ON" : "DATFOR OFF");
    GPIB_PACE(50);
}
void dm5120_set_autocal(int address, int on) {
    gpib_write_dm5120(address, on ? "AUTOCAL ON" : "AUTOCAL OFF");
    GPIB_PACE(50);
}
void dm5120_set_intfilt(int address, int on) {
    gpib_write_dm5120(address, on ? "INTFILT ON" : "INTFILT OFF");
    GPIB_PACE(50);
}

/* Value from a DM5120 reply. Query replies always carry a header ("BUFCNT 45;"),
   readings start with the number ("+1.234567E+0:NDCV:000;"). */
int dm5120_parse_value(const char *reply, float *value) {
    while (*reply && (isalpha((unsigned char)*reply) || isspace((unsigned char)*reply))) reply++;
    return sscanf(reply, "%f", value) == 1;
}

/* Send a query and parse the number in its reply; 0 if there was none */
static int dm5120_query_value(int address, char *query, float *value) {
    gpib_write_dm5120(address, query);
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) <= 0) return 0;
    return dm5120_parse_value(gpib_response_buffer, value);
}

/* ERROR?/EVENT? codes, manual Table 3-5 */
static const struct { int code; const char *text; } __far dm5120_errors[] = {
    {   0, "No error or event" },
    { 101, "Command header error" },
    { 102, "Header delimiter error" },
    { 103, "Command argument error" },
    { 104, "Argument delimiter error" },
    { 106, "Missing argument" },
    { 107, "Invalid message unit delimiter" },
    { 201, "Not executable in local" },
    { 202, "Settings lost due to RTL" },
    { 204, "Settings conflict" },
    { 205, "Argument out of range" },
    { 250, "Invalid RANGE argument" },
    { 251, "Invalid DIGIT argument" },
    { 252, "Invalid FILTERVAL argument" },
    { 253, "Invalid ZEROVAL argument" },
    { 254, "Invalid BUFSZ argument" },
    { 255, "Invalid STOINT argument" },
    { 256, "Invalid DELAY argument" },
    { 257, "TEXT argument too long" },
    { 258, "Invalid KEY argument" },
    { 259, "Calibration out of range" },
    { 260, "CAL locked" },
    { 401, "Power on" },
    { 402, "Operation complete" },
    { 403, "User request (ID key)" },
    { 450, "Buffer full" },
    { 451, "Buffer half full" },
    { 452, "Ready for conversion" },
    { 453, "EEPROM failure" },
    { 454, "Overflow" },
    { 551, "STOINT shorter than conversion time" },
    { 650, "Trigger during a conversion" }
};

const char *dm5120_error_text(int code) {
    int i;
    for (i = 0; i < sizeof(dm5120_errors) / sizeof(dm5120_errors[0]); i++)
        if (dm5120_errors[i].code == code) return dm5120_errors[i].text;
    return "Unknown code";
}

/* ERROR? code, or -1 if the meter did not answer */
int dm5120_query_error(int address) {
    float v;
    return dm5120_query_value(address, "ERROR?", &v) ? (int)v : -1;
}

/* Unit of the slot's function, for display */
const char *dm5120_unit(int slot) {
    const char *f = g_dm5120_config[slot].function;
    if (strstr(f, "DB")) return "dB";
    if (strstr(f, "OHMS")) return "ohm";
    if (f[2] == 'A') return "A";   /* DCA, ACA */
    return "V";
}
/* DM5120 reading rates (readings/s), manual p. 1-9: [3.5..6.5 digits][AutoCal off, on].
   5.5/6.5 digits use the 50 Hz figures: the line frequency (PROGRAM 32) can't be
   queried and they are the slower. 6.5 digits is listed with AutoCal on only. */

/* DCV, DCA, ACV, ACA rates - Continuous Into Internal Buffer */
static const float __far dm5120_voltage_current_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {1000.0, 1000.0},  /* 3.5 digit */
    {333.0, 333.0},    /* 4.5 digit */
    {29.0, 7.5},       /* 5.5 digit */
    {7.5, 7.5}         /* 6.5 digit */
};

/* OHMS rates - Continuous Into Internal Buffer */
static const float __far dm5120_ohms_rates[4][2] = {
    /* AutoCal OFF, AutoCal ON */
    {40.0, 24.0},      /* 3.5 digit */
    {38.0, 18.0},      /* 4.5 digit */
    {13.0, 7.5},       /* 5.5 digit */
    {7.5, 7.5}         /* 6.5 digit */
};

/* IEEE-488 Bus triggered rates (slower) */
static const float __far dm5120_bus_voltage_current_rates[4][2] = {
    {72.0, 45.0},      /* 3.5 digit */
    {63.0, 41.0},      /* 4.5 digit */
    {26.0, 7.5},       /* 5.5 digit */
    {0.2, 0.2}         /* 6.5 digit */
};

static const float __far dm5120_bus_ohms_rates[4][2] = {
    {32.0, 19.0},      /* 3.5 digit */
    {29.0, 16.0},      /* 4.5 digit */
    {12.5, 7.5},       /* 5.5 digit */
    {0.25, 0.25}       /* 6.5 digit */
};

/* Digits the meter actually converts at (manual Table 3-1): dB, OHMSCOMP and the
   top ohms ranges never go below 5.5. Rounded up where the table is unclear. */
static int dm5120_effective_digits(dm5120_config *cfg) {
    int d = (cfg->digits >= 3 && cfg->digits <= 6) ? cfg->digits : 4;
    if (d < 5) {
        if (strstr(cfg->function, "DB") || strstr(cfg->function, "OHMSCOMP")) d = 5;
        else if (strstr(cfg->function, "OHMS") && (cfg->range_mode == 0 || cfg->range_mode >= 5)) d = 5;
    }
    return d;
}

static float dm5120_table_rate(dm5120_config *cfg, int use_bus_rates) {
    int digit_index = dm5120_effective_digits(cfg) - 3;
    int autocal_index = cfg->autocal_off ? 0 : 1;
    int is_ohms_function = (strstr(cfg->function, "OHMS") != NULL);
    float rate;

    if (use_bus_rates) {
        rate = is_ohms_function ? dm5120_bus_ohms_rates[digit_index][autocal_index]
                                : dm5120_bus_voltage_current_rates[digit_index][autocal_index];
    } else {
        rate = is_ohms_function ? dm5120_ohms_rates[digit_index][autocal_index]
                                : dm5120_voltage_current_rates[digit_index][autocal_index];
    }
    if (strstr(cfg->function, "OHMSCOMP") != NULL) {
        rate *= 0.5; /* Offset compensated ohms is 0.5x normal rate */
    }
    if (rate < 0.1) rate = 0.1;  /* callers divide by rate */
    return rate;
}

/* Readings/s for the slot's DM5120 config; trigger_mode 2 forces bus-triggered rates */
float dm5120_get_measurement_rate(int slot, int trigger_mode) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int use_bus_rates;

    /* Bus-triggered vs continuous rates, per DM5120 manual:
     * - EXT,CONT with buffer enabled requires external trigger (bus rates)
     * - EXT,ONE always requires external trigger (bus rates)
     * - TALK mode always uses bus rates (triggered by GPIB addressing)
     * - EXT,CONT with buffer disabled uses continuous rates (auto-trigger)
     */
    use_bus_rates = (trigger_mode == 2) || /* Explicit bus-triggered mode */
                    (cfg->trigger_source == 1 && (cfg->trigger_mode == 1 || cfg->buffer_enabled)) || /* EXT,ONE or EXT,CONT with buffer */
                    (cfg->trigger_source == 0); /* TALK mode always uses bus rates */

    return dm5120_table_rate(cfg, use_bus_rates);
}

/* Shortest STOINT (ms) the slot's settings allow. Below one conversion into the
   buffer the meter stops with error 551 (SHORT TIME); +30% because the rates are
   nominal (6.5 digits with AutoCal gives 174 ms, near the factory STOINT 175).
   1-14 ms and 1-2 ms are the manual's settings conflicts (error 204). */
int dm5120_min_stoint_ms(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int ms = (int)(1300.0 / dm5120_table_rate(cfg, 0)) + 1;

    if (ms <= 14 && (strstr(cfg->function, "OHMS") || strstr(cfg->function, "DB") ||
                     cfg->range_mode == 0 || cfg->buffer_size == 0 || cfg->digits >= 5)) {
        ms = 15;
    }
    if (ms <= 2 && cfg->digits >= 4) ms = 3;
    return ms;
}

/* Delay in ms for a DM5120 operation, derived from the reading rate */
int dm5120_calculate_measurement_time(int slot, int operation_type, int sample_count) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int base_time_ms;
    int trigger_mode = cfg->trigger_mode;

    measurement_rate = dm5120_get_measurement_rate(slot, trigger_mode);

    base_time_ms = (int)(1000.0 / measurement_rate);

    base_time_ms = (base_time_ms * 150) / 100;  /* +50% for GPIB overhead and system lag */

    switch (operation_type) {
        case 0: /* Single measurement */
            return base_time_ms;

        case 1: /* Buffer setup (BUFSZ command) */
            return base_time_ms + 200; /* setup overhead */

        case 2: /* Buffer data retrieval (READ ALLSTORE) */
            return (base_time_ms * sample_count) + 500;  /* fill time + GPIB/setup lag */
            
        case 3: /* STOINT setup */
            return 50; /* Quick command response */
            
        case 4: /* Buffer status query (BUFCNT?, BUFAVE?, etc.) */
            return 50;
            
        default:
            return base_time_ms;
    }
}

/* Alias of dm5120_calculate_measurement_time */
int dm5120_calculate_delay(int slot, int operation_type, int sample_count) {
    return dm5120_calculate_measurement_time(slot, operation_type, sample_count);
}

void dm5120_enable_buffering(int address, int slot, int buffer_size) {
    
    /* DM5120 internal buffer holds 500 readings */
    if (buffer_size > 500) {
        buffer_size = 500;
    }

    if (buffer_size == 0) {
        /* 0 selects circular mode */
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", buffer_size);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }

    delay(dm5120_calculate_delay(slot, 1, buffer_size));
}

/* Check a STOINT interval against the manual's listed conflicts; 0 = OK, -1..-5 = conflict
   code, DM5120_STOINT_BAD_SLOT = slot out of range, DM5120_STOINT_TOO_SHORT = shorter
   than a conversion (error 551) */
int dm5120_validate_stoint_settings(int slot, int interval_ms) {
    dm5120_config *cfg;

    if (slot < 0 || slot >= 10) return DM5120_STOINT_BAD_SLOT;
    cfg = &g_dm5120_config[slot];

    /* STOINT 1-14 ms conflicts with: */
    if (interval_ms >= 1 && interval_ms <= 14) {
        if (strstr(cfg->function, "OHMS") != NULL ||
            strstr(cfg->function, "ACVDB") != NULL ||
            strstr(cfg->function, "ACADB") != NULL) {
            return -1; /* OHMS or dB function */
        }

        if (cfg->range_mode == 0) {
            return -2; /* AUTO range */
        }

        if (cfg->buffer_size == 0) {
            return -3; /* CIRCULAR buffer */
        }

        if (cfg->digits >= 5) {
            return -4; /* 5 or 6 digit resolution */
        }
    }

    if ((interval_ms == 1 || interval_ms == 2) && cfg->digits >= 4) {
        return -5; /* STOINT 1-2ms conflicts with 4+ digit resolution */
    }

    if (interval_ms > 0 && interval_ms < dm5120_min_stoint_ms(slot)) {
        return DM5120_STOINT_TOO_SHORT;
    }

    return 0; /* No conflicts */
}

/* Storage interval for the slot: the burst rate if burst mode is on, else 100 ms,
   raised to the shortest the settings allow */
int dm5120_stoint_for_slot(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int ms = 100;
    int min_ms = dm5120_min_stoint_ms(slot);

    if (cfg->burst_mode && cfg->sample_rate >= 0.05 && cfg->sample_rate <= 1000.0) {
        ms = (int)(1000.0 / cfg->sample_rate + 0.5);
    }
    return ms < min_ms ? min_ms : ms;
}

/* STOINT <ms>, or STOINT ONE for 0. An interval shorter than the settings allow is
   raised to dm5120_min_stoint_ms (the meter would stop with error 551 or 204); no
   message, since the monitor calls this. Returns the interval sent, -1 if none. */
int dm5120_set_storage_interval(int address, int slot, int interval_ms) {
    int status;
    int min_ms;

    if (slot < 0 || slot >= 10) {
        printf("ERROR: invalid DM5120 slot %d (valid: 0-9), STOINT not sent\n", slot);
        return -1;
    }

    if (interval_ms == 0) {
        /* One shot mode - store one reading per trigger */
        status = gpib_write_dm5120(address, "STOINT ONE");
    } else {
        min_ms = dm5120_min_stoint_ms(slot);
        if (interval_ms < min_ms) interval_ms = min_ms;
        sprintf(gpib_cmd_buffer, "STOINT %d", interval_ms);
        status = gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    if (status != 0) return -1;

    delay(dm5120_calculate_measurement_time(slot, 3, 1));
    return interval_ms;
}

int dm5120_query_storage_interval(int address) {

    long interval = 0;
    
    gpib_write_dm5120(address, "STOINT?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "ONE")) {
            return 0; /* One shot mode */
        }
        /* long: STOINT runs to 999999 ms */
        if (sscanf(gpib_response_buffer, "STOINT %ld", &interval) == 1) {
            return interval > 32767L ? 32767 : (int)interval;
        }
    }

    return -1; /* Error */
}

/* DM5120 SRQ events for buffer status */

/* Set FULL/HALF/RDY/OPC event masks, then enable RQS */
void dm5120_configure_srq_events(int address, int enable_full, int enable_half, int enable_rdy, int enable_opc) {

    sprintf(gpib_cmd_buffer, "FULL %s", enable_full ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "HALF %s", enable_half ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "RDY %s", enable_rdy ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    sprintf(gpib_cmd_buffer, "OPC %s", enable_opc ? "ON" : "OFF");
    gpib_write_dm5120(address, gpib_cmd_buffer);
    GPIB_PACE(50);
    
    gpib_write_dm5120(address, "RQS ON");
    GPIB_PACE(50);
}

/* Pick SRQ events to suit buffer type, size, and expected fill time */
void dm5120_configure_srq_events_smart(int address, int buffer_size, int is_circular, int expected_time_ms) {
    int enable_full, enable_half;
    
    if (is_circular) {
        /* CIRCULAR buffers never report FULL events */
        enable_full = 0;
        enable_half = 1;  /* Use HALF to monitor buffer activity */
        printf("SRQ Config: CIRCULAR buffer - HALF events enabled\n");
    } else {
        /* Linear buffers require FULL event for completion detection */
        enable_full = 1;
        
        /* Enable HALF for large buffers or slow operations */
        if (buffer_size > 100 || expected_time_ms > 10000) {
            enable_half = 1;
            printf("SRQ Config: Large/slow buffer - FULL+HALF events enabled\n");
        } else {
            enable_half = 0;
            printf("SRQ Config: Small/fast buffer - FULL events only\n");
        }
    }

    dm5120_configure_srq_events(address, enable_full, enable_half, 1, 1);
}

/* Check DM5120 buffer status using SRQ events */
int dm5120_check_buffer_status(int address) {
    
    int status = 0;
    float code;

    /* EVENT? replies with a Table 3-5 code, e.g. "EVENT 450;" */
    if (dm5120_query_value(address, "EVENT?", &code)) {
        switch ((int)code) {
            case 0:   status = 0; break;        /* No events */
            case 450: status = 0x01; break;     /* FULL buffer */
            case 451: status = 0x02; break;     /* HALF buffer */
            case 452: status = 0x04; break;     /* RDY ready */
            case 402: status = 0x08; break;     /* OPC operation complete */
            case 454: status = 0x10; break;     /* OVER overrange */
            case 551: status = 0x20; break;     /* STOINT shorter than a conversion */
            default:  status = 0x80; break;     /* Any other error or event */
        }
    }

    return status;
}

/* Fill the DM5120 buffer (CONT+STOINT if <=1 s/reading, else one SEND per reading)
   and read it back; returns samples read */
int dm5120_get_buffer_data_enhanced(int address, int slot, float far *buffer, int max_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    char response[GPIB_BUFFER_SIZE];
    float value;
    int count = 0;
    int timeout_ms, elapsed_ms = 0;
    int status;
    unsigned long start_time, current_time;
    int is_circular;
    float measurement_rate;
    int measurement_interval_ms;
    int use_cont_mode;
    int i;
    
    timeout_ms = dm5120_calculate_measurement_time(slot, 2, max_samples);

    is_circular = (cfg->buffer_size == 0);
    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    measurement_interval_ms = (int)(1000.0 / measurement_rate);
    use_cont_mode = (measurement_interval_ms <= 1000);

    dm5120_configure_srq_events_smart(address, max_samples, is_circular, timeout_ms);

    /* BUFSZ also clears any stored readings */
    if (is_circular) {
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", max_samples);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(100);
    
    if (use_cont_mode) {
        /* <=1 s per reading: CONT mode, STOINT fills the buffer */
        dm5120_set_trigger(address, "TALK", "CONT");
        measurement_interval_ms = dm5120_set_storage_interval(address, slot, measurement_interval_ms);
        printf("Buffer Strategy: CONT mode, %d ms intervals, %.1f r/s\n",
               measurement_interval_ms, measurement_rate);
    } else {
        /* >1 s per reading: trigger each one with SEND */
        dm5120_set_trigger(address, "TALK", "ONE");
        printf("Buffer Strategy: SEND mode, %d ms per measurement, %.1f r/s\n", 
               measurement_interval_ms, measurement_rate);
    }
    
    start_time = *((unsigned long far *)0x0040006CL);  /* BIOS tick count, ~55 ms/tick */

    if (use_cont_mode) {
        /* Buffer fills on its own; watch SRQ events */
        printf("Monitoring automatic buffer fill...\n");

        while (count < max_samples && elapsed_ms < timeout_ms) {
            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);

            status = dm5120_check_buffer_status(address);
            
            if (status & 0x01) { /* FULL buffer event */
                printf("FULL event received - buffer complete\n");
                break;
            }

            if (status & 0x20) {
                printf("ERROR 551: STOINT shorter than the conversion time\n");
                break;
            }
            
            if (status & 0x02) { /* HALF buffer event */
                printf("Buffer 50%% full...\n");
            }
            
            delay(200); /* Check events every 200ms */
        }
        
    } else {
        printf("Manual triggering for %d samples...\n", max_samples);

        for (i = 0; i < max_samples && elapsed_ms < timeout_ms; i++) {
            gpib_write_dm5120(address, "SEND");
            delay(measurement_interval_ms);

            current_time = *((unsigned long far *)0x0040006CL);
            elapsed_ms = (int)((current_time - start_time) * 55);

            /* Progress every 10 readings */
            if ((i + 1) % 10 == 0 || i == max_samples - 1) {
                printf("Progress: %d/%d samples (%.1f%%)\n", 
                       i + 1, max_samples, ((float)(i + 1) / max_samples) * 100.0);
            }
        }
        
        printf("Manual triggering complete\n");
    }

    printf("Checking buffer status before read...\n");
    gpib_write_dm5120(address, "BUFCNT?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        int buffer_count;
        if (sscanf(response, "%d", &buffer_count) == 1) {
            printf("Buffer contains %d samples\n", buffer_count);
            if (buffer_count == 0) {
                printf("WARNING: Buffer is empty - no data to read\n");
                return 0;
            }
        }
    }
    
    printf("Reading buffer data...\n");
    gpib_write_dm5120(address, "READ ALLSTORE");
    delay(dm5120_calculate_measurement_time(slot, 4, 1) * 2); /* 2x the status-query delay */

    /* Query adapter status; on ERROR/FAULT, abort to clear it before reading */
    printf("Checking GPIB status before read...\n");
    ieee_write("status\r\n");
    GPIB_PACE(50);
    
    if (ieee_read(response, sizeof(response)) > 0) {
        printf("GPIB Status: %s\n", response);
        if (strstr(response, "ERROR") || strstr(response, "FAULT")) {
            printf("ERROR: GPIB error detected: %s\n", response);
            printf("Attempting to clear error...\n");
            ieee_write("abort\r\n");
            GPIB_PACE(200);
            ieee_write("status\r\n");
            GPIB_PACE(50);
            if (ieee_read(response, sizeof(response)) > 0) {
                printf("Status after abort: %s\n", response);
            }
        }
    }

    printf("Attempting to read buffer data...\n");
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        char *token = strtok(response, ",;");
        
        while (token != NULL && count < max_samples) {
            if (sscanf(token, "%f", &value) == 1) {
                buffer[count] = value;
                count++;
            }
            token = strtok(NULL, ",;");
        }
        
        printf("Buffer read complete: %d samples retrieved\n", count);
    } else {
        printf("ERROR: Failed to read buffer data - READFAULT detected\n");
        printf("Attempting alternate read method...\n");
        
        /* Fallback: READ ONESTORE one sample at a time */
        for (i = 0; i < max_samples && count < max_samples; i++) {
            gpib_write_dm5120(address, "READ ONESTORE");
            GPIB_PACE(200);
            
            if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
                if (sscanf(response, "%f", &value) == 1) {
                    buffer[count] = value;
                    count++;
                } else {
                    printf("Failed to parse sample %d: '%s'\n", i, response);
                    break;
                }
            } else {
                printf("Failed to read sample %d\n", i);
                break;
            }
        }
        
        if (count > 0) {
            printf("Fallback read completed: %d samples retrieved\n", count);
        }
    }

    if (count == max_samples) {
        printf("SUCCESS: All %d samples collected\n", count);
    } else if (count > 0) {
        printf("PARTIAL: %d/%d samples collected\n", count, max_samples);
    } else {
        printf("FAILED: No samples collected (timeout: %d ms)\n", elapsed_ms);
    }
    
    return count;
}

/* DM5120 timing diagnostics */

/* Print rate, fill-time, and STOINT checks for the slot; returns warning count */
int dm5120_validate_configuration(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float measurement_rate;
    int optimal_delay, buffer_fill_time;
    int warnings = 0;

    printf("DM5120 Configuration Analysis for Slot %d:\n", slot);
    printf("==========================================\n");

    measurement_rate = dm5120_get_measurement_rate(slot, cfg->trigger_mode);
    printf("Measurement Rate: %.1f readings/sec\n", measurement_rate);

    optimal_delay = dm5120_calculate_measurement_time(slot, 0, 1);
    printf("Optimal Sample Interval: %d ms\n", optimal_delay);

    if (cfg->buffer_enabled) {
        int stoint_ms = dm5120_stoint_for_slot(slot);
        int min_ms = dm5120_min_stoint_ms(slot);

        printf("Storage Interval: %d ms (shortest for these settings: %d ms)\n", stoint_ms, min_ms);
        if (cfg->burst_mode && cfg->sample_rate > 0.0 && 1000.0 / cfg->sample_rate < min_ms) {
            printf("WARNING: burst rate %.1f/s is faster than the meter converts;\n", cfg->sample_rate);
            printf("  STOINT is raised to %d ms (%.1f/s)\n", min_ms, 1000.0 / min_ms);
            warnings++;
        }

        if (cfg->buffer_size > 0) {
            buffer_fill_time = (int)(((long)cfg->buffer_size * stoint_ms > 32767L) ? 32767L
                                     : (long)cfg->buffer_size * stoint_ms);
            printf("Buffer Fill Time (%d samples): %ld ms\n", cfg->buffer_size,
                   (long)cfg->buffer_size * stoint_ms);
            if (buffer_fill_time > 30000) { /* 30 s */
                printf("WARNING: Buffer fill time exceeds 30 seconds!\n");
                printf("Consider reducing buffer size or using faster settings.\n");
                warnings++;
            }
        }
    }

    printf("\nRecommendations:\n");
    if (measurement_rate < 10.0) {
        printf("- Consider using lower resolution for faster measurements\n");
        printf("- OHMS measurements are inherently slower than voltage\n");
    }
    
    if (cfg->digits >= 5) {
        printf("- High resolution reduces measurement speed significantly\n");
        printf("- AutoCal adds additional delay for 5-6 digit measurements\n");
    }
    
    if (cfg->buffer_size > 100) {
        printf("- Large buffers may cause timeout issues\n");
        printf("- Consider using smaller buffers with multiple reads\n");
    }
    
    printf("\nConfiguration Status: %s\n", warnings == 0 ? "OPTIMAL" : "NEEDS ATTENTION");
    return warnings;
}

void dm5120_timing_diagnostics(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float rate_continuous, rate_bus;
    int i;
    
    printf("DM5120 Timing Diagnostics - Slot %d\n", slot);
    printf("===================================\n\n");
    
    printf("Current Configuration:\n");
    printf("Function: %s\n", cfg->function);
    printf("Resolution: %d digits\n", cfg->digits);
    printf("Trigger Mode: %d\n", cfg->trigger_mode);
    printf("Buffer Enabled: %s\n", cfg->buffer_enabled ? "Yes" : "No");
    printf("Buffer Size: %d samples\n", cfg->buffer_size);
    printf("\n");
    
    printf("Measurement Rates:\n");
    rate_continuous = dm5120_get_measurement_rate(slot, 0); /* not forced: per config (TALK still gives bus rates) */
    rate_bus = dm5120_get_measurement_rate(slot, 2);        /* forced IEEE-488 bus */
    
    printf("Continuous Mode: %.1f readings/sec\n", rate_continuous);
    printf("IEEE-488 Bus Mode: %.1f readings/sec\n", rate_bus);
    printf("\n");
    
    printf("Operation Timings:\n");
    printf("Single Reading: %d ms\n", dm5120_calculate_measurement_time(slot, 0, 1));
    printf("Buffer Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 1, 1));
    printf("STOINT Setup: %d ms\n", dm5120_calculate_measurement_time(slot, 3, 1));
    printf("Status Query: %d ms\n", dm5120_calculate_measurement_time(slot, 4, 1));
    printf("\n");
    
    printf("Buffer Fill Times (STOINT %d ms, shortest %d ms):\n",
           dm5120_stoint_for_slot(slot), dm5120_min_stoint_ms(slot));
    for (i = 10; i <= 500; i += 100) {
        long fill_time = (long)i * dm5120_stoint_for_slot(slot);
        printf("%3d samples: %6ld ms (%5.1f sec)\n", i, fill_time, fill_time / 1000.0);
    }
    printf("\n");

    printf("Resolution vs. Speed Trade-offs:\n");
    printf("3.5 digits: Very fast, ~1000 r/s (voltage)\n");
    printf("4.5 digits: Fast, ~333 r/s (voltage)\n");
    printf("5.5 digits: Medium, ~7.5-35 r/s depending on AutoCal and 50/60 Hz\n");
    printf("6.5 digits: Slow, ~7.5-8.2 r/s\n");
    printf("\n");

    printf("Function Speed Comparison (5.5 digits):\n");
    printf("DCV/ACV: ~7.5-35 r/s\n");
    printf("OHMS: ~7.5-16 r/s\n");
    printf("OHMSCOMP: ~4-8 r/s (50%% of normal OHMS)\n");
}

int dm5120_get_buffer_data(int address, int slot, float far *buffer, int max_samples) {
    char response[GPIB_BUFFER_SIZE];
    int count = 0;
    int i;
    char *token;
    
    gpib_write_dm5120(address, "READ ALLSTORE");

    delay(dm5120_calculate_delay(slot, 2, max_samples));
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        token = strtok(response, ",");
        while (token && count < max_samples) {
            if (sscanf(token, "%e", &buffer[count]) == 1) {
                count++;
            }
            token = strtok(NULL, ",");
        }
    }
    
    return count;
}
/* Blocking fill: configure, wait the estimated fill time, then READ ALLSTORE */
int dm5120_fill_buffer_complete(int address, int slot, float far *buffer, int buffer_size) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    int samples_collected = 0;
    int total_wait_time;

    if (!cfg->buffer_enabled || buffer_size <= 0) {
        return 0;
    }

    if (cfg->buffer_enabled) {
        /* Configured source, default TALK,CONT, so the meter self-fills; EXT,CONT stalls
           the fill until an external trigger arrives. (FINDINGS #3) */
        dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", "CONT");

        gpib_write_dm5120(address, "HALF ON");
        GPIB_PACE(50);
        gpib_write_dm5120(address, "FULL ON");
        GPIB_PACE(50);
        gpib_write_dm5120(address, "RQS ON");
        GPIB_PACE(50);
    } else {
        dm5120_set_trigger(address, "TALK", "CONT");
    }

    dm5120_enable_buffering(address, slot, buffer_size);

    total_wait_time = dm5120_set_storage_interval(address, slot, dm5120_stoint_for_slot(slot));
    if (total_wait_time < 0) return 0;
    {
        long fill = (long)total_wait_time * buffer_size + 500;
        total_wait_time = fill > 32767L ? 32767 : (int)fill;
    }
    printf("Collecting %d samples at %d digits - estimated time: %dms\n",
           buffer_size, cfg->digits, total_wait_time);
    delay(total_wait_time);

    samples_collected = dm5120_get_buffer_data(address, slot, buffer, buffer_size);
    
    printf("Buffer operation complete: %d/%d samples collected\n", 
           samples_collected, buffer_size);
    
    return samples_collected;
}

/* DM5120 non-blocking buffer fill, polled from the measurement loop.
   buffer_state: 0 idle, 1 filling, 2 half, 3 full, 4 meter reported error 551,
   5 timed out */
void dm5120_start_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (cfg->buffer_state != 0) {
        return;  /* Already running */
    }

    /* TALK,CONT unless the user chose EXT; EXT,CONT without a trigger source
       leaves the buffer empty until the timeout. (FINDINGS #3) */
    dm5120_set_trigger(address, cfg->trigger_source ? "EXT" : "TALK", "CONT");
    GPIB_PACE(50);

    gpib_write_dm5120(address, "HALF ON");
    GPIB_PACE(50);
    gpib_write_dm5120(address, "FULL ON");
    GPIB_PACE(50);
    gpib_write_dm5120(address, "RQS ON");
    GPIB_PACE(50);

    /* BUFSZ clears the buffer (there is no BUFCLR). STOINT goes last: any later
       measurement setting would end the data store. */
    if (cfg->buffer_size > 0 && cfg->buffer_size <= DM5120_MAX_BUFFER_SIZE) {
        dm5120_set_buffer_size(address, cfg->buffer_size);
        GPIB_PACE(50);
    }

    dm5120_set_storage_interval(address, slot, dm5120_stoint_for_slot(slot));
    GPIB_PACE(50);

    cfg->buffer_state = 1; /* filling */
    cfg->buffer_start_time = *((unsigned long far *)0x0040006CL); /* BIOS tick count, 18.2 Hz */
    cfg->samples_ready = 0;
}

int dm5120_check_buffer_async(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    float code;
    int count;

    if (cfg->buffer_state < 1 || cfg->buffer_state > 2) {
        return cfg->buffer_state;  /* idle, or finished and waiting to be read */
    }

    /* Codes are numbers ("EVENT 450;"); others (power-on 401, 204, 1xx) are not
       buffer progress, so polling carries on */
    if (dm5120_query_value(address, "EVENT?", &code)) {
        switch ((int)code) {
            case 450:
                cfg->buffer_state = 3;
                cfg->samples_ready = cfg->buffer_size;
                break;
            case 451:
                cfg->buffer_state = 2;
                cfg->samples_ready = cfg->buffer_size / 2;
                break;
            case 551:
                cfg->buffer_state = 4;
                return cfg->buffer_state;
        }
    }

    if (cfg->buffer_state != 3) {
        count = dm5120_get_buffer_count(address);
        if (count > 0) {
            cfg->samples_ready = count;
            if (count >= cfg->buffer_size) {
                cfg->buffer_state = 3; /* full */
            } else if (count >= cfg->buffer_size / 2) {
                cfg->buffer_state = 2; /* half */
            }
        }
    }

    if (cfg->buffer_state == 1 || cfg->buffer_state == 2) {
        /* Expected fill time x1.5 + 10 s, in ~55 ms BIOS ticks */
        unsigned long limit_ms = (unsigned long)cfg->buffer_size * dm5120_stoint_for_slot(slot);
        unsigned long limit_ticks = (limit_ms + limit_ms / 2 + 10000UL) / 55UL;
        unsigned long elapsed_ticks = *((unsigned long far *)0x0040006CL) - cfg->buffer_start_time;
        if (elapsed_ticks > limit_ticks) {
            /* Not state 3: BUFAVE? on a part-filled buffer holds the bus until it fills */
            cfg->buffer_state = 5;
        }
    }

    return cfg->buffer_state;
}

void dm5120_reset_buffer_async(int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    cfg->buffer_state = 0;      /* idle */
    cfg->buffer_start_time = 0;
    cfg->samples_ready = 0;
}

/* DM5120 Buffer Query Functions */
float dm5120_get_buffer_average(int address) {
    
    float value;

    /* Holds the bus off until the buffer is full: call only once it is */
    return dm5120_query_value(address, "BUFAVE?", &value) ? value : 0.0;
}

int dm5120_get_buffer_count(int address) {
    float count;
    return dm5120_query_value(address, "BUFCNT?", &count) ? (int)count : 0;
}

float dm5120_get_buffer_min(int address) {
    float value;
    return dm5120_query_value(address, "BUFMIN?", &value) ? value : 0.0;
}

float dm5120_get_buffer_max(int address) {
    float value;
    return dm5120_query_value(address, "BUFMAX?", &value) ? value : 0.0;
}

void dm5120_set_buffer_size(int address, int size) {
    
    if (size == 0) {
        gpib_write_dm5120(address, "BUFSZ CIRCULAR");
    } else {
        sprintf(gpib_cmd_buffer, "BUFSZ %d", size);
        gpib_write_dm5120(address, gpib_cmd_buffer);
    }
    delay(100);
}

int dm5120_query_buffer_size(int address) {
    
    int size = 0;
    
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (strstr(gpib_response_buffer, "CIRCULAR")) {
            return 0;  /* same code BUFSZ takes for it */
        }
        if (sscanf(gpib_response_buffer, "BUFSZ %d", &size) == 1) {
            return size;
        }
    }

    return -1;
}

/* DM5120 stored-reading reads (READ ONESTORE / READ ALLSTORE) */
float dm5120_read_one_stored(int address) {
    
    float value = 0.0;
    
    gpib_write_dm5120(address, "READ ONESTORE");
    GPIB_PACE(100);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            return value;
        }
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}

int dm5120_read_all_stored(int address, float far *buffer, int max_samples) {
    char response[GPIB_BUFFER_SIZE];
    int count = 0;
    char *token;
    
    gpib_write_dm5120(address, "READ ALLSTORE");
    delay(200);  /* Allow time for all data to be formatted */
    
    if (gpib_read_dm5120(address, response, sizeof(response)) > 0) {
        token = strtok(response, ",");
        while (token && count < max_samples) {
            if (sscanf(token, "%e", &buffer[count]) == 1) {
                count++;
            }
            token = strtok(NULL, ",");
        }
    }
    
    return count;
}

float read_dm5120_buffered(int address, int slot) {
    dm5120_config *cfg = &g_dm5120_config[slot];
    static float *buffer_data = NULL;  /* Dynamic buffer for samples */
    static int buffer_allocated_size = 0;
    static int buffer_index = 0;
    static int buffer_valid_count = 0;
    static int last_slot = -1;
    
    /* Reset buffer if slot changed */
    if (last_slot != slot) {
        buffer_valid_count = 0;
        buffer_index = 0;
        last_slot = slot;
    }
    
    if (cfg->buffer_enabled) {
        if (!buffer_data || buffer_allocated_size < cfg->buffer_size) {
            if (buffer_data) {
                free(buffer_data);
            }
            buffer_data = (float *)malloc(cfg->buffer_size * sizeof(float));
            if (!buffer_data) {
                /* Allocation failed, fall back to single measurement */
                buffer_allocated_size = 0;
                return read_dm5120_enhanced(address, slot);
            }
            buffer_allocated_size = cfg->buffer_size;
            buffer_valid_count = 0;  /* Force buffer refill */
            buffer_index = 0;
        }
        if (buffer_index >= buffer_valid_count) {
            buffer_valid_count = dm5120_fill_buffer_complete(address, slot, 
                                                            buffer_data, cfg->buffer_size);
            buffer_index = 0;
            
            if (buffer_valid_count <= 0) {
                /* Buffer fill failed, fall back to single measurement */
                return read_dm5120_enhanced(address, slot);
            }
        }

        if (buffer_index < buffer_valid_count) {
            return buffer_data[buffer_index++];
        }
    }
    
    return read_dm5120_enhanced(address, slot);
}

int dm5120_get_multiple_samples(int address, int slot, float far *buffer, int num_samples) {
    dm5120_config *cfg = &g_dm5120_config[slot];

    if (!cfg->buffer_enabled) {
        int i;
        for (i = 0; i < num_samples; i++) {
            buffer[i] = read_dm5120_enhanced(address, slot);
            if (i < num_samples - 1) {
                delay(dm5120_calculate_delay(slot, 0, 1));
            }
        }
        return num_samples;
    } else {
        return dm5120_fill_buffer_complete(address, slot, buffer, num_samples);
    }
}

float read_dm5120_enhanced(int address, int slot) {
    float value = 0.0;
    int retry;
    dm5120_config *cfg = &g_dm5120_config[slot];
    
    int srq_status;
    
    srq_status = gpib_check_srq(address);
    if (srq_status & 0x40) {  /* Device requesting service */
        delay(100);
    }
    
    gpib_write_dm5120(address, "READ ADC");

    delay(dm5120_calculate_delay(slot, 0, 1));
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            if (cfg->min_max_enabled) {
                if (value < cfg->min_value) cfg->min_value = value;
                if (value > cfg->max_value) cfg->max_value = value;
            }
            store_module_data(slot, value);
            return value;
        }
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            if (cfg->min_max_enabled) {
                if (value < cfg->min_value) cfg->min_value = value;
                if (value > cfg->max_value) cfg->max_value = value;
            }
            store_module_data(slot, value);
            return value;
        }
    }
    
    /* SEND triggers one reading whatever the trigger setting (there is no X command) */
    gpib_write_dm5120(address, "SEND");
    delay(dm5120_calculate_delay(slot, 0, 1) + 50);
    
    if (gpib_read_float_dm5120(address, &value)) {
        if (cfg->min_max_enabled) {
            if (value < cfg->min_value) cfg->min_value = value;
            if (value > cfg->max_value) cfg->max_value = value;
        }
        store_module_data(slot, value);
        return value;
    }
    
    for (retry = 0; retry < 3; retry++) {
        gpib_write_dm5120(address, "SEND");
        GPIB_PACE(300);
        
        if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
            if (sscanf(gpib_response_buffer, "NDCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "DCV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "NACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "ACV%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
            if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
                store_module_data(slot, value);
                return value;
            }
        }
        
        delay(100);
    }
    
    return 0.0;
}

float read_dm5120_voltage(int address) {
    
    float value = 0.0;
    
    gpib_check_srq(address);
    GPIB_PACE(50);
    
    gpib_write_dm5120(address, "READ ADC");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            return value;
        }
    }
    
    return 0.0;
}
void dm5120_clear_statistics(int slot) {
    if (slot >= 0 && slot < 10) {
        g_dm5120_config[slot].min_value = 1e30;
        g_dm5120_config[slot].max_value = -1e30;
        g_dm5120_config[slot].sample_count = 0;
        clear_module_data(slot);
    }
}
void test_dm5120_comm(int address) {
    
    char status[128];
    float value;
    int i, srq_status;
    
    printf("\nTesting DM5120 at address %d...\n", address);
    
    ieee_write("status\r\n");
    GPIB_PACE(50);
    ieee_read(status, sizeof(status));
    printf("Driver status: %s\n", status);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote_dm5120(address);
    GPIB_PACE(200);
    
    printf("Checking SRQ status...\n");
    srq_status = gpib_check_srq(address);
    printf("SRQ status: %d\n", srq_status);
    
    printf("Sending ABORT command...\n");
    ieee_write("abort\r\n");
    GPIB_PACE(200);
    
    printf("\nSending INIT command...\n");
    gpib_write_dm5120(address, "INIT");
    GPIB_PACE(300);
    
    printf("Setting DCV function...\n");
    gpib_write_dm5120(address, "FUNCT DCV");
    GPIB_PACE(100);
    
    printf("Setting AUTO range...\n");
    gpib_write_dm5120(address, "RANGE AUTO");
    GPIB_PACE(100);
    
    printf("Setting data format ON (scientific)...\n");
    gpib_write_dm5120(address, "DATFOR ON");
    GPIB_PACE(100);
    
    printf("\nAttempting measurement reads:\n");
    
    printf("\n1. READ ADC:\n");
    gpib_write_dm5120(address, "READ ADC");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        if (sscanf(gpib_response_buffer, "%f", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n2. SEND (one triggered reading):\n");
    gpib_write_dm5120(address, "SEND");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
        if (sscanf(gpib_response_buffer, "%e", &value) == 1) {
            printf("   Parsed value: %g V\n", value);
        }
    } else {
        printf("   No response\n");
    }
    
    printf("\n3. READ? query:\n");
    gpib_write_dm5120(address, "READ?");
    GPIB_PACE(300);
    
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("   No response\n");
    }
    
    printf("\nQuerying current settings:\n");

    gpib_write_dm5120(address, "ID?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }

    gpib_write_dm5120(address, "FUNCT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    gpib_write_dm5120(address, "RANGE?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nTesting enhanced buffer commands:\n");
    
    printf("4. BUFSZ? (Buffer Size Query):\n");
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("5. BUFCNT? (Buffer Count Query):\n");
    gpib_write_dm5120(address, "BUFCNT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("6. Testing BUFSZ 50 (Set Buffer Size):\n");
    gpib_write_dm5120(address, "BUFSZ 50");
    GPIB_PACE(100);
    gpib_write_dm5120(address, "BUFSZ?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("7. STOINT? (Storage Interval Query):\n");
    gpib_write_dm5120(address, "STOINT?");
    GPIB_PACE(50);
    if (gpib_read_dm5120(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }

    i = dm5120_query_error(address);
    if (i >= 0) {
        printf("   ERROR %d: %s\n", i, dm5120_error_text(i));
    }

    printf("\nTest complete. Press any key to continue...");
    getch();
}
void test_dm5120_comm_debug(int address) {
    
    int bytes_read;
    int i;
    float test_value;
    
    printf("\n=== DM5120 Debug Communication Test ===\n");
    printf("Testing DM5120 at GPIB address %d\n\n", address);
    
    printf("Raw GPIB communication test:\n");
    
    printf("\n1. Sending raw OUTPUT command:\n");
    sprintf(gpib_cmd_buffer, "output %2d; INIT\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(500);
    
    printf("\n2. Sending function setup:\n");
    sprintf(gpib_cmd_buffer, "output %2d; FUNCT DCV\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    printf("\n3. Requesting reading:\n");
    sprintf(gpib_cmd_buffer, "output %2d; READ ADC\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    printf("\n4. Reading response:\n");
    sprintf(gpib_cmd_buffer, "enter %2d\r\n", address);
    printf("   %s\n", gpib_cmd_buffer);
    ieee_write(gpib_cmd_buffer);
    GPIB_PACE(200);
    
    bytes_read = ieee_read(gpib_response_buffer, sizeof(gpib_response_buffer));
    if (bytes_read > 0) {
        gpib_response_buffer[bytes_read] = '\0';
        printf("   %s\n", gpib_response_buffer);
        printf("   Hex dump: ");
        for (i = 0; i < bytes_read && i < 40; i++) {
            printf("%02X ", (unsigned char)gpib_response_buffer[i]);
        }
        printf("\n");
    } else {
        printf("   No response or error\n");
    }
    
    printf("\nDirect function test:\n");
    printf("\n5. Testing read_dm5120_enhanced():\n");
    test_value = read_dm5120_enhanced(address, 0);
    printf("   Returned value: %g\n", test_value);
    
    printf("\n=== Debug Test Complete ===\n");
    printf("Press any key to continue...");
    getch();
}

/* DM5010 multimeter: Tek Codes & Formats, manual 070-2994-01 section 3 */

static const char * __far dm5010_functs[] = { "DCV", "ACV", "ACDC", "OHMS", "DIODE" };

/* Full scale of each range, manual pp. 3-5, 3-9, 3-15. An argument above the top
   range is a command error; the meter rounds others up to the next range. */
static const float __far dm5010_dcv_fs[] = { 0.2f, 2.0f, 20.0f, 200.0f, 1000.0f };
static const float __far dm5010_acv_fs[] = { 0.2f, 2.0f, 20.0f, 200.0f, 700.0f };
static const float __far dm5010_ohm_fs[] = { 200.0f, 2e3f, 2e4f, 2e5f, 2e6f, 2e7f };

/* Two-state settings: header, argument when set, argument when clear (pp. 3-3/3-4) */
static const struct { const char *hdr, *on, *off; } __far d10_flags[] = {
    { "DIGIT", "3.5", "4.5" }, { "LFR", "ON", "OFF" },     { "SOURCE", "REAR", "FRONT" },
    { "MODE", "TRIG", "RUN" }, { "DT", "TRIG", "OFF" },    { "MONITOR", "ON", "OFF" },
    { "OPC", "ON", "OFF" },    { "OVER", "ON", "OFF" },    { "USER", "ON", "OFF" },
    { "RQS", "ON", "OFF" }
};
#define D10_RQS 9

/* ERR? codes, manual Table 3-1 p. 3-25 */
static const struct { int code; const char *text; } __far dm5010_errors[] = {
    {   0, "No errors or events" },          { 101, "Invalid command header" },
    { 102, "Header delimiter error" },       { 103, "Argument error" },
    { 104, "Argument delimiter error" },     { 106, "Missing argument" },
    { 107, "Invalid message unit delimiter" },
    { 201, "Not executable in local mode" }, { 202, "Settings lost due to rtl" },
    { 203, "Input and output buffers full" }, { 205, "Argument out of range" },
    { 206, "Group Execute Trigger ignored" }, { 231, "Not in calibrate mode" },
    { 232, "Beyond calibration or null capability" },
    { 301, "Interrupt fault" },              { 302, "System error" },
    { 303, "Math pack error" },              { 311, "Converter time-out" },
    { 317, "Front panel time-out" },         { 318, "Bad ohms calibration constant" },
    { 351, "Calibration checksum error" },   { 401, "Power on" },
    { 402, "Operation complete" },           { 403, "ID user request" },
    { 601, "Over-range" },                   { 701, "Below limits" },
    { 703, "Above limits" }
};

/* Calculations in effect, from CALC? or the slot's settings */
#define D10_AVE   1
#define D10_CMPR  2
#define D10_DBM   4
#define D10_DBR   8
#define D10_RATIO 16

/* Function and calculations the meter reported with its last reading; the unit
   label follows these, so a front-panel change shows up */
static struct { char funct[6]; unsigned char calc; } __far dm5010_live[10];
/* Cleared state; a profile or settings load does not run init_dm5010_config */
static float __far dm5010_min[10] = { 1e30f, 1e30f, 1e30f, 1e30f, 1e30f, 1e30f, 1e30f, 1e30f, 1e30f, 1e30f };
static float __far dm5010_max[10] = { -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f, -1e30f };
static char __far dm5010_reply[256];     /* SET? replies run to 225 characters (p. 3-19) */

/* Ranges of a function; 0 for DIODE, which takes no argument (p. 3-10) */
static int dm5010_ranges(const char *f, const float **fs) {
    if (f[0] == 'D' && f[1] == 'I') return 0;
    if (f[0] == 'O') { *fs = dm5010_ohm_fs; return 6; }
    *fs = (f[0] == 'D') ? dm5010_dcv_fs : dm5010_acv_fs;
    return 5;
}

/* Index of the function header that starts s ("OHMS -2.E+6"), -1 if none */
static int dm5010_funct_index(const char *s) {
    int i, n;
    for (i = 0; i < 5; i++) {
        n = strlen(dm5010_functs[i]);
        if (strncmp(s, dm5010_functs[i], n) == 0 && (s[n] == ' ' || s[n] == ';' || s[n] == '\0')) return i;
    }
    return -1;
}

static unsigned char dm5010_calc_bits(const char *s) {
    unsigned char b = 0;
    if (strstr(s, "AVE") || strstr(s, "AVG")) b |= D10_AVE;
    if (strstr(s, "CMPR") || strstr(s, "COMP")) b |= D10_CMPR;
    if (strstr(s, "DBM")) b |= D10_DBM;
    if (strstr(s, "DBR")) b |= D10_DBR;
    if (strstr(s, "RATIO")) b |= D10_RATIO;
    return b;
}

static unsigned char dm5010_cfg_calc(dm5010_config *cfg) {
    return (cfg->filter_enabled ? D10_AVE : 0) | (cfg->cmpr_on ? D10_CMPR : 0) |
           (cfg->ratio_on ? D10_RATIO : 0) |
           (cfg->calculation_mode == 3 ? D10_DBM : cfg->calculation_mode == 4 ? D10_DBR : 0);
}

static int d10_flag(dm5010_config *cfg, int k) {
    switch (k) {
        case 0: return cfg->high_speed_mode;
        case 1: return cfg->lfr_on;
        case 2: return !cfg->source_front;
        case 3: return cfg->trigger_mode != 0;
        case 4: return cfg->dt_trig;
        case 5: return cfg->monitor_on;
        case 6: return cfg->opc_on;
        case 7: return cfg->over_on;
        case 8: return cfg->user_on;
        default: return cfg->rqs_on;
    }
}

static void d10_set_flag(dm5010_config *cfg, int k, int on) {
    switch (k) {
        case 0: cfg->high_speed_mode = on; break;
        case 1: cfg->lfr_on = on; break;
        case 2: cfg->source_front = !on; break;
        case 3: cfg->trigger_mode = on; break;
        case 4: cfg->dt_trig = on; break;
        case 5: cfg->monitor_on = on; break;
        case 6: cfg->opc_on = on; break;
        case 7: cfg->over_on = on; break;
        case 8: cfg->user_on = on; break;
        default: cfg->rqs_on = on; break;
    }
}

static const char *d10_arg(dm5010_config *cfg, int k) {
    return d10_flag(cfg, k) ? d10_flags[k].on : d10_flags[k].off;
}

static void dm5010_stats_clear(int slot) {
    dm5010_min[slot] = 1e30;
    dm5010_max[slot] = -1e30;
}

/* Power-on settings, manual Table 3-3 p. 3-26 */
void init_dm5010_config(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];

    memset(cfg, 0, sizeof(*cfg));
    strcpy(cfg->function, "DCV");
    cfg->avg_count = 2;
    cfg->scale_factor = 1.0;
    cfg->dbr_reference = 1.0;
    cfg->source_front = 1;
    cfg->rqs_on = 1;
    dm5010_live[slot].funct[0] = '\0';
    dm5010_stats_clear(slot);
}

/* Values the v3.7 SCPI menu or its defaults leave in a profile or settings file */
void dm5010_fix_legacy_config(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    const float *fs;

    cfg->function[sizeof(cfg->function) - 1] = '\0';
    if (dm5010_funct_index(cfg->function) < 0) strcpy(cfg->function, "DCV");
    if (cfg->range_mode > dm5010_ranges(cfg->function, &fs)) cfg->range_mode = 0;
    if (cfg->trigger_mode > 1) cfg->trigger_mode = 1;
    if (cfg->calculation_mode == 1) cfg->filter_enabled = 1;      /* old AVG */
    else if (cfg->calculation_mode == 2) cfg->ratio_on = 1;       /* old SCALE */
    if (cfg->calculation_mode != 3 && cfg->calculation_mode != 4) cfg->calculation_mode = 0;
    if (cfg->avg_count == 0) cfg->avg_count = 2;
    if (cfg->scale_factor == 0.0) cfg->scale_factor = 1.0;       /* RATIO A can't be 0 (p. 3-17) */
    if (cfg->dbr_reference == 0.0) cfg->dbr_reference = 1.0;     /* nor DBR (p. 3-8) */
    /* v3.7 kept min/max statistics, starting at +-1e30, where the limits are */
    if (fabs(cfg->limit_hi) > 1e20 || fabs(cfg->limit_lo) > 1e20) cfg->limit_hi = cfg->limit_lo = 0.0;
    if (cfg->nullval == 0.0) cfg->null_enabled = 0;               /* NULL 0 is off (p. 3-15) */
}

/* One result in ms: conversion time (DIGIT, p. 3-9; DIODE taken as ohms), x4 with
   LFR on ACV/ACDC (p. 3-13), xN with CALC AVE (p. 3-7) */
static long dm5010_result_ms(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    const char *f = dm5010_live[slot].funct[0] ? dm5010_live[slot].funct : cfg->function;
    int ohms = (f[0] == 'O' || f[1] == 'I');
    long ms = cfg->high_speed_mode ? (ohms ? 130 : 35) : (ohms ? 620 : 310);

    if (cfg->lfr_on && f[0] == 'A') ms *= 4;
    if (cfg->filter_enabled) ms *= cfg->avg_count;
    return ms;
}

/* Read timeout for the DM5010 at this address: two results plus autorange settling
   (1.24 s ohms step response, p. 1-7), at least 5 s, within 16 bits with room for
   gpib.c to round up to whole seconds */
unsigned dm5010_timeout_ms(int address) {
    int i;
    long ms;

    if (!g_system) return 5000u;
    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled && g_system->modules[i].module_type == MOD_DM5010 &&
            g_system->modules[i].gpib_address == address) break;
    }
    if (i == 10) return 5000u;
    ms = 2 * dm5010_result_ms(i) + 2000;
    if (ms < 5000) ms = 5000;
    if (ms > 64000L) ms = 64000L;
    return (unsigned)ms;
}

static const char *dm5010_error_text(int code) {
    int i;
    for (i = 0; i < sizeof(dm5010_errors) / sizeof(dm5010_errors[0]); i++)
        if (dm5010_errors[i].code == code) return dm5010_errors[i].text;
    return "Unknown code";
}

/* Send a query; its reply, line ends removed, is left in dm5010_reply */
static int dm5010_query(int address, char *query) {
    dm5010_reply[0] = '\0';
    if (gpib_write_dm5010(address, query) < 0) return 0;
    GPIB_PACE(50);
    if (gpib_read_dm5010(address, dm5010_reply, sizeof(dm5010_reply)) <= 0) {
        dm5010_reply[0] = '\0';
        return 0;
    }
    dm5010_reply[strcspn(dm5010_reply, "\r\n")] = '\0';
    return 1;
}

/* First number after a reply's header: "ERR  401;" -> 401, "NULL 0.;" -> 0 */
static int dm5010_reply_number(const char *s, double *v) {
    while (*s && (isalpha((unsigned char)*s) || *s == ' ')) s++;
    return sscanf(s, "%lf", v) == 1;
}

/* ERR? code, or -1 if the meter did not answer */
static int dm5010_query_error(int address) {
    double v;
    if (!dm5010_query(address, "ERR?") || !strstr(dm5010_reply, "ERR") ||
        !dm5010_reply_number(dm5010_reply, &v)) return -1;
    return (int)v;
}

/* Print and drain ERR? until it reports 0. Returns the 1xx-3xx errors (4xx-7xx
   are events and warnings, Table 3-1), -1 if ERR? got no reply. */
static int dm5010_show_errors(int address, int *events) {
    int i, code, errors = 0, shown = 0;
    for (i = 0; i < 10; i++) {
        code = dm5010_query_error(address);
        if (code < 0 && i == 0) errors = -1;
        if (code <= 0) break;
        printf("  ERR %d: %s\n", code, dm5010_error_text(code));
        shown++;
        if (code < 400) errors++;
    }
    if (events) *events = shown;
    return errors;
}

/* Parse the output of "FUNCT?;CALC?;SEND", e.g. "OHMS -2.E+4;CALC OFF;+1.234E+3;"
   (pp. 3-7, 3-11, 3-18): function and calculations for the unit, then the reading,
   which has no header. +-1.E+99 is over-range; it does not fit a float. */
static int dm5010_parse_reading(int slot, char *reply, double *value) {
    char *tok;
    int n, r = DM5010_READ_NONE;

    for (tok = strtok(reply, ";"); tok; tok = strtok(NULL, ";")) {
        while (*tok == ' ' || *tok == '\r' || *tok == '\n') tok++;
        if (strncmp(tok, "CALC", 4) == 0) {
            dm5010_live[slot].calc = dm5010_calc_bits(tok + 4);
        } else if (isalpha((unsigned char)*tok)) {
            n = strcspn(tok, " ");
            if (n > 5) n = 5;
            memcpy(dm5010_live[slot].funct, tok, n);
            dm5010_live[slot].funct[n] = '\0';
        } else if (strstr(tok, "E+99")) {
            r = DM5010_READ_OVER;
        } else if (sscanf(tok, "%lf", value) == 1) {
            r = DM5010_READ_OK;
        }
    }
    return r;
}

/* One reading. SEND returns the latest reading or triggers one and waits (p. 3-30),
   in MODE TRIG as well, so no INIT or trigger is needed first. No printf: the
   monitor calls this. */
int dm5010_read(int address, int slot, double *value) {
    int r = DM5010_READ_NONE;

    if (gpib_write_dm5010(address, "FUNCT?;CALC?;SEND") == 0 &&
        gpib_read_dm5010(address, dm5010_reply, sizeof(dm5010_reply)) > 0) {
        r = dm5010_parse_reading(slot, dm5010_reply, value);
        /* An answer without the reading (SEND dropped from the message): SEND alone */
        if (r == DM5010_READ_NONE && gpib_write_dm5010(address, "SEND") == 0 &&
            gpib_read_dm5010(address, dm5010_reply, sizeof(dm5010_reply)) > 0)
            r = dm5010_parse_reading(slot, dm5010_reply, value);
    }
    if (r == DM5010_READ_OK) {
        store_module_data(slot, *value);
        if (g_dm5010_config[slot].statistics_enabled) {
            if (*value < dm5010_min[slot]) dm5010_min[slot] = (float)*value;
            if (*value > dm5010_max[slot]) dm5010_max[slot] = (float)*value;
        }
    }
    return r;
}

/* Unit of the slot's readings: the meter's last report, else the slot's settings */
const char *dm5010_unit(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    int live = dm5010_live[slot].funct[0] != '\0';
    const char *f = live ? dm5010_live[slot].funct : cfg->function;
    unsigned char c = live ? dm5010_live[slot].calc : dm5010_cfg_calc(cfg);

    if (c & D10_CMPR) return "cmp";
    if (c & D10_DBM) return "dBm";
    if (c & D10_DBR) return "dB";
    return f[0] == 'O' ? "ohm" : "V";
}

/* A dm5010_read result as text (no printf: the monitor uses it). With CALC CMPR the
   reading is 1/2/3 for below/between/above the limits (p. 3-18). */
void dm5010_format(int slot, int r, double v, char *out) {
    if (r == DM5010_READ_OVER) strcpy(out, "   OVERRANGE");
    else if (r != DM5010_READ_OK) strcpy(out, "  No reading");
    else if (dm5010_unit(slot)[0] == 'c') strcpy(out, v < 1.5 ? "  LO (limits)" : v < 2.5 ? "  PASS" : "  HI (limits)");
    else sprintf(out, "%12.6g %s", v, dm5010_unit(slot));
}

static int dm5010_confirm(const char *what) {
    printf("\n%s - are you sure (Y/N)? ", what);
    return toupper(getch()) == 'Y';
}

/* 1 if the message went out */
static int dm5010_cmd(int address, char *cmd) {
    int ok = gpib_write_dm5010(address, cmd) == 0;
    GPIB_PACE(50);
    return ok;
}

/* Send the slot's settings, one command per message so an error drops only that
   one (p. 3-22). Function first, since it clears NULL (p. 3-15); CALC turns off
   whatever it doesn't list (p. 3-7). RQS OFF keeps errors queued for ERR?, not
   cleared by the serial polls (p. 3-25). Numbers go in the manual's upper-case
   E form (p. 3-21). Returns the errors reported, -1 if the meter didn't answer. */
static int dm5010_apply(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    unsigned char c = dm5010_cfg_calc(cfg);
    const float *fs;
    int k, errors, sent = 1;

    gpib_remote(address);
    GPIB_PACE(100);
    sent &= dm5010_cmd(address, "RQS OFF");
    /* Events queued before this Apply are not its errors */
    for (k = 0; k < 10 && dm5010_query_error(address) > 0; k++) ;
    if (dm5010_ranges(cfg->function, &fs) == 0) strcpy(gpib_cmd_buffer, "DIODE");
    else sprintf(gpib_cmd_buffer, "%s %G", cfg->function,      /* 0 = autorange */
                 cfg->range_mode ? fs[cfg->range_mode - 1] : 0.0f);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    for (k = 0; k < D10_RQS; k++) {
        if (k == 1 && cfg->function[0] != 'A') continue;     /* LFR is for ACV/ACDC (p. 3-13) */
        /* Asked again here unless the meter is already on REAR: a loaded profile
           can hold REAR (caution, p. 3-19) */
        if (k == 2 && !cfg->source_front &&
            !(dm5010_query(address, "SOURCE?") && strstr(dm5010_reply, "REAR"))) {
            printf("CAUTION: switching to the rear input with over 500 V peak on the front\n");
            printf("input can damage the meter (p. 3-19).\n");
            if (!dm5010_confirm("Switch to SOURCE REAR")) {
                printf("\nSOURCE REAR not sent\n");
                continue;
            }
            printf("\n");
        }
        sprintf(gpib_cmd_buffer, "%s %s", d10_flags[k].hdr, d10_arg(cfg, k));
        sent &= dm5010_cmd(address, gpib_cmd_buffer);
    }
    sprintf(gpib_cmd_buffer, "AVE %d", cfg->avg_count);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    sprintf(gpib_cmd_buffer, "RATIO %G,%G", cfg->scale_factor, cfg->scale_offset);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    sprintf(gpib_cmd_buffer, "DBR %G", cfg->dbr_reference);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    sprintf(gpib_cmd_buffer, "LIMITS %G,%G", cfg->limit_hi, cfg->limit_lo);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    sprintf(gpib_cmd_buffer, "NULL %G", cfg->null_enabled ? cfg->nullval : 0.0f);
    sent &= dm5010_cmd(address, gpib_cmd_buffer);
    sprintf(gpib_cmd_buffer, "CALC %s%s%s%s%s", c & D10_AVE ? "AVE," : "", c & D10_CMPR ? "CMPR," : "",
            c & D10_DBM ? "DBM," : "", c & D10_DBR ? "DBR," : "", c & D10_RATIO ? "RATIO," : "");
    if (c) gpib_cmd_buffer[strlen(gpib_cmd_buffer) - 1] = '\0';
    else strcat(gpib_cmd_buffer, "OFF");
    sent &= dm5010_cmd(address, gpib_cmd_buffer);

    errors = dm5010_show_errors(address, NULL);
    if (cfg->rqs_on) sent &= dm5010_cmd(address, "RQS ON");
    dm5010_live[slot].funct[0] = '\0';
    return sent ? errors : -1;
}

/* Settings from a SET? reply (example p. 3-19) into the slot */
static void dm5010_load_settings(int slot, char *s) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    const float *fs;
    char *tok;
    double a, b;
    int i, k, n;

    for (tok = strtok(s, ";"); tok; tok = strtok(NULL, ";")) {
        while (*tok == ' ') tok++;
        a = b = 0.0;
        dm5010_reply_number(tok, &a);
        if ((i = dm5010_funct_index(tok)) >= 0) {       /* "OHMS -2.E+6": negative = autorange */
            strcpy(cfg->function, dm5010_functs[i]);
            cfg->range_mode = 0;
            n = dm5010_ranges(cfg->function, &fs);
            for (k = 0; a > 0.0 && k < n; k++) {
                if (fs[k] >= a * 0.999) { cfg->range_mode = k + 1; break; }
            }
        } else if (strncmp(tok, "AVE ", 4) == 0) {
            if (a > 255.0) printf("AVE %.0f is above this menu's 255; set to 255\n", a);
            cfg->avg_count = a > 255.0 ? 255 : a < 1.0 ? 1 : (unsigned char)a;
        } else if (strncmp(tok, "RATIO ", 6) == 0 || strncmp(tok, "LIMITS ", 7) == 0) {
            sscanf(strchr(tok, ' '), "%lf%*[, ]%lf", &a, &b);
            if (tok[0] == 'R') {
                cfg->scale_factor = (float)a;
                cfg->scale_offset = (float)b;
            } else {
                cfg->limit_hi = (float)a;
                cfg->limit_lo = (float)b;
            }
        } else if (strncmp(tok, "DBR ", 4) == 0) {
            cfg->dbr_reference = (float)a;
        } else if (strncmp(tok, "NULL ", 5) == 0) {
            cfg->nullval = (float)a;
            cfg->null_enabled = (a != 0.0);
        } else if (strncmp(tok, "CALC ", 5) == 0) {
            k = dm5010_calc_bits(tok + 5);
            cfg->filter_enabled = (k & D10_AVE) != 0;
            cfg->cmpr_on = (k & D10_CMPR) != 0;
            cfg->ratio_on = (k & D10_RATIO) != 0;
            cfg->calculation_mode = (k & D10_DBM) ? 3 : (k & D10_DBR) ? 4 : 0;
        } else {
            for (k = 0; k <= D10_RQS; k++) {
                n = strlen(d10_flags[k].hdr);
                if (strncmp(tok, d10_flags[k].hdr, n) == 0 && tok[n] == ' ') {
                    d10_set_flag(cfg, k, strncmp(tok + n + 1, d10_flags[k].on, strlen(d10_flags[k].on)) == 0);
                    break;
                }
            }
        }
    }
    dm5010_fix_legacy_config(slot);
}

static void dm5010_fs_text(const char *f, float fs, char *out) {
    if (f[0] != 'O') sprintf(out, fs < 1.0f ? "%g mV" : "%g V", fs < 1.0f ? fs * 1000.0f : fs);
    else if (fs >= 1e6f) sprintf(out, "%g Mohm", fs / 1e6f);
    else if (fs >= 1e3f) sprintf(out, "%g kohm", fs / 1e3f);
    else sprintf(out, "%g ohm", fs);
}

/* Number from the console; 0 if the entry is not a number */
static int dm5010_ask(const char *prompt, float *v) {
    char line[32];
    char extra;
    int c;

    printf("%s", prompt);
    /* Leading space skips the newline an earlier scanf left behind */
    if (scanf(" %31[^\n]", line) != 1) return 0;
    while ((c = getchar()) != '\n' && c != EOF) ;
    if (sscanf(line, "%f %c", v, &extra) != 1) {
        printf("Not a number\n");
        return 0;
    }
    return 1;
}

/* Each setting by its own query, plus ID?, RDY?, TEST and DATA */
static const char * __far dm5010_queries[] = {
    "ID?", "FUNCT?", "DIGIT?", "LFR?", "SOURCE?", "MODE?", "DT?", "RDY?", "NULL?", "AVE?",
    "CALC?", "RATIO?", "DBR?", "LIMITS?", "MONITOR?", "OPC?", "OVER?", "RQS?", "USER?"
};

static void dm5010_utilities_menu(int slot) {
    int address = g_system->modules[slot].gpib_address;
    double v;
    int i, choice, done = 0;

    while (!done) {
        clrscr();
        printf("DM5010 Utilities - GPIB %d\n", address);
        printf("==========================\n\n");
        printf("1. Identify (ID?)\n");
        printf("2. Calibration checksum (TEST)\n");
        printf("3. Read error/event queue (ERR?)\n");
        printf("4. Reading ready? (RDY?)\n");
        printf("5. Reading that caused the MONITOR SRQ, or the latest one (DATA)\n");
        printf("6. Trigger with GET and read (needs MODE TRIG, DT TRIG applied)\n");
        printf("7. Read every setting by its own query\n");
        printf("8. INIT - return to the power-on settings\n");
        printf("0. Return\n\nSelect option: ");
        choice = getch();
        printf("\n\n");

        switch (choice) {
            case '1':
                printf("%s\n", dm5010_query(address, "ID?") ? dm5010_reply : "No reply");
                break;
            case '4':
                /* "RDY  0;" measuring or waiting for a trigger, "RDY  1;" ready (p. 3-17) */
                if (dm5010_query(address, "RDY?") && strncmp(dm5010_reply, "RDY", 3) == 0 &&
                    dm5010_reply_number(dm5010_reply, &v))
                    printf("%s\n%s\n", dm5010_reply, v != 0.0 ? "A reading is available" : "Measuring or waiting for a trigger");
                else
                    printf("No reply: %s\n", dm5010_reply);
                break;
            case '2':
                /* TEST only reports the stored checksum, "TEST 0;" or "TEST 351;" (p. 3-20) */
                if (dm5010_query(address, "TEST") && strncmp(dm5010_reply, "TEST", 4) == 0 &&
                    dm5010_reply_number(dm5010_reply, &v)) {
                    printf("%s\n%s\n", dm5010_reply, v == 0.0 ? "Calibration checksum correct"
                                                             : "Calibration checksum ERROR (351)");
                } else {
                    printf("No reply: %s\n", dm5010_reply);
                }
                break;
            case '3':
                if (dm5010_show_errors(address, &i) < 0) printf("No reply\n");
                else if (i == 0) printf("No errors or events\n");
                break;
            case '5':
                /* "DATA <number>;" or "DATA +-1.E+99;"; neither triggers nor waits (p. 3-8) */
                if (dm5010_query(address, "DATA") && strncmp(dm5010_reply, "DATA", 4) == 0) {
                    if (strstr(dm5010_reply, "E+99")) printf("Over-range\n");
                    else if (dm5010_reply_number(dm5010_reply, &v)) printf("%.7G %s\n", v, dm5010_unit(slot));
                    else printf("%s\n", dm5010_reply);
                } else {
                    printf("No reply: %s\n", dm5010_reply);
                }
                break;
            case '6':
                /* GET in local state is ignored (206, p. 3-23) */
                gpib_remote(address);
                GPIB_PACE(100);
                sprintf(gpib_cmd_buffer, "trigger %2d\r\n", address);
                ieee_write(gpib_cmd_buffer);
                GPIB_PACE(50);
                /* Talk-addressed with no query, the meter returns the pending reading (p. 3-30) */
                if (gpib_read_dm5010(address, dm5010_reply, sizeof(dm5010_reply)) > 0) {
                    dm5010_reply[strcspn(dm5010_reply, "\r\n")] = '\0';
                    printf("%s\n", dm5010_reply);
                } else {
                    printf("No reading\n");
                }
                if (dm5010_show_errors(address, NULL) > 0) printf("(206 = GET ignored: DT is OFF or the meter was busy)\n");
                break;
            case '7':
                for (i = 0; i < sizeof(dm5010_queries) / sizeof(dm5010_queries[0]); i++) {
                    printf("%-9s %s\n", dm5010_queries[i],
                           dm5010_query(address, (char *)dm5010_queries[i]) ? dm5010_reply : "(no reply)");
                }
                break;
            case '8':
                if (dm5010_confirm("Reset the meter's function, range, calculations and SRQs")) {
                    /* Operational: refused in local state (201, p. 3-3) */
                    gpib_remote(address);
                    GPIB_PACE(100);
                    if (gpib_write_dm5010(address, "INIT") != 0) {
                        printf("\nNo reply from the meter\n");
                        break;
                    }
                    GPIB_PACE(300);
                    dm5010_live[slot].funct[0] = '\0';
                    printf("\n");
                    i = dm5010_show_errors(address, NULL);
                    printf("%s\n", i == 0 ? "Meter at its power-on settings (DCV autorange). D in the setup menu re-applies TM5000's." :
                                   i > 0 ? "INIT not done: see the errors above" : "No reply to ERR?");
                }
                break;
            case '0':
            case 27:
                done = 1;
                break;
        }
        if (!done) {
            printf("\nPress any key...");
            getch();
        }
    }
}

void configure_dm5010_advanced(int slot) {
    dm5010_config *cfg = &g_dm5010_config[slot];
    int address = g_system->modules[slot].gpib_address;
    int choice, i, n, pause, done = 0;
    const float *fs;
    char txt[32];
    double v;
    float a, b;

    dm5010_fix_legacy_config(slot);

    while (!done) {
        clrscr();
        printf("DM5010 Configuration - Slot %d, GPIB %d\n", slot, address);
        printf("========================================\n\n");
        n = dm5010_ranges(cfg->function, &fs);
        if (cfg->range_mode && n) dm5010_fs_text(cfg->function, fs[cfg->range_mode - 1], txt);
        else strcpy(txt, n ? "AUTO" : "-");
        printf("1. Function: %-6s           2. Range: %s\n", cfg->function, txt);
        printf("3. DIGIT %s (%ld ms/result)   4. Low freq response: LFR %s\n",
               d10_arg(cfg, 0), dm5010_result_ms(slot), d10_arg(cfg, 1));
        printf("5. Input: SOURCE %-5s        6. Trigger: MODE %s, DT %s\n",
               d10_arg(cfg, 2), d10_arg(cfg, 3), d10_arg(cfg, 4));
        printf("7. Null: %s", cfg->null_enabled ? "ON" : "OFF");
        if (cfg->null_enabled) printf(" (%g)", cfg->nullval);
        printf("\n8. Average: %s (AVE %d)\n", cfg->filter_enabled ? "ON" : "OFF", cfg->avg_count);
        printf("9. Ratio X-B/A: %s (A %g, B %g)\n", cfg->ratio_on ? "ON" : "OFF",
               cfg->scale_factor, cfg->scale_offset);
        printf("A. dB: %s (DBR reference %g)\n", cfg->calculation_mode == 3 ? "DBM" :
               cfg->calculation_mode == 4 ? "DBR" : "OFF", cfg->dbr_reference);
        printf("B. Compare: %s (LIMITS %g, %g)\n", cfg->cmpr_on ? "ON" : "OFF",
               cfg->limit_hi, cfg->limit_lo);
        printf("C. SRQs: RQS %s, OPC %s, OVER %s, USER %s, MONITOR %s\n", d10_arg(cfg, 9),
               d10_arg(cfg, 6), d10_arg(cfg, 7), d10_arg(cfg, 8), d10_arg(cfg, 5));
        printf("\nD. Apply Settings to DM5010      E. Read Settings from DM5010 (SET?)\n");
        printf("F. Test Measurement (SEND)       G. Communication Test\n");
        printf("H. Min/Max Statistics: %-3s       I. Utilities (ID, TEST, ERR?, DATA, GET, INIT)\n",
               cfg->statistics_enabled ? "ON" : "OFF");
        printf("0. Return\n\nSelect option: ");
        choice = toupper(getch());
        printf("\n\n");
        pause = 1;

        switch (choice) {
            case '1':
                for (i = 0; i < 5; i++) printf("%d. %s\n", i, dm5010_functs[i]);
                printf("\nFunction: ");
                i = getch() - '0';
                if (i >= 0 && i < 5 && strcmp(cfg->function, dm5010_functs[i]) != 0) {
                    strcpy(cfg->function, dm5010_functs[i]);
                    cfg->range_mode = 0;
                    cfg->null_enabled = 0;   /* a function change turns NULL off (p. 3-15) */
                }
                pause = 0;
                break;
            case '2':
                if (n == 0) {
                    printf("DIODE has no range\n");
                    break;
                }
                printf("0. AUTO\n");
                for (i = 0; i < n; i++) {
                    dm5010_fs_text(cfg->function, fs[i], txt);
                    printf("%d. %s\n", i + 1, txt);
                }
                printf("\nRange: ");
                i = getch() - '0';
                if (i >= 0 && i <= n) cfg->range_mode = i;
                pause = 0;
                break;
            case '3':
            case '4':
                /* LFR averages four conversions; it applies to ACV and ACDC (p. 3-13) */
                d10_set_flag(cfg, choice - '3', !d10_flag(cfg, choice - '3'));
                pause = 0;
                break;
            case '5':
                if (cfg->source_front) {
                    printf("CAUTION (p. 3-19): do not switch to the rear input while over 500 V\n");
                    printf("peak is applied to the front panel input; it can damage the meter.\n");
                    if (!dm5010_confirm("Use the rear interface input")) break;
                }
                cfg->source_front = !cfg->source_front;
                pause = 0;
                break;
            case '6':
                printf("MODE: 0 = RUN (free-run), 1 = TRIG (SEND, GET or talk triggers a reading): ");
                i = getch() - '0';
                if (i == 0 || i == 1) cfg->trigger_mode = i;
                printf("\nDT TRIG: a GET triggers a reading (Y/N)? ");
                cfg->dt_trig = (toupper(getch()) == 'Y');
                pause = 0;
                break;
            case '7':
                /* Up to 100% of the range (p. 3-15); 0 turns NULL off */
                if (dm5010_ask("NULL offset in V or ohms (0 = off): ", &a)) {
                    b = n ? fs[cfg->range_mode ? cfg->range_mode - 1 : n - 1] : 2.0f;
                    if (fabs(a) > b) {
                        printf("Beyond the %s range (%g)\n", cfg->range_mode ? "selected" : "top", b);
                    } else {
                        cfg->nullval = a;
                        cfg->null_enabled = (a != 0.0f);
                        pause = 0;
                    }
                }
                break;
            case '8':
                if (dm5010_ask("Readings per average, AVE N (1-255, 0 = averaging off): ", &a)) {
                    if (a < 0.0f || a > 255.0f) {
                        printf("N runs 1-255 here\n");
                        break;
                    }
                    cfg->filter_enabled = (a >= 1.0f);
                    if (a >= 1.0f) cfg->avg_count = (unsigned char)a;   /* the meter truncates too (p. 3-6) */
                    if (dm5010_result_ms(slot) > 60000L)
                        printf("One result now takes %ld s; reads time out at 64 s\n", dm5010_result_ms(slot) / 1000L);
                    else
                        pause = 0;
                }
                break;
            case '9':
                if (dm5010_ask("Scale factor A (0 = ratio off): ", &a)) {
                    cfg->ratio_on = (a != 0.0f);
                    if (a != 0.0f) {
                        cfg->scale_factor = a;
                        if (dm5010_ask("Offset B: ", &b)) cfg->scale_offset = b;
                    }
                    pause = 0;
                }
                break;
            case 'A':
                printf("0. OFF\n1. DBM (1 mW in 600 ohm, .7746 V; p. 3-7)\n2. DBR (relative to a reference)\n\ndB: ");
                i = getch() - '0';
                printf("\n");
                if (i == 0 || i == 1) cfg->calculation_mode = i ? 3 : 0;
                if (i == 2) {
                    if (!dm5010_ask("DBR reference (any number but 0): ", &a)) break;
                    if (a == 0.0f) {
                        printf("The reference can't be 0 (p. 3-8)\n");
                        break;
                    }
                    cfg->dbr_reference = a;
                    cfg->calculation_mode = 4;
                }
                pause = 0;
                break;
            case 'B':
                printf("With compare on, a reading is 1 below, 2 between, 3 above the limits.\n");
                printf("The limits also feed the MONITOR SRQ (C).\n\n");
                if (!dm5010_ask("Upper limit: ", &a) || !dm5010_ask("Lower limit: ", &b)) break;
                cfg->limit_hi = a;
                cfg->limit_lo = b;
                printf("Compare readings with the limits (Y/N)? ");
                cfg->cmpr_on = (toupper(getch()) == 'Y');
                pause = 0;
                break;
            case 'C':
                printf("1. RQS %-4s all service requests\n", d10_arg(cfg, 9));
                printf("2. OPC %-4s SRQ when a reading is available\n", d10_arg(cfg, 6));
                printf("3. OVER %-4s SRQ on over-range\n", d10_arg(cfg, 7));
                printf("4. USER %-4s SRQ when INST ID is pressed\n", d10_arg(cfg, 8));
                printf("5. MONITOR %-4s SRQ on the first reading outside the limits\n", d10_arg(cfg, 5));
                printf("\nToggle: ");
                i = getch() - '1';
                if (i >= 0 && i < 5) {
                    static const unsigned char __far srq_k[] = { 9, 6, 7, 8, 5 };
                    d10_set_flag(cfg, srq_k[i], !d10_flag(cfg, srq_k[i]));
                }
                pause = 0;
                break;
            case 'D':
                printf("Applying settings to DM5010...\n");
                i = dm5010_apply(slot);
                printf("%s\n", i == 0 ? "Settings applied, no errors" : i > 0 ? "The meter reported the errors above" :
                                 "No reply from the meter: check the address and cable");
                break;
            case 'E':
                if (!dm5010_query(address, "SET?") || dm5010_reply[0] == '\0') {
                    printf("No reply\n");
                    break;
                }
                printf("%s\n", dm5010_reply);
                if (dm5010_confirm("Copy these into this slot's settings")) {
                    dm5010_load_settings(slot, dm5010_reply);
                    printf("\n");
                    pause = 0;
                }
                break;
            case 'F':
                i = dm5010_read(address, slot, &v);
                dm5010_format(slot, i, v, txt);
                printf("Reading: %s\n", txt);
                if (dm5010_live[slot].funct[0]) printf("Meter function: %s\n", dm5010_live[slot].funct);
                break;
            case 'G':
                test_dm5010_comm(address);
                pause = 0;
                break;
            case 'H':
                if (cfg->statistics_enabled && dm5010_min[slot] <= dm5010_max[slot])
                    printf("Min %g, Max %g %s\n\n", dm5010_min[slot], dm5010_max[slot], dm5010_unit(slot));
                printf("T = turn %s, C = clear, other = return: ", cfg->statistics_enabled ? "off" : "on");
                i = toupper(getch());
                if (i == 'T') cfg->statistics_enabled = !cfg->statistics_enabled;
                if (i == 'T' || i == 'C') dm5010_stats_clear(slot);
                pause = 0;
                break;
            case 'I':
                dm5010_utilities_menu(slot);
                pause = 0;
                break;
            case '0':
            case 27:
                done = 1;
                pause = 0;
                break;
            default:
                pause = 0;
                break;
        }
        if (pause) {
            printf("\nPress any key to continue...");
            getch();
        }
    }
}

/* Each step counts only when its reply checks out. No INIT, so the user's
   front-panel setup survives. */
void test_dm5010_comm(int address) {
    int pass = 0, n;
    double v;
    unsigned char stb;

    printf("\n=== DM5010 Communication Test ===\n");
    printf("Testing DM5010 at GPIB address %d\n\n", address);

    printf("1. Identification (ID?)\n");
    if (dm5010_query(address, "ID?") && strstr(dm5010_reply, "DM5010")) {
        printf("   %s\n", dm5010_reply);
        pass++;
    } else {
        printf("   %s%s\n", dm5010_reply[0] ? "Not a DM5010: " : "No reply", dm5010_reply);
    }

    printf("2. Function (FUNCT?)\n");
    if (dm5010_query(address, "FUNCT?") && dm5010_funct_index(dm5010_reply) >= 0) {
        printf("   %s\n", dm5010_reply);
        pass++;
    } else {
        printf("   Bad or no reply: %s\n", dm5010_reply);
    }

    printf("3. Reading (SEND)\n");
    if (dm5010_query(address, "SEND") && strstr(dm5010_reply, "E+99")) {
        printf("   Over-range (%s)\n", dm5010_reply);
        pass++;
    } else if (dm5010_reply[0] && sscanf(dm5010_reply, "%lf", &v) == 1) {
        printf("   %g\n", v);
        pass++;
    } else {
        printf("   No reading\n");
    }

    printf("4. Calibration checksum (TEST)\n");
    if (dm5010_query(address, "TEST") && strncmp(dm5010_reply, "TEST", 4) == 0 &&
        dm5010_reply_number(dm5010_reply, &v)) {
        printf("   %s - %s\n", dm5010_reply, v == 0.0 ? "correct" : "calibration checksum error");
        if (v == 0.0) pass++;
    } else {
        printf("   No reply: %s\n", dm5010_reply);
    }

    printf("5. Status (serial poll, ERR?)\n");
    if (ieee_spoll(address, &stb) == 0) {
        printf("   Status byte %u\n", stb);
        n = dm5010_show_errors(address, NULL);
        if (n == 0) pass++;
        else if (n < 0) printf("   ERR? not answered\n");
    } else {
        printf("   Serial poll failed\n");
    }

    printf("\n=== Test Summary ===\n");
    printf("Tests passed: %d/5\n", pass);
    printf("DM5010 communication: %s\n", pass == 5 ? "GOOD" : pass >= 3 ? "PARTIAL - see above" :
                                         "POOR - check address and connections");
    printf("\nPress any key to continue...");
    getch();
}

/* PS5004 power supply: Tek Codes & Formats (manual section 3). Query replies
   carry their header ("VOLTAGE 5.0000;") and end in CR LF (manual p. 3-16). */

/* SET? and HELP? run to about 120 bytes plus CR LF (manual p. 3-8, 3-12) */
static char __far ps5004_reply[160];

/* DISPLAY arguments for display_mode 0-2 (manual p. 3-6) */
static const char * __far ps5004_disp_arg[3] = { "VOLTAGE", "CURRENT", "CLIMIT" };

/* On/off settings, ps5004_flag() order; SET? names the output OUT (manual p. 3-12) */
static const char * __far ps5004_flag_hdr[PS5004_NFLAGS] = {
    "OUT", "VRI", "CRI", "URI", "DT", "USER", "RQS"
};

/* Last DISPLAY sent per slot, -1 unknown. A display change holds SEND for two
   DVM readings, about 400 ms at 5 per second (manual p. 3-6, 1-3). */
static signed char __far ps5004_disp_sent[10] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

/* Power-on / INIT settings, manual Table 3-2 */
void init_ps5004_config(int slot) {
    ps5004_config *cfg = &g_ps5004_config[slot];
    int i;

    cfg->voltage = 0.0;
    cfg->current_limit = 0.1;
    cfg->display_mode = 0;
    for (i = 0; i < PS5004_NFLAGS; i++) ps5004_set_flag(cfg, i, i == PS5004_RQS);
    cfg->dt_set = 0;
    cfg->lf_termination = 0;
    ps5004_disp_sent[slot] = -1;
}

int ps5004_flag(const ps5004_config *cfg, int i) {
    switch (i) {
        case 0:  return cfg->output_enabled;
        case 1:  return cfg->vri_enabled;
        case 2:  return cfg->cri_enabled;
        case 3:  return cfg->uri_enabled;
        case 4:  return cfg->dt_enabled;
        case 5:  return cfg->user_enabled;
        default: return cfg->rqs_enabled;
    }
}

void ps5004_set_flag(ps5004_config *cfg, int i, int on) {
    on = on != 0;
    switch (i) {
        case 0:  cfg->output_enabled = on; break;
        case 1:  cfg->vri_enabled = on; break;
        case 2:  cfg->cri_enabled = on; break;
        case 3:  cfg->uri_enabled = on; break;
        case 4:  cfg->dt_enabled = on; break;
        case 5:  cfg->user_enabled = on; break;
        default: cfg->rqs_enabled = on; break;
    }
}

/* "VRI ON", "OUTPUT OFF" ...; the output command is OUTput (manual p. 3-10) */
void ps5004_send_flag(int address, int i, int on) {
    sprintf(gpib_cmd_buffer, "%s %s", i == PS5004_OUT ? "OUTPUT" : ps5004_flag_hdr[i], on ? "ON" : "OFF");
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

/* Setting resolution 0.5 mV and 2.5 mA; the supply rounds to it (manual p. 3-5,
   3-14, 3-17), so the screen shows what it will really use */
float ps5004_round_voltage(float v) {
    return (float)(floor(v / 0.0005 + 0.5) * 0.0005);
}

float ps5004_round_current(float a) {
    return (float)(floor(a / 0.0025 + 0.5) * 0.0025);
}

/* Reply bytes as received (binary blocks too); returns the count, 0 if none.
   Talked to with nothing queued, the supply sends one all-ones byte (p. 3-18). */
int ps5004_query_block(int address, const char *query, char **reply) {
    int n;

    *reply = ps5004_reply;
    ps5004_reply[0] = '\0';
    if (gpib_write(address, (char *)query) != 0) return 0;
    GPIB_PACE(50);
    n = gpib_read(address, ps5004_reply, sizeof(ps5004_reply));
    /* A full buffer may leave a tail in Driver488 that would pass as the next reply */
    if (n >= (int)sizeof(ps5004_reply) - 1) drain_input_buffer();
    if (n <= 0 || (unsigned char)ps5004_reply[0] == 0xFF) {
        ps5004_reply[0] = '\0';
        return 0;
    }
    return n;
}

/* Text reply without its CR LF, or NULL if none came */
char *ps5004_query(int address, const char *query) {
    char *r;

    if (ps5004_query_block(address, query, &r) == 0) return NULL;
    r[strcspn(r, "\r\n")] = '\0';
    return r;
}

/* Number after an optional header: "VOLTAGE 5.0000;", "ERR 205;", "50.0E-3".
   The manual prints a space before the exponent ("CURRENT <numeric> E-3;",
   "<current> E-3", p. 3-5, 3-12), so a detached exponent is read as well. */
int ps5004_parse_number(const char *s, float *value) {
    double v;
    int n = 0, e;

    while (*s && !isdigit((unsigned char)*s) && *s != '-' && *s != '+' && *s != '.') s++;
    if (sscanf(s, "%lf%n", &v, &n) != 1) return 0;
    s += n;
    while (*s == ' ') s++;
    if ((*s == 'E' || *s == 'e') && sscanf(s + 1, "%d", &e) == 1 && e > -20 && e < 20) {
        for (; e > 0; e--) v *= 10.0;
        for (; e < 0; e++) v /= 10.0;
    }
    *value = (float)v;
    return 1;
}

static int ps5004_query_value(int address, const char *query, float *value) {
    char *r = ps5004_query(address, query);
    return r != NULL && ps5004_parse_number(r, value);
}

/* Next DVM reading, volts or amperes per DISPLAY (SEND, manual p. 3-12).
   0 on no reply, so a failed read can't pass for a 0 V reading. */
int ps5004_read_value(int address, float *value) {
    return ps5004_query_value(address, "SEND", value);
}

/* DISPLAY for mode 0-2, sent only when the supply isn't known to show it */
void ps5004_select_display(int slot, int mode) {
    if (mode < 0 || mode > 2) mode = 0;
    if (ps5004_disp_sent[slot] == mode) return;
    sprintf(gpib_cmd_buffer, "DISPLAY %s", ps5004_disp_arg[mode]);
    ps5004_disp_sent[slot] = gpib_write(g_system->modules[slot].gpib_address, gpib_cmd_buffer) == 0 ? mode : -1;
    GPIB_PACE(50);
}

void ps5004_forget_display(int slot) {
    ps5004_disp_sent[slot] = -1;
}

/* What the supply shows, from "DISPLAY CLIMIT;" etc. (manual p. 3-6); -1 no reply */
int ps5004_display_shown(int address) {
    char *r = ps5004_query(address, "DISPLAY?");
    if (r == NULL) return -1;
    return strstr(r, "CL") ? 2 : strstr(r, "CU") ? 1 : 0;
}

/* One reading of what mode selects. Called from the monitor: no printf. */
int ps5004_measure(int slot, int mode, float *value) {
    ps5004_select_display(slot, mode);
    return ps5004_read_value(g_system->modules[slot].gpib_address, value);
}

/* INIT is a setting command: error 201 in local (manual p. 3-19) */
void ps5004_init(int address) {
    gpib_remote(address);
    gpib_write(address, "INIT");
    GPIB_PACE(100);
}

void ps5004_set_voltage(int address, float voltage) {
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 20.0) voltage = 20.0;
    sprintf(gpib_cmd_buffer, "VOLTAGE %.4f", voltage);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

/* Amperes with 4 places: 12.5 mA must go as 0.0125, not 0.013 */
void ps5004_set_current(int address, float current) {
    if (current < 0.01) current = 0.01;
    if (current > 0.305) current = 0.305;
    sprintf(gpib_cmd_buffer, "CURRENT %.4f", current);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}

void ps5004_set_output(int address, int on) {
    ps5004_send_flag(address, PS5004_OUT, on);
}

/* REGULATION? 1 = CV, 2 = CC, 3 = unregulated (manual p. 3-11); 0 no reply */
int ps5004_get_regulation_status(int address) {
    float v;
    return ps5004_query_value(address, "REGULATION?", &v) ? (int)v : 0;
}

const char *ps5004_regulation_name(int status) {
    switch (status) {
        case 1:  return "CV (voltage regulation)";
        case 2:  return "CC (current regulation)";
        case 3:  return "UR (unregulated)";
        default: return "no reply";
    }
}

/* ERROR? codes, manual Table 3-1 */
static const struct { int code; const char *text; } __far ps5004_errors[] = {
    { 101, "Command header error" },
    { 102, "Header delimiter error" },
    { 103, "Command argument error" },
    { 106, "Missing argument" },
    { 107, "Invalid message unit delimiter" },
    { 108, "Checksum error" },
    { 109, "Byte count error" },
    { 201, "Not executable in local mode" },
    { 202, "Returned to local, pending settings lost" },
    { 203, "I/O buffers full, output dumped" },
    { 205, "Argument out of range" },
    { 206, "GET ignored (DT OFF, local or busy)" },
    { 302, "System error" },
    { 303, "Math pack error" },
    { 311, "Measurement not complete" },
    { 401, "Power on" },
    { 403, "User request (INST ID button)" },
    { 724, "Voltage regulation" },
    { 725, "Current regulation or unregulated" }    /* Table 3-1 gives 725 for both */
};

static const char *ps5004_error_text(int code) {
    int i;
    for (i = 0; i < sizeof(ps5004_errors) / sizeof(ps5004_errors[0]); i++)
        if (ps5004_errors[i].code == code) return ps5004_errors[i].text;
    return "Unknown code";
}

/* Print pending errors and events; returns how many. With RQS ON each event is
   reported by serial poll and ERROR? then names it; with RQS OFF ERROR? returns
   and clears the highest-priority one, 0 when none is left (manual p. 3-7,
   3-20, 3-21). Either way this clears the power-on SRQ. */
int ps5004_show_errors(int address, int print) {
    unsigned char stb = 0;
    char *r = ps5004_query(address, "RQS?");
    int rqs_on = r == NULL || strstr(r, "RQS ON") != NULL;
    int i, code, shown = 0;
    float v;

    for (i = 0; i < 8; i++) {
        if (rqs_on && (ieee_spoll(address, &stb) != 0 || !(stb & 0x40))) break;
        if (!ps5004_query_value(address, "ERROR?", &v)) break;
        code = (int)v;
        if (code <= 0) break;
        /* Device-status STB 203 (219 busy) is the unregulated one of the two 725s (Fig. 3-3) */
        if (print) printf("  %s %d: %s\n", code >= 400 ? "EVENT" : "ERROR", code,
                          code == 725 && (stb & 0x83) == 0x83 ? "Unregulated" : ps5004_error_text(code));
        shown++;
    }
    return shown;
}

/* Under DT SET every setting, DT OFF included, waits for GET (manual p. 3-6):
   send DT OFF and GET so the commands after this one take effect. DT? lists
   only ON and OFF (p. 3-6), so SET reads as ON. A plain DT ON supply answers
   the GET with error 206. */
void ps5004_release_dt(int address) {
    char *r = ps5004_query(address, "DT?");
    if (r == NULL || strstr(r, "ON") == NULL) return;
    gpib_remote(address);
    ps5004_send_flag(address, PS5004_DT, 0);
    ps5004_trigger(address);
}

/* Serial-poll until no event is pending, quietly: the power-up SRQ must be
   cleared before continuing (manual p. 3-21) */
void ps5004_clear_srq(int address) {
    unsigned char stb;
    int i;
    for (i = 0; i < 8; i++)
        if (ieee_spoll(address, &stb) != 0 || !(stb & 0x40)) break;
}

/* GET (manual p. 3-18) runs settings held by DT SET; both GPIB back ends
   take Driver488's TRIGGER command. In local GET is ignored (206), so go
   remote first. */
void ps5004_trigger(int address) {
    char cmd[16];
    gpib_remote(address);
    sprintf(cmd, "TRIGGER %2d\r\n", address);
    ieee_write(cmd);
    GPIB_PACE(50);
}

/* Load a SET? reply into cfg: "VOLTAGE 5.0000;CURRENT 100.0 E-3;OUT OFF;DISPLAY
   VOLTAGE;VRI OFF;CRI OFF;URI OFF;DT OFF;USER OFF;RQS ON;" (manual p. 3-12).
   Splits s in place; returns the number of the 10 fields found. */
int ps5004_parse_settings(char *s, ps5004_config *cfg) {
    char *tok, *arg;
    int i, n = 0, on;
    float v;

    for (tok = strtok(s, ";"); tok; tok = strtok(NULL, ";")) {
        while (*tok == ' ' || *tok == '\r' || *tok == '\n') tok++;
        if ((arg = strchr(tok, ' ')) == NULL) continue;
        *arg++ = '\0';
        while (*arg == ' ') arg++;
        on = strncmp(arg, "OFF", 3) != 0;
        if (strcmp(tok, "VOLTAGE") == 0 && ps5004_parse_number(arg, &v)) {
            cfg->voltage = v;
            n++;
        } else if (strcmp(tok, "CURRENT") == 0 && ps5004_parse_number(arg, &v)) {
            cfg->current_limit = v;
            n++;
        } else if (strcmp(tok, "DISPLAY") == 0) {
            for (i = 0; i < 3; i++)
                if (strncmp(arg, ps5004_disp_arg[i], 2) == 0) {
                    cfg->display_mode = i;
                    n++;
                }
        } else {
            /* Prefix match takes OUTPUT for OUT too */
            for (i = 0; i < PS5004_NFLAGS; i++)
                if (strncmp(tok, ps5004_flag_hdr[i], strlen(ps5004_flag_hdr[i])) == 0) {
                    ps5004_set_flag(cfg, i, on);
                    n++;
                }
            /* SET? reports DT SET as ON: keep the slot's SET/ON choice */
            if (strcmp(tok, "DT") == 0 && !on) cfg->dt_set = 0;
        }
    }
    return n;
}

/* Print the settings that differ between the supply (now) and the slot (want),
   within half a resolution step; returns how many */
int ps5004_compare(const ps5004_config *now, const ps5004_config *want) {
    int i, n = 0;

    if (fabs(now->voltage - want->voltage) > 0.00026) {
        printf("  VOLTAGE: supply %.4f V, slot %.4f V\n", now->voltage, want->voltage);
        n++;
    }
    if (fabs(now->current_limit - want->current_limit) > 0.0013) {
        printf("  CURRENT: supply %.1f mA, slot %.1f mA\n",
               now->current_limit * 1000, want->current_limit * 1000);
        n++;
    }
    if (now->display_mode != want->display_mode) {
        printf("  DISPLAY: supply %s, slot %s\n", ps5004_disp_arg[(unsigned)now->display_mode % 3],
               ps5004_disp_arg[(unsigned)want->display_mode % 3]);
        n++;
    }
    for (i = 0; i < PS5004_NFLAGS; i++) {
        if (ps5004_flag(now, i) != ps5004_flag(want, i)) {
            printf("  %s: supply %s, slot %s\n", ps5004_flag_hdr[i],
                   ps5004_flag(now, i) ? "ON" : "OFF", ps5004_flag(want, i) ? "ON" : "OFF");
            n++;
        }
    }
    return n;
}

/* One ';'-separated item per column, three to a line */
void ps5004_print_items(const char *s) {
    int len, col = 0;

    while (*s) {
        while (*s == ' ') s++;
        len = strcspn(s, ";");
        if (len > 0) {
            printf("  %-24.*s", len > 24 ? 24 : len, s);
            if (++col % 3 == 0) printf("\n");
        }
        s += len;
        if (*s) s++;
    }
    if (col % 3) printf("\n");
}

static int ps5004_slot_at(int address) {
    int i;
    for (i = 0; i < 10; i++)
        if (g_system->modules[i].enabled && g_system->modules[i].module_type == MOD_PS5004 &&
            g_system->modules[i].gpib_address == address) return i;
    return -1;
}

/* Queries only, unless the write test is accepted, and that re-sends the
   supply's own voltage and limit: neither the output nor the slot changes. */
void test_ps5004_comm(int address) {
    ps5004_config now;
    char *r;
    float v, a, v2, a2;
    int slot = ps5004_slot_at(address), mode, good = 0;

    printf("\nTesting PS5004 at address %d (queries only)...\n", address);

    r = ps5004_query(address, "ID?");
    printf("\nID?    %s\n", r ? r : "no reply");
    if (r && strncmp(r, "ID", 2) == 0) good++;

    /* ROM checksum: TEST 0 passes, TEST 394 fails (manual p. 3-13) */
    if (ps5004_query_value(address, "TEST", &v)) {
        printf("TEST   %s\n", v == 0 ? "ROM test passed" : "ROM test FAILED");
        if (v == 0) good++;
    } else {
        printf("TEST   no reply\n");
    }

    printf("\nPending errors/events:\n");
    if (ps5004_show_errors(address, 1) == 0) printf("  none\n");

    printf("\nSettings (SET?):\n");
    r = ps5004_query(address, "SET?");
    if (r) {
        ps5004_print_items(r);
        good++;
        if (slot >= 0) {
            now = g_ps5004_config[slot];
            ps5004_parse_settings(r, &now);
            if (ps5004_compare(&now, &g_ps5004_config[slot]) == 0)
                printf("Slot %d's settings match the supply\n", slot);
            else
                printf("Slot %d differs: L in the PS5004 menu loads the supply's settings,\n"
                       "D applies the slot's\n", slot);
        }
    } else {
        printf("  no reply\n");
    }

    mode = ps5004_display_shown(address);
    printf("\nRegulation: %s\n", ps5004_regulation_name(ps5004_get_regulation_status(address)));
    if (ps5004_read_value(address, &v)) {
        if (mode < 0) printf("Reading (SEND): %g (no reply to DISPLAY?)\n", v);
        else if (mode == 0) printf("Reading (SEND): %.4f V\n", v);
        else printf("Reading (SEND): %.1f mA%s\n", v * 1000, mode == 2 ? " current limit" : "");
        good++;
    } else {
        printf("Reading (SEND): no reply\n");
    }

    printf("\nWrite test: re-sends the supply's own VOLTAGE and CURRENT settings and\n");
    printf("reads them back. The output state does not change. Run it (Y/N)? ");
    if (toupper(getch()) == 'Y') {
        printf("\n");
        /* Setting commands give error 201 in local (manual p. 3-19) */
        gpib_remote(address);
        GPIB_PACE(200);
        if (ps5004_query_value(address, "VOLTAGE?", &v) && ps5004_query_value(address, "CURRENT?", &a)) {
            ps5004_set_voltage(address, v);
            ps5004_set_current(address, a);
            if (ps5004_query_value(address, "VOLTAGE?", &v2) && ps5004_query_value(address, "CURRENT?", &a2) &&
                fabs(v2 - v) < 0.00026 && fabs(a2 - a) < 0.0013) {
                printf("Wrote and read back %.4f V, %.1f mA\n", v2, a2 * 1000);
                good++;
            } else {
                printf("Read-back failed\n");
            }
        } else {
            printf("No reply to VOLTAGE?/CURRENT?\n");
        }
        ps5004_show_errors(address, 1);
    }

    printf("\n\nPS5004 communication: %s\n", good >= 4 ? "OK" : good > 0 ? "PARTIAL" : "FAILED");
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* PS5010 triple power supply */
void init_ps5010_config(int slot) {
    ps5010_config *cfg = &g_ps5010_config[slot];
    
    cfg->voltage1 = 0.0;        /* Positive supply (0 to +32V) */
    cfg->current_limit1 = 0.1;  /* 100mA default (50mA-750mA range, 1.6A @ ≤15V) */
    cfg->output1_enabled = 0;
    
    cfg->voltage2 = 0.0;        /* Negative supply (0 to -32V) */
    cfg->current_limit2 = 0.1;  /* 100mA default (50mA-750mA range, 1.6A @ ≤15V) */
    cfg->output2_enabled = 0;
    
    cfg->logic_voltage = 5.0;   /* Logic supply (4.5-5.5V range) */
    cfg->logic_current_limit = 1.0;  /* 1A default (100mA-3.0A range) */
    cfg->logic_enabled = 0;
    
    cfg->tracking_mode = 0;  /* Independent */
    cfg->tracking_ratio = 1.0;  /* 1:1 tracking */
    
    cfg->display_channel = 0;  /* Show channel 1 */
    cfg->display_mode = 0;     /* Show voltage */
    
    cfg->ovp_enabled = 0;
    cfg->ovp_level1 = 35.0;  /* Just above 32V max */
    cfg->ovp_level2 = 35.0;
    
    cfg->cv_mode1 = 1;
    cfg->cc_mode1 = 0;
    cfg->cv_mode2 = 1;
    cfg->cc_mode2 = 0;
    
    cfg->srq_enabled = 1;
    cfg->error_queue_size = 0;
    cfg->lf_termination = 0;  /* Default to CRLF */
}
void ps5010_init(int address) {
    gpib_write(address, "INIT");
    delay(500);  /* PS5010 needs time for reset */
}
void ps5010_set_voltage(int address, int channel, float voltage) {
    
    switch(channel) {
        case 1:  /* Positive floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 32.0) voltage = 32.0;
            sprintf(gpib_cmd_buffer, "VPOS %.1f", voltage);
            break;
        case 2:  /* Negative floating supply */
            if (voltage < 0.0) voltage = 0.0;
            if (voltage > 32.0) voltage = 32.0;
            sprintf(gpib_cmd_buffer, "VNEG %.1f", voltage);
            break;
        case 3:  /* Logic supply */
            if (voltage < 4.5) voltage = 4.5;
            if (voltage > 5.5) voltage = 5.5;
            sprintf(gpib_cmd_buffer, "VLOG %.1f", voltage);
            break;
        default:
            return;  /* Invalid channel */
    }
    
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void ps5010_set_current(int address, int channel, float current) {
    
    switch(channel) {
        case 1:  /* Positive supply */
            if (current < 0.050) current = 0.050;  /* 50mA minimum */
            if (current > 0.750) current = 0.750;  /* 750mA maximum (1.6A @ ≤15V) */
            sprintf(gpib_cmd_buffer, "IPOS %.3f", current);
            break;
        case 2:  /* Negative supply */
            if (current < 0.050) current = 0.050;  /* 50mA minimum */
            if (current > 0.750) current = 0.750;  /* 750mA maximum (1.6A @ ≤15V) */
            sprintf(gpib_cmd_buffer, "INEG %.3f", current);
            break;
        case 3:  /* Logic supply */
            if (current < 0.100) current = 0.100;  /* 100mA minimum */
            if (current > 3.0) current = 3.0;      /* 3.0A maximum */
            sprintf(gpib_cmd_buffer, "ILOG %.3f", current);
            break;
        default:
            return;
    }
    
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void ps5010_set_output(int address, int channel, int on) {
    
    switch(channel) {
        case 0:  /* All outputs */
            sprintf(gpib_cmd_buffer, "OUT %s", on ? "ON" : "OFF");
            break;
        case 1:  /* Floating supplies only */
            sprintf(gpib_cmd_buffer, "FSOUT %s", on ? "ON" : "OFF");
            break;
        case 2:  /* Logic supply only */
            sprintf(gpib_cmd_buffer, "LSOUT %s", on ? "ON" : "OFF");
            break;
        default:
            return;
    }
    
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
void ps5010_set_tracking_voltage(int address, float voltage) {
    
    if (voltage < 0.0) voltage = 0.0;
    if (voltage > 40.0) voltage = 40.0;
    
    sprintf(gpib_cmd_buffer, "VTRA %.1f", voltage);
    gpib_write(address, gpib_cmd_buffer);
    GPIB_PACE(50);
}
int ps5010_read_regulation(int address, int *neg_stat, int *pos_stat, int *log_stat) {
    
    
    gpib_write(address, "REG?");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        /* REG returns <neg>,<pos>,<logic> where:
           1 = voltage regulation
           2 = current regulation
           3 = unregulated */
        if (sscanf(gpib_response_buffer, "%d,%d,%d", neg_stat, pos_stat, log_stat) == 3) {
            return 1;  /* Success */
        }
    }
    
    return 0;  /* Failed */
}
int ps5010_get_settings(int address, char *buffer, int maxlen) {
    gpib_write(address, "SET?");
    GPIB_PACE(100);
    return gpib_read(address, gpib_response_buffer, maxlen);
}
int ps5010_get_error(int address) {
    
    int error_code = 0;
    
    gpib_write(address, "ERR?");
    GPIB_PACE(50);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        sscanf(gpib_response_buffer, "%d", &error_code);
    }
    
    return error_code;
}
void ps5010_set_interrupts(int address, int pri_on, int nri_on, int lri_on) {
    gpib_write(address, pri_on ? "PRI ON" : "PRI OFF");
    GPIB_PACE(50);
    gpib_write(address, nri_on ? "NRI ON" : "NRI OFF");
    GPIB_PACE(50);
    gpib_write(address, lri_on ? "LRI ON" : "LRI OFF");
    GPIB_PACE(50);
}
void ps5010_set_srq(int address, int on) {
    gpib_write(address, on ? "RQS ON" : "RQS OFF");
    GPIB_PACE(50);
}

void test_ps5010_comm(int address) {
    
    int neg_stat, pos_stat, log_stat;
    
    printf("\nTesting PS5010 at address %d...\n", address);
    
    printf("Setting REMOTE mode...\n");
    gpib_remote(address);
    GPIB_PACE(200);
    
    printf("\nResetting PS5010...\n");
    ps5010_init(address);
    
    printf("\nGetting ID...\n");
    gpib_write(address, "ID?");
    GPIB_PACE(100);
    
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    } else {
        printf("No ID response\n");
    }
    
    printf("\nSetting test voltages...\n");
    ps5010_set_voltage(address, 1, 5.0);   /* VPOS 5.0 */
    ps5010_set_voltage(address, 2, 12.0);  /* VNEG 12.0 */
    ps5010_set_current(address, 1, 0.1);   /* IPOS 0.1 */
    ps5010_set_current(address, 2, 0.1);   /* INEG 0.1 */
    ps5010_set_voltage(address, 3, 5.0);   /* VLOG 5.0 */
    ps5010_set_current(address, 3, 1.0);   /* ILOG 1.0 */
    
    printf("\nQuerying settings...\n");
    if (ps5010_get_settings(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nReading regulation status...\n");
    if (ps5010_read_regulation(address, &neg_stat, &pos_stat, &log_stat)) {
        printf("Negative supply: %s\n", 
               neg_stat == 1 ? "Voltage regulation (CV)" : 
               (neg_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
        printf("Positive supply: %s\n",
               pos_stat == 1 ? "Voltage regulation (CV)" : 
               (pos_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
        printf("Logic supply: %s\n",
               log_stat == 1 ? "Voltage regulation (CV)" : 
               (log_stat == 2 ? "Current regulation (CC)" : "Unregulated"));
    }
    
    printf("\nTesting VTRA (tracking voltage) command...\n");
    ps5010_set_tracking_voltage(address, 10.0);
    GPIB_PACE(200);
    printf("Set both POS and NEG to 10V using VTRA\n");
    
    printf("\nChecking error status...\n");
    gpib_write(address, "ERR?");
    GPIB_PACE(50);
    if (gpib_read(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        int error_code = atoi(gpib_response_buffer);
        if (error_code == 0) {
            printf("No errors\n");
        } else {
            printf("Error code: %d\n", error_code);
        }
    }
    
    printf("\nTesting output control...\n");
    ps5010_set_output(address, 0, 0);  /* All outputs OFF */
    printf("All outputs OFF\n");
    delay(500);
    
    ps5010_set_output(address, 1, 1);  /* Floating supplies ON */
    printf("Floating supplies ON\n");
    delay(500);
    
    ps5010_set_output(address, 2, 1);  /* Logic supply ON */
    printf("Logic supply ON\n");
    
    printf("\nFinal settings check...\n");
    if (ps5010_get_settings(address, gpib_response_buffer, sizeof(gpib_response_buffer)) > 0) {
        printf("   %s\n", gpib_response_buffer);
    }
    
    printf("\nNote: PS5010 cannot measure output voltage/current.\n");
    printf("It can only report regulation status (CV/CC/UR).\n");
    printf("Use an external DMM to verify actual output values.\n");
    
    printf("\nTest complete. Press any key to continue...");
    getch();
}

/* Single-shot and continuous measurement across all configured slots */
void single_measurement(void) {
    int i;
    double value;
    
    int active_modules[10];
    int active_count = 0;
    
    clrscr();
    printf("Single Measurement - Multi-Module Coordination\n");
    printf("==============================================\n\n");

    for (i = 0; i < 10; i++) {
        if (g_system->modules[i].enabled) {
            active_modules[active_count] = i;
            active_count++;
        }
    }
    
    if (active_count == 0) {
        printf("No modules configured!\n\n");
        printf("Press any key to continue...");
        getch();
        return;
    }
    
    printf("Synchronizing %d modules for simultaneous measurement...\n\n", active_count);
    
    /* Arm counters and INIT meters one by one before reading */
    for (i = 0; i < active_count; i++) {
        int slot = active_modules[i];
        int addr = g_system->modules[slot].gpib_address;
        
        switch (g_system->modules[slot].module_type) {
            case MOD_DC5009:
            case MOD_DC5010:
                dc50xx_single(slot, 0, &value);   /* arm every counter, then read */
                break;
                
            case MOD_DM5010:
                /* No INIT: it resets the function and range (p. 3-26); SEND triggers */
                break;

            case MOD_DM5120:
                gpib_write(addr, "INIT");
                break;
                
            case MOD_PS5004:
            case MOD_PS5010:
                /* Power supplies don't need INIT for reading */
                break;
        }
    }
    
    if (active_count > 0) {
        ieee_write("*OPC?\r\n");
        delay(100); /* Allow time for all modules to complete */
        drain_input_buffer();
    }
    
    printf("Collecting results:\n");
    printf("-------------------\n");
    
    for (i = 0; i < active_count; i++) {
        int slot = active_modules[i];
        printf("Slot %d (%s at GPIB %d): ", slot, g_system->modules[slot].description, g_system->modules[slot].gpib_address);
        
        switch (g_system->modules[slot].module_type) {
            case MOD_DC5009:
            case MOD_DC5010:
                {
                    char cbuf[32];
                    int r = dc50xx_single(slot, 1, &value);
                    if (r == 1) {
                        dc50xx_format(cbuf, slot, value);
                        printf("%s\n", cbuf);
                        g_system->modules[slot].last_reading = value;
                    } else {
                        printf(r ? "No reply\n" : "No result (no signal, or TOT/TMAN still counting)\n");
                    }
                }
                break;
                
            case MOD_DM5010:
                {
                    char txt[32];
                    int r = dm5010_read(g_system->modules[slot].gpib_address, slot, &value);
                    dm5010_format(slot, r, value, txt);
                    printf("%s\n", txt);
                    if (r == DM5010_READ_OK) g_system->modules[slot].last_reading = value;
                }
                break;
                
            case MOD_DM5120:
                value = read_dm5120_enhanced(g_system->modules[slot].gpib_address, slot);
                if (value == 0.0) {
                    /* 0.0 on a failed read: retry with the plain voltage read */
                    value = read_dm5120_voltage(g_system->modules[slot].gpib_address);
                }
                printf("%.6g V\n", value);
                g_system->modules[slot].last_reading = value;
                break;
                
            case MOD_PS5010:
                /* Power supply regulation status */
                {
                    int neg_stat, pos_stat, log_stat;
                    if (ps5010_read_regulation(g_system->modules[slot].gpib_address, &neg_stat, &pos_stat, &log_stat)) {
                        printf("POS:%s NEG:%s LOG:%s\n",
                               pos_stat == 1 ? "CV" : (pos_stat == 2 ? "CC" : "UR"),
                               neg_stat == 1 ? "CV" : (neg_stat == 2 ? "CC" : "UR"),
                               log_stat == 1 ? "CV" : (log_stat == 2 ? "CC" : "UR"));
                    } else {
                        printf("Communication Error\n");
                    }
                }
                break;
                
            case MOD_PS5004:
                /* SEND reads what DISPLAY selects (manual p. 3-6), so select it first.
                   One-shot: always resend it, the front panel may have changed it. */
                {
                    int mode = g_ps5004_config[slot].display_mode;
                    int reg_status = ps5004_get_regulation_status(g_system->modules[slot].gpib_address);
                    float reading;

                    ps5004_forget_display(slot);
                    if (!ps5004_measure(slot, mode, &reading)) {
                        printf("No reply\n");
                        break;
                    }
                    value = reading;
                    if (mode == 1) printf("%.1f mA ", value * 1000);
                    else if (mode == 2) printf("limit %.1f mA ", value * 1000);
                    else printf("%.4f V ", value);
                    printf("%s\n", ps5004_regulation_name(reg_status));
                    g_system->modules[slot].last_reading = value;
                }
                break;

            case MOD_FG5010:
                /* A source: report what it is programmed to */
                {
                    char state[60];
                    fg5010_state_text(g_system->modules[slot].gpib_address, state);
                    printf("%s\n", state);
                }
                break;

            default:
                printf("Unknown module type\n");
                break;
        }
    }
    
    printf("\nPress any key to continue...");
    getch();
}
/* ---- Monitor display: build each screen line in a buffer and write it
 * straight into text-mode video memory (vputs, graphics.c); printf through
 * DOS + BIOS teletype is slow on the 286 and flickers.  Non-sampling passes
 * redraw the last sample's exact text so the value keeps one format.     */
static char mon_buf[200];
static int  mon_len, mon_row;
static char mon_last[10][60];     /* text after the module prefix, last sample */
static int  mon_pfx;              /* length of the "S0 DC5009[29]:" prefix    */

static void lp(const char *fmt, ...)
{
    va_list ap;
    if (mon_len > 150) return;
    va_start(ap, fmt);
    mon_len += vsprintf(mon_buf + mon_len, fmt, ap);
    va_end(ap);
}

static void lp_nl(void)
{
    mon_buf[mon_len] = '\0';
    vputs(mon_row++, 1, mon_buf, 79);
    mon_len = 0;
    mon_buf[0] = '\0';
}

void continuous_monitor(void) {
    int i, done = 0;
    double value;
    time_t start_time = time(NULL);
    time_t current_time;
    unsigned long last_tick_count = 0;
    unsigned long current_tick_count;
    unsigned long tick_start, tick_end;
    unsigned long ticks_per_sample;
    unsigned long measurement_ticks = 0;
    ps5004_config *ps_cfg;
    char type_str[20];
    int key;
    int need_sample = 0;
    int samples_taken = 0;
    int active_modules = 0;
    int should_monitor;
    int display_update_counter = 0;
    /* AUTO rate: time the first AUTO_CAL_PASSES samples taken back-to-back */
#define AUTO_CAL_PASSES 3
    int auto_cal = 0;
    unsigned long auto_sum = 0;
    unsigned long mod_ticks[10];
    unsigned long t_mod;
    /* "Time" = measurement time: BIOS ticks accumulated only while RUNNING */
    unsigned long run_ticks = 0, run_mark, run_now, run_ms;
    unsigned long passes = 0;                    /* readings per slot this run */
    unsigned int pool_size = MAX_SAMPLES_PER_MODULE;
    int avg_shown = -1;
    int mon_rows_used = 0;                       /* rows drawn on the last pass */
    unsigned long mclock_base = g_mclock + 1;   /* measurement clock keeps rising */
    
    for (i = 0; i < 10; i++) { mod_ticks[i] = 0; mon_last[i][0] = '\0'; }
    
    validate_enabled_modules();

    /* Buffer pooling: the modules this run reads share the memory of ten
       1024-sample slots - 1 module: 8192 (POOLED_MAX), 2: 5120 each,
       3: 3413 ... 10: 1024 - unless g_buffer_samples sets the size.  Buffers
       are cleared at the start of a run anyway, so they can be resized here.
       If memory runs short the size is halved until everything fits. */
    {
        int nmon = 0;
        for (i = 0; i < 10; i++)
            if (g_system->modules[i].enabled && !module_is_result(i) &&
                (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))))
                nmon++;
        if (nmon > 0) {
            unsigned long want = g_buffer_samples ? (unsigned long)g_buffer_samples
                                                  : POOL_SAMPLES / nmon;
            unsigned int size;
            int ok;
            if (want > POOLED_MAX) want = POOLED_MAX;
            if (want < MAX_SAMPLES_PER_MODULE) want = MAX_SAMPLES_PER_MODULE;
            size = (unsigned int)want;
            do {
                ok = 1;
                for (i = 0; i < 10; i++) {
                    if (g_system->modules[i].enabled && !module_is_result(i) &&
                        (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))) &&
                        g_system->modules[i].module_data_size != size) {
                        free_module_buffer(i);       /* free first: less fragmentation */
                    }
                }
                for (i = 0; i < 10 && ok; i++) {
                    if (g_system->modules[i].enabled && !module_is_result(i) &&
                        (g_control_panel.monitor_all || (g_control_panel.monitor_mask & (1 << i))) &&
                        !g_system->modules[i].module_data) {
                        if (!allocate_module_buffer(i, size) || !g_system->modules[i].sample_t) ok = 0;
                    }
                }
                if (!ok) size /= 2;
            } while (!ok && size >= 256);
            pool_size = size;
        }
    }
    
    for (i = 0; i < 10; i++) {
        /* Skip computed-result traces (FFT/math): they have no GPIB instrument to
           read, and clearing them here would erase the computed result. */
        if (g_system->modules[i].enabled && !module_is_result(i)) {
            active_modules++;
            if (!g_system->modules[i].module_data) {
                if (!allocate_module_buffer(i, MAX_SAMPLES_PER_MODULE)) {
                    printf("WARNING: Failed to allocate buffer for slot %d!\n", i);
                }
            }
            clear_module_data(i);
        }
    }
    
    g_system->data_count = 0;
    g_store_suppress = 1;      /* one sample per slot per pass, stored below */

    /* BIOS ticks run at 18.2 Hz: ticks = ms * 182 / 10000 */
    ticks_per_sample = (g_control_panel.sample_rate_ms * 182L) / 10000L;
    if (ticks_per_sample < 1) ticks_per_sample = 1;
    
    clrscr();
    printf("Continuous Monitor - Press SPACE to start/stop, ESC to exit\n");
    if (g_control_panel.auto_rate)
        printf("Sample rate: AUTO (measured when started), %d active modules          \n",
               active_modules);
    else
        printf("Sample rate: %d ms (%lu ticks), %d active modules\n", 
               g_control_panel.sample_rate_ms, ticks_per_sample, active_modules);
    printf("Commands: C=Clear data      Buffer: %u samples per module%s\n", pool_size,
           g_buffer_samples ? (pool_size < g_buffer_samples ? " (less memory free)" : "")
                            : (pool_size > MAX_SAMPLES_PER_MODULE ? " (pooled)" : ""));
    printf("============================================================\n\n");
    
    last_tick_count = *((unsigned long far *)0x0040006CL);
    run_mark = last_tick_count;
    
    while (!done) {
        current_time = time(NULL);
        mon_row = 5; mon_len = 0;
        run_now = *((unsigned long far *)0x0040006CL);
        run_ms = run_ticks + (g_control_panel.running ? run_now - run_mark : 0);
        run_ms = (run_ms / 182L) * 10000L + ((run_ms % 182L) * 10000L) / 182L; /* ticks to ms without 32-bit overflow */
        lp("Time: %5lu.%lu s  Readings: %lu  Status: %-8s  ", 
               run_ms / 1000L, (run_ms / 100L) % 10L, passes,
               g_control_panel.running ? "RUNNING" : "STOPPED");
        
        /* OH: last pass's measurement overhead in ms */
        if (measurement_ticks > 0 && g_control_panel.running) {
            lp("OH:%lu ", (measurement_ticks * 10000L) / 182L);
        }
        lp_nl(); mon_row++;           /* row 6 belongs to the AVG light */
        {   /* AVG light: a buffer is full and its samples are now averaged */
            int sh = 0;
            for (i = 0; i < 10; i++)
                if (g_system->modules[i].enabled && g_system->modules[i].avg_shift > sh)
                    sh = g_system->modules[i].avg_shift;
            if (sh != avg_shown) {
                char lt[80];
                if (sh > 0)
                    sprintf(lt, " AVG x%-5u  buffer full - each sample is the mean of %u readings",
                            1u << sh, 1u << sh);
                else
                    lt[0] = '\0';
                vputs(6, 1, lt, 79);
                vattr(6, 1, 12, sh > 0 ? 0x70 : 0x07);   /* reverse video = lit */
                avg_shown = sh;
            }
        }
        
        current_tick_count = *((unsigned long far *)0x0040006CL);
        
        /* Subtract the last pass's measurement time so the sample period holds;
           AUTO rate samples back-to-back until calibrated */
        if (g_control_panel.running) {
            unsigned long adjusted_ticks = ticks_per_sample;
            if (measurement_ticks > 0 && measurement_ticks < ticks_per_sample) {
                adjusted_ticks = ticks_per_sample - measurement_ticks;
            }
            
            if ((g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES) ||
                (current_tick_count - last_tick_count) >= adjusted_ticks) {
                need_sample = 1;
                last_tick_count = current_tick_count;
                tick_start = current_tick_count;
            }
        }
        
        samples_taken = 0;
        for (i = 0; i < 10; i++) {
            /* Skip phantoms and computed traces: need type, address 1-30, description */
            if (g_system->modules[i].enabled &&
                g_system->modules[i].module_type != MOD_NONE &&
                g_system->modules[i].gpib_address >= 1 &&
                g_system->modules[i].gpib_address <= 30 &&
                strlen(g_system->modules[i].description) > 0) {
                
                should_monitor = g_control_panel.monitor_all || 
                                (g_control_panel.monitor_mask & (1 << i));
                
                if (!should_monitor && g_control_panel.running) {
                    continue;
                }

                switch(g_system->modules[i].module_type) {
                    case MOD_DC5009: strcpy(type_str, "DC5009"); break;
                    case MOD_DM5010: strcpy(type_str, "DM5010"); break;
                    case MOD_DM5120: 
                        strcpy(type_str, "DM5120");
                        if (g_dm5120_config[i].buffer_enabled) {
                            strcat(type_str, "*");
                        }
                        break;
                    case MOD_PS5004: strcpy(type_str, "PS5004"); break;
                    case MOD_PS5010: strcpy(type_str, "PS5010"); break;
                    case MOD_DC5010: strcpy(type_str, "DC5010"); break;
                    case MOD_FG5010: strcpy(type_str, "FG5010"); break;
                    default: strcpy(type_str, "Unknown"); break;
                }
                
                if (!should_monitor && !g_control_panel.monitor_all) {
                    strcat(type_str, "-");  /* - indicates not monitored */
                }
                
                lp("S%d %-6s[%2d]:", i, type_str, g_system->modules[i].gpib_address);
                mon_pfx = mon_len;
                
                t_mod = *((unsigned long far *)0x0040006CL);
                if (g_control_panel.running && need_sample && should_monitor) {
                    int no_reading = 0;          /* slot had no new result: keep the last point */
                    switch(g_system->modules[i].module_type) {
                        case MOD_DC5009:
                        case MOD_DC5010:
                            {
                                char cbuf[32];
                                int r;
                                if (!mon_last[i][0]) dc50xx_sync_function(i);   /* unit of this run */
                                r = dc50xx_read(i, &value);
                                if (r == 1) {
                                    dc50xx_format(cbuf, i, value);
                                    lp("%s", cbuf);
                                } else {
                                    /* no new result yet, or no reply: store nothing */
                                    no_reading = 1;
                                    value = g_system->modules[i].last_reading;
                                    lp("%s", r ? "[no reply]         " : mon_last[i][0] ? mon_last[i] : "[measuring]        ");
                                }
                            }
                            break;
                            
                        case MOD_DM5010:
                            {
                                char txt[32];
                                int r = dm5010_read(g_system->modules[i].gpib_address, i, &value);
                                dm5010_format(i, r, value, txt);
                                lp("%-18s", txt);
                                /* Over-range or no reply: nothing new, hold the last value */
                                if (r != DM5010_READ_OK) value = g_system->modules[i].last_reading;
                            }
                            break;
                            
                        case MOD_DM5120:
                            {
                                dm5120_config *cfg = &g_dm5120_config[i];
                                int address = g_system->modules[i].gpib_address;
                                float measurement_rate = dm5120_get_measurement_rate(i, cfg->trigger_mode);
                                int optimal_delay;
                                
                                if (cfg->buffer_enabled && cfg->buffer_size > 1) {
                                    /* Non-blocking fill; report the buffer average once full */
                                    int state = cfg->buffer_state;

                                    if (state == 0 && need_sample) {
                                        dm5120_start_buffer_async(address, i);
                                        lp("[Starting buffer]     ");
                                        value = g_system->modules[i].last_reading; /* Show last value */
                                        break;
                                    }
                                    
                                    if (state == 1 || state == 2) {
                                        dm5120_check_buffer_async(address, i);
                                        state = cfg->buffer_state;
                                    }

                                    switch(state) {
                                        case 1: /* Filling */
                                            lp("[Filling %3d samples] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 2: /* Half full */
                                            lp("[50%% Full - %3d smp] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            break;
                                            
                                        case 3: /* Full */
                                            value = dm5120_get_buffer_average(address);
                                            if (value == 0.0) {
                                                /* BUFAVE? gave nothing: read one stored reading */
                                                int count = dm5120_get_buffer_count(address);
                                                if (count > 0) {
                                                    value = dm5120_read_one_stored(address);
                                                }
                                            }
                                            lp("%12.6f %-3s[Avg%3d]", value, dm5120_unit(i), cfg->buffer_size);

                                            if (cfg->min_max_enabled && value != 0.0) {
                                                if (value < cfg->min_value) cfg->min_value = value;
                                                if (value > cfg->max_value) cfg->max_value = value;
                                                cfg->sample_count++;
                                            }

                                            dm5120_reset_buffer_async(i);
                                            break;

                                        case 4: /* Meter reported 551; the next pass restarts the fill */
                                            lp("[ERR 551 SHORT TIME] ");
                                            value = g_system->modules[i].last_reading;
                                            dm5120_reset_buffer_async(i);
                                            break;

                                        case 5: /* Timed out part-filled */
                                            lp("[Timeout at %3d smp] ", cfg->samples_ready);
                                            value = g_system->modules[i].last_reading;
                                            dm5120_reset_buffer_async(i);
                                            break;

                                        default:
                                            lp("[Buffer idle]         ");
                                            value = g_system->modules[i].last_reading;
                                            break;
                                    }
                                    
                                } else {
                                    value = read_dm5120_enhanced(address, i);
                                    if (value == 0.0) {
                                        value = read_dm5120_voltage(address);
                                    }
                                    
                                    lp("%12.6f %-3s(%4.1f r/s)", value, dm5120_unit(i), measurement_rate);
                                    
                                    /* [FAST]: sample period shorter than the meter can deliver */
                                    optimal_delay = dm5120_calculate_measurement_time(i, 0, 1);
                                    if (g_control_panel.sample_rate_ms < optimal_delay) {
                                        lp(" [FAST]");
                                    }

                                    if (cfg->min_max_enabled && value != 0.0) {
                                        if (value < cfg->min_value) cfg->min_value = value;
                                        if (value > cfg->max_value) cfg->max_value = value;
                                        cfg->sample_count++;
                                    }
                                }
                            }
                            break;
                            
                        case MOD_PS5004:
                            /* DISPLAY goes out only when it changes, plus once every 32
                               samples in case the front panel changed it (manual p. 3-6) */
                            ps_cfg = &g_ps5004_config[i];
                            if ((passes & 31) == 0) ps5004_forget_display(i);
                            {
                                float reading;
                                if (ps5004_measure(i, ps_cfg->display_mode, &reading)) {
                                    value = reading;
                                    if (ps_cfg->display_mode == 1) lp("%12.1f mA    ", value * 1000);
                                    else if (ps_cfg->display_mode == 2) lp("%12.1f mA lim", value * 1000);
                                    else lp("%12.4f V     ", value);
                                } else {
                                    value = g_system->modules[i].last_reading;
                                    lp("    No reply       ");
                                }
                            }
                            break;
                            
                        case MOD_PS5010:  
                            {
                                ps5010_config *ps_cfg = &g_ps5010_config[i];
                                int neg_stat, pos_stat, log_stat;
                                
                                if (ps5010_read_regulation(g_system->modules[i].gpib_address,
                                                          &neg_stat, &pos_stat, &log_stat)) {
                                    lp("P:%s N:%s L:%s     ",
                                           pos_stat == 1 ? "CV" : (pos_stat == 2 ? "CC" : "UR"),
                                           neg_stat == 1 ? "CV" : (neg_stat == 2 ? "CC" : "UR"),
                                           log_stat == 1 ? "CV" : (log_stat == 2 ? "CC" : "UR"));
                                    value = 0.0;
                                } else {
                                    lp("No status         ");
                                    value = 0.0;
                                }
                            }
                            break;

                        case MOD_FG5010:
                            /* A source: show its programmed state, store no sample. SET?
                               once a second (every 10 s while it doesn't answer), so the
                               generator doesn't set the pace */
                            {
                                static unsigned long __far fg5010_last[10];
                                static unsigned char __far fg5010_slow[10];
                                unsigned long now = *((unsigned long far *)0x0040006CL);
                                if (!mon_last[i][0] ||
                                    now - fg5010_last[i] >= (fg5010_slow[i] ? 182UL : 18UL)) {
                                    fg5010_slow[i] = !fg5010_state_text(g_system->modules[i].gpib_address,
                                                                        mon_last[i]);
                                    fg5010_last[i] = now;
                                }
                                lp("%s", mon_last[i]);
                                lp_nl();
                            }
                            continue;

                        default:
                            value = 0.0;
                            lp("Not implemented       ");
                    } 
                    
                    /* AUTO rate: time each instrument during calibration */
                    if (g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES)
                        mod_ticks[i] += *((unsigned long far *)0x0040006CL) - t_mod;
                    
                    if (!no_reading) {
                        g_system->modules[i].last_reading = value;
                        /* Stamp with the measurement clock (run time), then store once */
                        g_mclock = mclock_base + run_ticks + *((unsigned long far *)0x0040006CL) - run_mark;
                        g_store_suppress = 0;
                        g_mclock_valid = 1;
                        store_module_data(i, value);
                        g_mclock_valid = 0;
                        g_store_suppress = 1;

                        if (samples_taken == 0 && g_system->data_count < g_system->buffer_size) {
                            g_system->data_buffer[g_system->data_count] = value;
                        }

                        samples_taken++;
                    }
                    /* Remember this sample's text for the in-between redraws */
                    mon_buf[mon_len] = '\0';
                    strncpy(mon_last[i], mon_buf + mon_pfx, sizeof(mon_last[i]) - 1);
                    mon_last[i][sizeof(mon_last[i]) - 1] = '\0';
                } else {
                    /* Not sampling this pass: show the last reading */
                    if (g_control_panel.running) {
                        if (should_monitor) {
                            if (g_system->modules[i].module_type == MOD_DM5120 &&
                                g_dm5120_config[i].buffer_enabled && g_dm5120_config[i].buffer_state != 0) {
                                /* Keep polling an in-progress DM5120 buffer between samples */
                                dm5120_check_buffer_async(g_system->modules[i].gpib_address, i);
                                switch(g_dm5120_config[i].buffer_state) {
                                    case 1: lp("[Filling %3d]       ", g_dm5120_config[i].samples_ready); break;
                                    case 2: lp("[50%% Full]         "); break;
                                    case 3: lp("[Ready to read]    "); break;
                                    case 4: lp("[ERR 551 SHORT TIME]"); break;
                                    case 5: lp("[Fill timed out]   "); break;
                                    default: lp("%12.6f       ", g_system->modules[i].last_reading); break;
                                }
                            } else if (mon_last[i][0]) {
                                lp("%s", mon_last[i]);      /* same text as the last sample */
                            } else {
                                lp("%12.6f       ", g_system->modules[i].last_reading);
                            }
                        } else {
                            lp(" [Not Selected]    ");
                        }
                    } else {
                        lp(" [Press SPACE]        ");
                    }
                }
                
                lp_nl();
            } 
        } 
        
        /* Blank rows left over from a longer previous pass; a stale module
           line would otherwise look like a duplicated or overwritten slot */
        {
            int r;
            for (r = mon_row; r < mon_rows_used; r++) vputs(r, 1, "", 79);
            mon_rows_used = mon_row;
        }
        
        if (need_sample && g_control_panel.running && samples_taken > 0) {
            if (g_system->data_count < g_system->buffer_size) {
                g_system->data_count++;
            }
            
            tick_end = *((unsigned long far *)0x0040006CL);
            measurement_ticks = tick_end - tick_start;
            passes++;
            
            need_sample = 0;
            
            /* AUTO rate: after the calibration passes, lock the rate to the
               measured cycle, rounded up to whole 55 ms timer ticks */
            if (g_control_panel.auto_rate && auto_cal < AUTO_CAL_PASSES) {
                auto_sum += measurement_ticks;
                if (++auto_cal == AUTO_CAL_PASSES) {
                    int slow = -1;
                    ticks_per_sample = (auto_sum + AUTO_CAL_PASSES - 1) / AUTO_CAL_PASSES;
                    if (ticks_per_sample < 1) ticks_per_sample = 1;
                    g_control_panel.sample_rate_ms =
                        (int)((ticks_per_sample * 10000L + 181L) / 182L);
                    for (i = 0; i < 10; i++)
                        if (mod_ticks[i] && (slow < 0 || mod_ticks[i] > mod_ticks[slow]))
                            slow = i;
                    {
                        char msg[100];
                        if (slow >= 0)
                            sprintf(msg, "Sample rate: AUTO -> %d ms/sample (slowest: S%d, %lu ms per read)",
                                    g_control_panel.sample_rate_ms, slow,
                                    (mod_ticks[slow] * 10000L / AUTO_CAL_PASSES) / 182L);
                        else
                            sprintf(msg, "Sample rate: AUTO -> %d ms/sample",
                                    g_control_panel.sample_rate_ms);
                        vputs(2, 1, msg, 79);
                    }
                }
            }
        }
        
        if (kbhit()) {
            key = getch();
            switch(toupper(key)) {
                case 27:  /* ESC */
                    done = 1;
                    break;
                    
                case ' ':
                    if (g_control_panel.running)   /* stopping: bank the run time */
                        run_ticks += *((unsigned long far *)0x0040006CL) - run_mark;
                    else
                        run_mark = *((unsigned long far *)0x0040006CL);
                    g_control_panel.running = !g_control_panel.running;
                    if (g_control_panel.running) {
                        last_tick_count = *((unsigned long far *)0x0040006CL);
                        measurement_ticks = 0;
                        if (g_control_panel.auto_rate) {   /* re-measure */
                            auto_cal = 0;
                            auto_sum = 0;
                            for (i = 0; i < 10; i++) { mod_ticks[i] = 0; mon_last[i][0] = '\0'; }
                        }
                    }
                    break;
                    
                case 'C':
                    for (i = 0; i < 10; i++) {
                        clear_module_data(i);
                    }
                    g_system->data_count = 0;
                    passes = 0;
                    avg_shown = -1;
                    run_ticks = 0;                 /* measurement time restarts */
                    run_mark = *((unsigned long far *)0x0040006CL);
                    gotoxy(1, 22);
                    printf("*** All data cleared ***");
                    delay(500);
                    gotoxy(1, 22);
                    printf("                        ");
                    break;
            } 
        }
        
        if (g_control_panel.running && samples_taken > 0) {
            delay(5);  /* Shorter delay when actively sampling */
        } else {
            delay(15);  /* Longer delay when idle to save CPU */
        }
    } 
    
    g_store_suppress = 0;
    clrscr();
    printf("\n\nMonitoring complete.\n");
    printf("Readings per slot: %lu\n", passes);
    for (i = 0; i < 10; i++) {          /* per-slot summary from its own buffer */
        tm5000_module *m = &g_system->modules[i];
        if (!m->enabled || !m->module_data || m->module_data_count == 0) continue;
        printf("S%d: %u samples", i, m->module_data_count);
        if (m->avg_shift) printf(" (each the mean of %u readings)", 1u << m->avg_shift);
        printf(", first %.10g, last %.10g\n", m->module_data[0],
               m->module_data[m->module_data_count - 1]);
    }
    printf("\nPress any key to continue...");
    getch();
}
