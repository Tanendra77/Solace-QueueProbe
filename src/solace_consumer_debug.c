
#define APP_NAME "QueueProbe"
#define APP_VERSION "v1.0.1"
/* [RELEASE] v1.0.0 ->
    - pretty json format
    - proxy support
    - TCP/TCPS , certificate support
*/


/* ── Windows headers must come first ─────────────────────────────────────── */
#ifdef _WIN32
#  include <winsock2.h>
#  include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#ifndef _WIN32
#  include <unistd.h>
#  include <sys/time.h>
#else
   /* MinGW provides POSIX headers; if using MSVC replace with GetSystemTimeAsFileTime */
#  include <sys/time.h>
   static void sleep(unsigned int s) { Sleep(s * 1000); }
#endif

#include "solclient/solClient.h"
#include "solclient/solClientMsg.h"

/* ── ANSI colour codes ────────────────────────────────────────────────────── */
#define A_RESET       "\x1b[0m"
#define A_BOLD        "\x1b[1m"
#define A_DIM         "\x1b[2m"
#define A_RED         "\x1b[31m"
#define A_GREEN       "\x1b[32m"
#define A_YELLOW      "\x1b[33m"
#define A_BLUE        "\x1b[34m"
#define A_MAGENTA     "\x1b[35m"
#define A_CYAN        "\x1b[36m"
#define A_WHITE       "\x1b[37m"
#define A_BOLD_RED    "\x1b[1;31m"
#define A_BOLD_GREEN  "\x1b[1;32m"
#define A_BOLD_YELLOW "\x1b[1;33m"
#define A_BOLD_CYAN   "\x1b[1;36m"
#define A_BOLD_WHITE  "\x1b[1;37m"
#define A_BOLD_MAG    "\x1b[1;35m"
#define A_ORANGE      "\x1b[38;5;208m"

/* ── Constants ────────────────────────────────────────────────────────────── */
#define MAX_TOPICS        16
#define BROWSE_WINDOW     "255"   /* deliver up to 1M msgs before pause */
#define MAX_CONTENT_PRINT 800         /* chars to print for non-JSON payloads */

/* ── Mode enum ────────────────────────────────────────────────────────────── */
typedef enum { MODE_QUEUE = 0, MODE_BROWSE, MODE_TOPIC } AppMode;

/* ── Application context ──────────────────────────────────────────────────── */
typedef struct appContext {
    solClient_opaqueSession_pt  session_p;
    solClient_opaqueFlow_pt     flow_p;
    volatile int                running;
    volatile int                msgCount;
    FILE                       *logfile;
    AppMode                     mode;
} appContext_t;

static appContext_t appCtx;
static int          g_useColor = 1;   /* 0 = strip colors from terminal output too */

/* ── Config-file value storage (persists for program lifetime) ────────────── */
static char cfg_host    [512] = "";
static char cfg_vpn     [128] = "";
static char cfg_username[128] = "";
static char cfg_password[256] = "";
static char cfg_queue   [256] = "";
static char cfg_proxy   [512] = "";
static char cfg_certdir [512] = "";
static char cfg_logfile [512] = "";
static char cfg_topics  [MAX_TOPICS][256];
static int  cfg_topicCount   = 0;
static int  cfg_noVerify     = 0;
static int  cfg_browse       = 0;
static int  cfg_noColor      = 0;

/* =============================================================================
 *  Colour helpers
 * ============================================================================ */

static void enable_ansi(void) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   /* render UTF-8 em-dashes etc. correctly */
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD  mode = 0;
    if (GetConsoleMode(h, &mode))
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    else
        g_useColor = 0;  /* not a real console (e.g. redirected) — disable colors */
#else
    if (!isatty(STDOUT_FILENO))
        g_useColor = 0;
#endif
}

static const char *level_color(const char *lvl) {
    if (!strcmp(lvl,"SUCCESS"))       return A_BOLD_GREEN;
    if (!strcmp(lvl,"ERROR"))         return A_BOLD_RED;
    if (!strcmp(lvl,"SDK_ERROR"))     return A_RED;
    if (!strcmp(lvl,"WARNING"))       return A_BOLD_YELLOW;
    if (!strcmp(lvl,"SDK_WARN"))      return A_YELLOW;
    if (!strcmp(lvl,"MESSAGE"))       return A_BOLD_MAG;
    if (!strcmp(lvl,"FLOW_EVENT"))    return A_BLUE;
    if (!strcmp(lvl,"SESSION_EVENT")) return A_BLUE;
    if (!strcmp(lvl,"STATS"))         return A_BOLD_CYAN;
    if (!strcmp(lvl,"PROXY"))         return A_MAGENTA;
    if (!strcmp(lvl,"BROWSE"))        return A_CYAN;
    if (!strcmp(lvl,"TOPIC"))         return A_GREEN;
    if (!strncmp(lvl,"SDK_",4))       return A_DIM;
    if (!strcmp(lvl,"CLEANUP"))       return A_DIM;
    return A_CYAN;   /* INFO / CONFIG / SESSION / FLOW / INIT */
}

/* =============================================================================
 *  Logging
 * ============================================================================ */

/* Remove ANSI escape sequences (ESC [ ... m) from src into dst */
static void strip_ansi(const char *src, char *dst, size_t dstsz) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di + 1 < dstsz; si++) {
        if ((unsigned char)src[si] == 0x1b && src[si + 1] == '[') {
            si += 2;
            while (src[si] && src[si] != 'm') si++;
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static void get_timestamp(char *buf, size_t sz) {
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL);
    time_t now = (time_t)tv.tv_sec;
    tm_info = localtime(&now);
    strftime(buf, sz, "%H:%M:%S", tm_info);
    snprintf(buf + strlen(buf), sz - strlen(buf), ".%03ld", (long)(tv.tv_usec / 1000));
}

static void debug_log(const char *lvl, const char *fmt, ...) {
    char ts[24], raw[2048], clean[2048];
    va_list ap;
    get_timestamp(ts, sizeof(ts));
    va_start(ap, fmt);
    vsnprintf(raw, sizeof(raw), fmt, ap);
    va_end(ap);

    /* Terminal output */
    if (g_useColor) {
        const char *col = level_color(lvl);
        printf(A_DIM "[%s]" A_RESET " %s%-14s" A_RESET " %s\n", ts, col, lvl, raw);
    } else {
        strip_ansi(raw, clean, sizeof(clean));
        printf("[%s] %-14s %s\n", ts, lvl, clean);
    }

    /* Log file — always strip ANSI */
    if (appCtx.logfile) {
        strip_ansi(raw, clean, sizeof(clean));
        fprintf(appCtx.logfile, "[%s] [%-14s] %s\n", ts, lvl, clean);
        fflush(appCtx.logfile);
    }
}

/* =============================================================================
 *  JSON pretty-printer with syntax colouring
 *
 *  Keys        → cyan
 *  String vals → green
 *  Numbers /   → yellow
 *    booleans /
 *    null
 *  Braces/     → bold white
 *    brackets
 * ============================================================================ */

static void json_indent(int n) {
    for (int i = 0; i < n; i++) fputs("  ", stdout);
}

/* Returns 1 if the first non-whitespace char is '{' or '[' */
static int is_json(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        return c == '{' || c == '[';
    }
    return 0;
}

static void pretty_print_json(const char *json, size_t len) {
    char ctx[64]     = {0};   /* container stack: '{' or '[' */
    int  key_exp[64] = {0};   /* 1 when next string at this depth is a key */
    int  top    = 0;
    int  indent = 0;
    int  in_string   = 0;
    int  escape_next = 0;

    for (size_t i = 0; i < len; i++) {
        char c = json[i];

        if (escape_next) { printf("%c", c); escape_next = 0; continue; }

        if (in_string) {
            if      (c == '\\') { printf("\\"); escape_next = 1; }
            else if (c == '"')  { printf("\"" A_RESET); in_string = 0; }
            else                { printf("%c", c); }
            continue;
        }

        switch (c) {
            case '{':
                printf(A_BOLD_WHITE "{" A_RESET "\n");
                indent++;  json_indent(indent);
                if (top < 63) { ctx[++top] = '{'; key_exp[top] = 1; }
                break;

            case '[':
                printf(A_BOLD_WHITE "[" A_RESET "\n");
                indent++;  json_indent(indent);
                if (top < 63) { ctx[++top] = '['; key_exp[top] = 0; }
                break;

            case '}':
                printf("\n"); indent--;  json_indent(indent);
                printf(A_BOLD_WHITE "}" A_RESET);
                if (top > 0) top--;
                break;

            case ']':
                printf("\n"); indent--;  json_indent(indent);
                printf(A_BOLD_WHITE "]" A_RESET);
                if (top > 0) top--;
                break;

            case '"':
                in_string = 1;
                if (top > 0 && ctx[top] == '{' && key_exp[top])
                    printf(A_CYAN "\"");    /* key */
                else
                    printf(A_GREEN "\"");   /* string value */
                break;

            case ':':
                printf(A_WHITE ":" A_RESET " ");
                if (top > 0) key_exp[top] = 0;   /* value follows */
                break;

            case ',':
                printf(A_WHITE "," A_RESET "\n");
                json_indent(indent);
                if (top > 0 && ctx[top] == '{') key_exp[top] = 1;
                break;

            /* Skip whitespace outside strings */
            case ' ': case '\t': case '\n': case '\r': break;

            default:
                /* number / bool / null: read until a delimiter */
                printf(A_YELLOW "%c", c);
                while (i + 1 < len) {
                    char nx = json[i + 1];
                    if (nx == ',' || nx == '}' || nx == ']' ||
                        nx == ' ' || nx == '\t' || nx == '\n' || nx == '\r') break;
                    printf("%c", json[++i]);
                }
                printf(A_RESET);
                break;
        }
    }
    printf("\n");
}

/* =============================================================================
 *  Message rendering
 * ============================================================================ */

static void print_message_content(solClient_opaqueMsg_pt msg_p) {
    void              *ptr  = NULL;
    solClient_uint32_t size = 0;

    if (solClient_msg_getBinaryAttachmentPtr(msg_p,
            (solClient_opaquePointer_pt)&ptr, &size) != SOLCLIENT_OK
        || !ptr || size == 0) {
        debug_log("MESSAGE", "<no binary attachment>");
        return;
    }

    debug_log("MESSAGE", "Payload (%u bytes):", size);

    if (is_json((const char *)ptr, size)) {
        fputs("  ", stdout);                          /* indent JSON block */
        pretty_print_json((const char *)ptr, size);
        /* Write raw JSON to log file (no color codes) */
        if (appCtx.logfile) {
            fprintf(appCtx.logfile, "[payload] %.*s\n", (int)size, (char *)ptr);
            fflush(appCtx.logfile);
        }
    } else {
        int n = (int)size > MAX_CONTENT_PRINT ? MAX_CONTENT_PRINT : (int)size;
        printf(A_WHITE "  %.*s" A_RESET, n, (char *)ptr);
        if ((int)size > MAX_CONTENT_PRINT)
            printf(A_DIM " ...(truncated)" A_RESET);
        printf("\n");
        if (appCtx.logfile) {
            fprintf(appCtx.logfile, "[payload] %.*s", n, (char *)ptr);
            if ((int)size > MAX_CONTENT_PRINT)
                fprintf(appCtx.logfile, " ...(truncated)");
            fprintf(appCtx.logfile, "\n");
            fflush(appCtx.logfile);
        }
    }
}

/* Shared core processor — called from both flow and session callbacks */
static void process_message(solClient_opaqueMsg_pt msg_p, solClient_msgId_t *out_msgId) {
    solClient_msgId_t       msgId     = 0;
    solClient_destination_t dest;
    solClient_destination_t replyTo;
    const char             *corrId    = NULL;
    const char             *appMsgId  = NULL;
    const char             *senderId  = NULL;
    solClient_uint32_t      delivMode = 0;
    solClient_uint32_t      cos       = 0;
    int                     priority  = -1;
    solClient_int64_t       ttl       = 0;

    appCtx.msgCount++;

    /* ── Terminal separator ─────────────────────────────────────────── */
    printf("\n" A_BOLD_WHITE "+----- Message #%d -----+" A_RESET "\n", appCtx.msgCount);
    /* ── Log file separator (no ANSI) ───────────────────────────────── */
    if (appCtx.logfile) {
        fprintf(appCtx.logfile, "\n+----- Message #%d -----+\n", appCtx.msgCount);
        fflush(appCtx.logfile);
    }

    /* ── Solace Message ID ──────────────────────────────────────────── */
    if (solClient_msg_getMsgId(msg_p, &msgId) == SOLCLIENT_OK)
        debug_log("MESSAGE", "ID:             " A_YELLOW "%lld" A_RESET, (long long)msgId);
    if (out_msgId) *out_msgId = msgId;

    /* ── Application Message ID (JMSMessageID equivalent) ───────────── */
    if (solClient_msg_getApplicationMessageId(msg_p, &appMsgId) == SOLCLIENT_OK && appMsgId && appMsgId[0])
        debug_log("MESSAGE", "AppMsgId:       " A_CYAN "%s" A_RESET, appMsgId);

    /* ── Destination ────────────────────────────────────────────────── */
    if (solClient_msg_getDestination(msg_p, &dest, sizeof(dest)) == SOLCLIENT_OK)
        debug_log("MESSAGE", "Destination:    " A_CYAN "%s" A_RESET, dest.dest);

    /* ── ReplyTo ────────────────────────────────────────────────────── */
    memset(&replyTo, 0, sizeof(replyTo));
    if (solClient_msg_getReplyTo(msg_p, &replyTo, sizeof(replyTo)) == SOLCLIENT_OK
        && replyTo.dest && replyTo.dest[0])
        debug_log("MESSAGE", "ReplyTo:        %s", replyTo.dest);

    /* ── Correlation ID ─────────────────────────────────────────────── */
    if (solClient_msg_getCorrelationId(msg_p, &corrId) == SOLCLIENT_OK && corrId && corrId[0])
        debug_log("MESSAGE", "CorrelationId:  %s", corrId);

    /* ── Sender ID ──────────────────────────────────────────────────── */
    if (solClient_msg_getSenderId(msg_p, &senderId) == SOLCLIENT_OK && senderId && senderId[0])
        debug_log("MESSAGE", "SenderId:       %s", senderId);

    /* ── Delivery Mode ──────────────────────────────────────────────── */
    if (solClient_msg_getDeliveryMode(msg_p, &delivMode) == SOLCLIENT_OK) {
        const char *ms =
            delivMode == SOLCLIENT_DELIVERY_MODE_DIRECT
                ? A_GREEN  "DIRECT"         A_RESET :
            delivMode == SOLCLIENT_DELIVERY_MODE_PERSISTENT
                ? A_YELLOW "PERSISTENT"     A_RESET :
            delivMode == SOLCLIENT_DELIVERY_MODE_NONPERSISTENT
                ? A_CYAN   "NON-PERSISTENT" A_RESET : "UNKNOWN";
        debug_log("MESSAGE", "DeliveryMode:   %s", ms);
    }

    /* ── Class of Service ───────────────────────────────────────────── */
    if (solClient_msg_getClassOfService(msg_p, &cos) == SOLCLIENT_OK)
        debug_log("MESSAGE", "ClassOfService: COS_%u", cos);

    /* ── Priority ───────────────────────────────────────────────────── */
    if (solClient_msg_getPriority(msg_p, &priority) == SOLCLIENT_OK && priority >= 0)
        debug_log("MESSAGE", "Priority:       %d", priority);

    /* ── TTL ────────────────────────────────────────────────────────── */
    if (solClient_msg_getTimeToLive(msg_p, &ttl) == SOLCLIENT_OK && ttl > 0)
        debug_log("MESSAGE", "TTL:            %lld ms", (long long)ttl);

    /* ── DMQ Eligible ───────────────────────────────────────────────── */
    if (solClient_msg_isDMQEligible(msg_p))
        debug_log("MESSAGE", "DMQEligible:    " A_GREEN "yes" A_RESET);

    /* ── Payload ────────────────────────────────────────────────────── */
    print_message_content(msg_p);

    /* ── Closing separator ──────────────────────────────────────────── */
    printf(A_DIM "+----------------------+" A_RESET "\n");
    if (appCtx.logfile) {
        fprintf(appCtx.logfile, "+----------------------+\n");
        fflush(appCtx.logfile);
    }
}

/* =============================================================================
 *  Signal handler
 * ============================================================================ */

static void signalHandler(int sig) {
    debug_log("INFO", "Signal %d received — shutting down...", sig);
    appCtx.running = 0;
}

/* =============================================================================
 *  Solace callbacks
 * ============================================================================ */

/* Session events (connection lifecycle) */
static void sessionEventCallback(solClient_opaqueSession_pt            opaqueSession_p,
                                  solClient_session_eventCallbackInfo_pt ei,
                                  void                                  *user_p) {
    debug_log("SESSION_EVENT", "%s (%d)",
              solClient_session_eventToString(ei->sessionEvent), ei->sessionEvent);
    if (ei->info_p && ei->info_p[0])
        debug_log("SESSION_EVENT", "Info: %s", ei->info_p);

    switch (ei->sessionEvent) {
        case SOLCLIENT_SESSION_EVENT_UP_NOTICE:
            debug_log("SUCCESS", "Session established");
            break;
        case SOLCLIENT_SESSION_EVENT_CONNECT_FAILED_ERROR:
            debug_log("ERROR", "Connection failed (rc=%d): %s",
                      ei->responseCode, ei->info_p ? ei->info_p : "");
            break;
        case SOLCLIENT_SESSION_EVENT_DOWN_ERROR:
            debug_log("WARNING", "Session went down: %s", ei->info_p ? ei->info_p : "");
            break;
        case SOLCLIENT_SESSION_EVENT_RECONNECTING_NOTICE:
            debug_log("INFO", "Reconnecting...");
            break;
        case SOLCLIENT_SESSION_EVENT_RECONNECTED_NOTICE:
            debug_log("SUCCESS", "Session reconnected");
            break;
        default:
            break;
    }
}

/* Session-level message callback — active in TOPIC mode */
static solClient_rxMsgCallback_returnCode_t
sessionMsgCallback(solClient_opaqueSession_pt opaqueSession_p,
                   solClient_opaqueMsg_pt      msg_p,
                   void                       *user_p) {
    process_message(msg_p, NULL);   /* direct topic msgs: no ACK */
    return SOLCLIENT_CALLBACK_OK;
}

/* Flow message callback — active in QUEUE and BROWSE modes */
static solClient_rxMsgCallback_returnCode_t
flowMsgCallback(solClient_opaqueFlow_pt opaqueFlow_p,
                solClient_opaqueMsg_pt  msg_p,
                void                   *user_p) {
    appContext_t     *ctx   = (appContext_t *)user_p;
    solClient_msgId_t msgId = 0;

    process_message(msg_p, &msgId);

    if (ctx->mode == MODE_QUEUE) {
        if (msgId != 0) {
            if (solClient_flow_sendAck(opaqueFlow_p, msgId) != SOLCLIENT_OK)
                debug_log("ERROR", "ACK failed for msg %lld", (long long)msgId);
            else
                debug_log("SUCCESS", "Acknowledged msg %lld", (long long)msgId);
        } else {
            debug_log("WARNING", "No msgId — cannot ACK");
        }
    } else {
        /* BROWSE: deliberately no ACK — message stays on queue */
        debug_log("BROWSE", "Viewed (not consumed — message remains on queue)");
    }

    return SOLCLIENT_CALLBACK_OK;
}

/* Flow events (bind lifecycle) */
static void flowEventCallback(solClient_opaqueFlow_pt              opaqueFlow_p,
                               solClient_flow_eventCallbackInfo_pt  ei,
                               void                                *user_p) {
    debug_log("FLOW_EVENT", "%s (%d)",
              solClient_flow_eventToString(ei->flowEvent), ei->flowEvent);

    switch (ei->flowEvent) {
        case SOLCLIENT_FLOW_EVENT_UP_NOTICE:
            debug_log("SUCCESS", "Flow active and ready");
            break;
        case SOLCLIENT_FLOW_EVENT_DOWN_ERROR:
            debug_log("ERROR", "Flow down: %s (rc=%d)",
                      ei->info_p ? ei->info_p : "", ei->responseCode);
            break;
        case SOLCLIENT_FLOW_EVENT_BIND_FAILED_ERROR:
            debug_log("ERROR", "Bind failed: %s", ei->info_p ? ei->info_p : "");
            break;
        default:
            break;
    }
}

/* Combines broker URL and proxy config into Solace's host%proxy format */
static void buildProxyHostString(const char *host, const char *proxy,
                                  char *out, size_t outsz) {
    snprintf(out, outsz, "%s%%%s", host, proxy);
    debug_log("PROXY", "Host string: %s", out);
}

/* Route Solace SDK internal log messages through our logger */
static void solClientLogCallback(solClient_log_callbackInfo_pt li, void *user_p) {
    const char *lvl;
    switch (li->level) {
        case SOLCLIENT_LOG_ERROR:   lvl = "SDK_ERROR";  break;
        case SOLCLIENT_LOG_WARNING: lvl = "SDK_WARN";   break;
        case SOLCLIENT_LOG_NOTICE:  lvl = "SDK_NOTICE"; break;
        case SOLCLIENT_LOG_INFO:    lvl = "SDK_INFO";   break;
        default:                    lvl = "SDK_DEBUG";  break;
    }
    debug_log(lvl, "%s", li->msg_p);
}

/* =============================================================================
 *  Config file parser
 *  Format:  key = value   (# and ; start comments; blank lines ignored)
 *  Example: host = tcp://broker:55555
 * ============================================================================ */

static int is_true_val(const char *v) {
    return !strcmp(v,"1") || !strcmp(v,"true") || !strcmp(v,"yes");
}

static void parse_config_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: cannot open config file '%s'\n", path);
        return;
    }

    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';   /* strip newline */

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#' || *p == ';') continue;   /* comment / blank */

        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';

        /* Trim key */
        char *key = p;
        char *kend = eq - 1;
        while (kend >= key && (*kend == ' ' || *kend == '\t')) *kend-- = '\0';

        /* Trim value */
        char *val = eq + 1;
        while (*val == ' ' || *val == '\t') val++;
        char *vend = val + strlen(val) - 1;
        while (vend >= val && (*vend == ' ' || *vend == '\t')) *vend-- = '\0';

        /* Strip inline comment: whitespace followed by '#' (e.g. "value  # note") */
        for (char *ic = val; *ic; ic++) {
            if ((*ic == ' ' || *ic == '\t') && *(ic + 1) == '#') {
                *ic = '\0';
                break;
            }
        }
        /* Re-trim trailing whitespace after comment removal */
        vend = val + strlen(val) - 1;
        while (vend >= val && (*vend == ' ' || *vend == '\t')) *vend-- = '\0';

        if (!val[0]) continue;   /* skip empty values */

        if      (!strcmp(key, "host"))                    snprintf(cfg_host,     sizeof(cfg_host),     "%s", val);
        else if (!strcmp(key, "vpn"))                     snprintf(cfg_vpn,      sizeof(cfg_vpn),      "%s", val);
        else if (!strcmp(key, "username"))                snprintf(cfg_username, sizeof(cfg_username), "%s", val);
        else if (!strcmp(key, "password"))                snprintf(cfg_password, sizeof(cfg_password), "%s", val);
        else if (!strcmp(key, "queue"))                   snprintf(cfg_queue,    sizeof(cfg_queue),    "%s", val);
        else if (!strcmp(key, "proxy"))                   snprintf(cfg_proxy,    sizeof(cfg_proxy),    "%s", val);
        else if (!strcmp(key, "certdir"))                 snprintf(cfg_certdir,  sizeof(cfg_certdir),  "%s", val);
        else if (!strcmp(key, "logfile"))                 snprintf(cfg_logfile,  sizeof(cfg_logfile),  "%s", val);
        else if (!strcmp(key, "topic")) {
            if (cfg_topicCount < MAX_TOPICS)
                snprintf(cfg_topics[cfg_topicCount++], 256, "%s", val);
        }
        else if (!strcmp(key, "no-verify") || !strcmp(key, "no_verify")) { if (is_true_val(val)) cfg_noVerify = 1; }
        else if (!strcmp(key, "browse"))                                  { if (is_true_val(val)) cfg_browse   = 1; }
        else if (!strcmp(key, "no-color")  || !strcmp(key, "no_color"))  { if (is_true_val(val)) cfg_noColor  = 1; }
    }
    fclose(f);
}

/* =============================================================================
 *  Startup banner
 * ============================================================================ */

static void print_banner(void) {
    printf("\n");
    printf(A_ORANGE
        "  ___                     ____            _          \n"
        " / _ \\ _   _  ___ _   _  |  _ \\ _ __ ___| |__   ___ \n"
        "| | | | | | |/ _ \\ | | | | |_) | '__/ _ \\ '_ \\ / _ \\\n"
        "| |_| | |_| |  __/ |_| | |  __/| | | (_) | |_) |  __/\n"
        " \\__\\_\\\\__,_|\\___||__,_| |_|   |_|  \\___/|_.__/ \\___|"
        A_RESET "\n");
    printf(A_BOLD_GREEN
        "                             by Tanendra77@Github\n"
        A_RESET "\n");
}

/* =============================================================================
 *  Usage
 * ============================================================================ */

static void print_usage(const char *prog) {
    printf(A_BOLD_WHITE "\nUsage:\n" A_RESET);
    printf("  %s <queue>           [options]    " A_DIM "Queue consumer (default)\n" A_RESET, prog);
    printf("  %s <queue> --browse  [options]    " A_DIM "Browse without consuming\n" A_RESET, prog);
    printf("  %s --topic <t> [--topic <t>] ...  " A_DIM "Topic subscriber\n" A_RESET, prog);

    printf(A_BOLD_WHITE "\nConnection:\n" A_RESET);
    printf("  --host <url>       Broker URL        (default: tcp://localhost:55555)\n");
    printf("  --vpn  <name>      VPN name           (default: default)\n");
    printf("  --username <user>  Username           (default: default)\n");
    printf("  --password <pass>  Password\n");
    printf("  --proxy <url>      Proxy URL\n");

    printf(A_BOLD_WHITE "\nTLS / Certificate validation:\n" A_RESET);
    printf("  --certdir <path>   Trust-store directory  (enables cert validation)\n");
    printf("  --no-verify        Disable SSL cert check (for self-signed certs)\n");

    printf(A_BOLD_WHITE "\nOther:\n" A_RESET);
    printf("  --config  <file>   Load settings from config file\n");
    printf("                     (auto-loads queueprobe.conf if present)\n");
    printf("  --logfile <file>   Log file path     (default: solace_debug.log)\n");
    printf("  --no-color         Disable ANSI color in terminal output\n");

    printf(A_BOLD_WHITE "\nProxy examples:\n" A_RESET);
    printf("  socks5://172.31.0.1:1080\n");
    printf("  httpc://proxy.corp.com:3128\n");
    printf("  socks5://user:pass@proxy.corp.com:13128\n\n");
}

/* =============================================================================
 *  main
 * ============================================================================ */

int main(int argc, char *argv[]) {

        /* ---- VERSION CHECK ---- */
    if (argc > 1 &&
       (strcmp(argv[1], "--version") == 0 ||
        strcmp(argv[1], "-v") == 0)) {
        printf("%s %s\n", APP_NAME, APP_VERSION);
        return 0;
    }

    solClient_returnCode_t rc         = SOLCLIENT_OK;
    solClient_opaqueContext_pt ctx_p  = NULL;

    const char *sessionProps[60];
    const char *flowProps[20];
    int  propIndex;

    /* ── Hardcoded defaults ─────────────────────────────────────────── */
    const char *brokerHost  = "tcp://localhost:55555";
    const char *vpnName     = "default";
    const char *username    = "default";
    const char *password    = "";
    const char *queueName   = NULL;
    const char *proxyConfig = NULL;
    const char *certDir     = NULL;
    const char *logFile     = "solace_debug.log";
    int         noVerify    = 0;
    int         browseFlag  = 0;

    const char *topics[MAX_TOPICS];
    int         topicCount = 0;

    char hostWithProxy[512];

    enable_ansi();

    /* ── Config file detection (three ways, in priority order) ─────────
     *  1. argv[1] ends with .conf  →  .\QueueProbe.exe my.conf [opts]
     *  2. --config <path> flag     →  .\QueueProbe.exe --config my.conf
     *  3. auto-detect queueprobe.conf in current directory
     * ──────────────────────────────────────────────────────────────── */
    const char *configFile    = NULL;
    int         configFromPos = 0;   /* 1 = argv[1] was the config path */

    /* 1. Positional .conf argument */
    if (argc >= 2 && argv[1][0] != '-') {
        size_t l = strlen(argv[1]);
        if (l > 5 && !strcmp(argv[1] + l - 5, ".conf")) {
            configFile    = argv[1];
            configFromPos = 1;
        }
    }
    /* 2. --config flag */
    if (!configFile) {
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "--config") && i + 1 < argc) {
                configFile = argv[i + 1];
                break;
            }
        }
    }
    /* 3. Auto-detect */
    if (!configFile) {
        FILE *probe = fopen("queueprobe.conf", "r");
        if (probe) { fclose(probe); configFile = "queueprobe.conf"; }
    }
    if (configFile) parse_config_file(configFile);

    /* Apply config values — CLI args parsed below will override these */
    if (cfg_host[0])     brokerHost  = cfg_host;
    if (cfg_vpn[0])      vpnName     = cfg_vpn;
    if (cfg_username[0]) username    = cfg_username;
    if (cfg_password[0]) password    = cfg_password;
    if (cfg_queue[0])    queueName   = cfg_queue;
    if (cfg_proxy[0])    proxyConfig = cfg_proxy;
    if (cfg_certdir[0])  certDir     = cfg_certdir;
    if (cfg_logfile[0])  logFile     = cfg_logfile;
    if (cfg_noVerify)    noVerify    = 1;
    if (cfg_browse)      browseFlag  = 1;
    if (cfg_noColor)     g_useColor  = 0;
    for (int t = 0; t < cfg_topicCount && topicCount < MAX_TOPICS; t++)
        topics[topicCount++] = cfg_topics[t];

    /* ── Argument parsing (overrides config file) ───────────────────── */
    int argStart = 1;
    if (argc >= 2 && argv[1][0] != '-') {
        if (configFromPos)
            argStart = 2;          /* argv[1] was the .conf path — skip it */
        else {
            queueName = argv[1];   /* argv[1] is the queue name */
            argStart  = 2;
        }
    }

    for (int i = argStart; i < argc; i++) {
        if      (!strcmp(argv[i],"--host")     && i+1<argc) brokerHost  = argv[++i];
        else if (!strcmp(argv[i],"--vpn")      && i+1<argc) vpnName     = argv[++i];
        else if (!strcmp(argv[i],"--username") && i+1<argc) username    = argv[++i];
        else if (!strcmp(argv[i],"--password") && i+1<argc) password    = argv[++i];
        else if (!strcmp(argv[i],"--proxy")    && i+1<argc) proxyConfig = argv[++i];
        else if (!strcmp(argv[i],"--certdir")  && i+1<argc) certDir     = argv[++i];
        else if (!strcmp(argv[i],"--logfile")  && i+1<argc) logFile     = argv[++i];
        else if (!strcmp(argv[i],"--config")   && i+1<argc) { i++; /* already handled */ }
        else if (!strcmp(argv[i],"--no-verify"))             noVerify    = 1;
        else if (!strcmp(argv[i],"--no-color"))              g_useColor  = 0;
        else if (!strcmp(argv[i],"--browse"))                browseFlag  = 1;
        else if (!strcmp(argv[i],"--topic")    && i+1<argc) {
            if (topicCount < MAX_TOPICS) topics[topicCount++] = argv[++i];
        }
    }

    /* ── Mode determination ────────────────────────────────────────── */
    AppMode mode;
    if      (topicCount > 0) mode = MODE_TOPIC;
    else if (browseFlag)     mode = MODE_BROWSE;
    else                     mode = MODE_QUEUE;

    /* ── Validate required arguments ───────────────────────────────── */
    if (mode != MODE_TOPIC && !queueName) { print_usage(argv[0]); return 1; }
    if (mode == MODE_TOPIC && topicCount == 0) { print_usage(argv[0]); return 1; }

    /* ── Initialise app context ────────────────────────────────────── */
    memset(&appCtx, 0, sizeof(appCtx));
    appCtx.running = 1;
    appCtx.mode    = mode;

    appCtx.logfile = fopen(logFile, "a");
    if (!appCtx.logfile)
        printf(A_YELLOW "Warning: cannot open log file '%s'\n" A_RESET, logFile);

    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    /* ── Banner ────────────────────────────────────────────────────── */
    print_banner();

    if (configFile)
        debug_log("CONFIG", "Config file:  " A_DIM "%s" A_RESET, configFile);

    const char *modeLabel =
        mode == MODE_BROWSE ? A_CYAN   "BROWSE"           A_RESET :
        mode == MODE_TOPIC  ? A_GREEN  "TOPIC SUBSCRIBER" A_RESET :
                              A_YELLOW "QUEUE CONSUMER"   A_RESET;

    debug_log("CONFIG", "Mode:     %s", modeLabel);
    debug_log("CONFIG", "Broker:   %s", brokerHost);
    debug_log("CONFIG", "VPN:      %s", vpnName);
    debug_log("CONFIG", "Username: %s", username);
    if (queueName)  debug_log("CONFIG", "Queue:    %s", queueName);
    for (int t = 0; t < topicCount; t++)
        debug_log("CONFIG", "Topic[%d]: " A_GREEN "%s" A_RESET, t, topics[t]);
    if (proxyConfig) debug_log("PROXY",   "Proxy:    %s", proxyConfig);
    if (certDir)     debug_log("CONFIG",  "CertDir:  %s", certDir);
    if (noVerify)    debug_log("WARNING", "SSL certificate validation " A_BOLD_RED "DISABLED" A_RESET);
    printf("\n");

    /* ── Initialise Solace API ─────────────────────────────────────── */
    debug_log("INIT", "Initialising Solace C API...");
    rc = solClient_initialize(SOLCLIENT_LOG_DEFAULT_FILTER, NULL);
    if (rc != SOLCLIENT_OK) {
        debug_log("ERROR", "solClient_initialize() failed (rc=%d)", rc);
        return 1;
    }
    /* Configure SDK logging after init */
    solClient_log_setCallback(solClientLogCallback, NULL);
    solClient_log_setFilterLevel(SOLCLIENT_LOG_CATEGORY_ALL, SOLCLIENT_LOG_NOTICE);
    debug_log("SUCCESS", "API initialised");

    /* ── Create context ────────────────────────────────────────────── */
    solClient_context_createFuncInfo_t ctxFuncInfo = SOLCLIENT_CONTEXT_CREATEFUNC_INITIALIZER;
    rc = solClient_context_create(SOLCLIENT_CONTEXT_PROPS_DEFAULT_WITH_CREATE_THREAD,
                                  &ctx_p, &ctxFuncInfo, sizeof(ctxFuncInfo));
    if (rc != SOLCLIENT_OK) {
        debug_log("ERROR", "solClient_context_create() failed (rc=%d)", rc);
        goto cleanup;
    }
    debug_log("SUCCESS", "Context created");

    /* ── Session properties ────────────────────────────────────────── */
    propIndex = 0;

    if (proxyConfig) {
        buildProxyHostString(brokerHost, proxyConfig, hostWithProxy, sizeof(hostWithProxy));
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = hostWithProxy;
    } else {
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_HOST;
        sessionProps[propIndex++] = brokerHost;
    }

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_VPN_NAME;
    sessionProps[propIndex++] = vpnName;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_USERNAME;
    sessionProps[propIndex++] = username;
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_PASSWORD;
    sessionProps[propIndex++] = password;

    /* TLS/SSL certificate validation */
    if (certDir) {
        /* Validate against provided CA trust store */
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_TRUST_STORE_DIR;
        sessionProps[propIndex++] = certDir;
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
        sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
        debug_log("CONFIG", "TLS cert validation " A_BOLD_GREEN "ENABLED" A_RESET
                  " (trust store: %s)", certDir);
    } else {
        /* Disabled by default (matches SDK examples; use --certdir for production) */
        sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_SSL_VALIDATE_CERTIFICATE;
        sessionProps[propIndex++] = SOLCLIENT_PROP_DISABLE_VAL;
    }

    /* Re-apply subscriptions on reconnect — critical for topic mode */
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_REAPPLY_SUBSCRIPTIONS;
    sessionProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;

    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_CONNECT_TIMEOUT_MS;
    sessionProps[propIndex++] = "30000";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRIES;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_RECONNECT_RETRY_WAIT_MS;
    sessionProps[propIndex++] = "3000";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_KEEP_ALIVE_INT_MS;
    sessionProps[propIndex++] = "3000";
    sessionProps[propIndex++] = SOLCLIENT_SESSION_PROP_KEEP_ALIVE_LIMIT;
    sessionProps[propIndex++] = "3";
    sessionProps[propIndex]   = NULL;

    /* ── Create + connect session ──────────────────────────────────── */
    solClient_session_createFuncInfo_t sessFuncInfo = SOLCLIENT_SESSION_CREATEFUNC_INITIALIZER;
    sessFuncInfo.eventInfo.callback_p = sessionEventCallback;
    sessFuncInfo.eventInfo.user_p     = &appCtx;

    /* The Solace API requires a non-null session rx callback even in queue/browse
     * mode where messages are actually delivered via the flow callback.
     * sessionMsgCallback handles direct/topic messages; it is never invoked
     * for queued messages (those go to flowMsgCallback instead). */
    sessFuncInfo.rxMsgInfo.callback_p = sessionMsgCallback;
    sessFuncInfo.rxMsgInfo.user_p     = &appCtx;

    rc = solClient_session_create(sessionProps, ctx_p,
                                  &appCtx.session_p, &sessFuncInfo, sizeof(sessFuncInfo));
    if (rc != SOLCLIENT_OK) {
        debug_log("ERROR", "solClient_session_create() failed (rc=%d)", rc);
        goto cleanup;
    }

    debug_log("SESSION", "Connecting to %s...", brokerHost);
    rc = solClient_session_connect(appCtx.session_p);
    if (rc != SOLCLIENT_OK) {
        debug_log("ERROR", "Connect failed: %s", solClient_returnCodeToString(rc));
        goto cleanup;
    }
    debug_log("SUCCESS", "Connected!\n");

    /* ── Mode-specific setup ───────────────────────────────────────── */

    if (mode == MODE_QUEUE || mode == MODE_BROWSE) {

        /* Check browse capability before trying */
        if (mode == MODE_BROWSE &&
            !solClient_session_isCapable(appCtx.session_p,
                                         SOLCLIENT_SESSION_CAPABILITY_BROWSER)) {
            debug_log("ERROR", "Broker does not support browse mode");
            goto cleanup;
        }

        /* Build flow properties */
        propIndex = 0;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_BLOCKING;
        flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_ID;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_ENTITY_QUEUE;
        flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BIND_NAME;
        flowProps[propIndex++] = queueName;

        if (mode == MODE_BROWSE) {
            /* Browser flow: read without consuming */
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_BROWSER;
            flowProps[propIndex++] = SOLCLIENT_PROP_ENABLE_VAL;
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_WINDOWSIZE;
            flowProps[propIndex++] = BROWSE_WINDOW;
        } else {
            /* Consumer flow: explicit client ack */
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE;
            flowProps[propIndex++] = SOLCLIENT_FLOW_PROP_ACKMODE_CLIENT;
        }
        flowProps[propIndex] = NULL;

        solClient_flow_createFuncInfo_t flowFuncInfo = SOLCLIENT_FLOW_CREATEFUNC_INITIALIZER;
        flowFuncInfo.rxMsgInfo.callback_p = flowMsgCallback;
        flowFuncInfo.rxMsgInfo.user_p     = &appCtx;
        flowFuncInfo.eventInfo.callback_p = flowEventCallback;
        flowFuncInfo.eventInfo.user_p     = &appCtx;

        debug_log(mode==MODE_BROWSE ? "BROWSE" : "FLOW",
                  "%s queue '%s'...",
                  mode==MODE_BROWSE ? "Browsing" : "Binding to",
                  queueName);

        rc = solClient_session_createFlow(flowProps, appCtx.session_p,
                                          &appCtx.flow_p, &flowFuncInfo,
                                          sizeof(flowFuncInfo));
        if (rc != SOLCLIENT_OK) {
            debug_log("ERROR",
                      "solClient_session_createFlow() failed (rc=%d) — "
                      "does queue '%s' exist and have Consume permission?",
                      rc, queueName);
            goto cleanup;
        }

        if (mode == MODE_BROWSE)
            debug_log("BROWSE",
                      A_BOLD_CYAN "Browse active" A_RESET
                      " — messages will NOT be consumed from the queue");
        else
            debug_log("SUCCESS",
                      A_BOLD_GREEN "Bound to queue" A_RESET " — consuming messages");

    } else { /* MODE_TOPIC */

        for (int t = 0; t < topicCount; t++) {
            rc = solClient_session_topicSubscribeExt(
                    appCtx.session_p,
                    SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                    topics[t]);
            if (rc != SOLCLIENT_OK) {
                debug_log("ERROR", "Subscribe failed for '%s' (rc=%d)", topics[t], rc);
                goto cleanup;
            }
            debug_log("TOPIC", "Subscribed: " A_GREEN "%s" A_RESET, topics[t]);
        }
    }

    debug_log("INFO", A_BOLD "Waiting for messages — Ctrl+C to exit" A_RESET "\n");

    /* ── Main loop ─────────────────────────────────────────────────── */
    while (appCtx.running) {
        sleep(1);
    }

    /* ── Exit stats ────────────────────────────────────────────────── */
    printf("\n" A_BOLD_CYAN
           "+------------------------------+\n"
           "|  Session statistics          |\n"
           "+------------------------------+\n" A_RESET);
    debug_log("STATS", "Total messages received: " A_BOLD_WHITE "%d" A_RESET,
              appCtx.msgCount);

    /* Clean unsubscribe for topic mode */
    if (mode == MODE_TOPIC) {
        for (int t = 0; t < topicCount; t++) {
            solClient_session_topicUnsubscribeExt(
                appCtx.session_p,
                SOLCLIENT_SUBSCRIBE_FLAGS_WAITFORCONFIRM,
                topics[t]);
            debug_log("TOPIC", "Unsubscribed: %s", topics[t]);
        }
    }

cleanup:
    debug_log("CLEANUP", "Releasing resources...");

    if (appCtx.flow_p) {
        solClient_flow_destroy(&appCtx.flow_p);
        debug_log("CLEANUP", "Flow destroyed");
    }
    if (appCtx.session_p) {
        solClient_session_disconnect(appCtx.session_p);
        solClient_session_destroy(&appCtx.session_p);
        debug_log("CLEANUP", "Session destroyed");
    }
    if (ctx_p) {
        solClient_context_destroy(&ctx_p);
        debug_log("CLEANUP", "Context destroyed");
    }
    solClient_cleanup();
    debug_log("CLEANUP", A_BOLD_GREEN "Done" A_RESET);

    if (appCtx.logfile) fclose(appCtx.logfile);

    return (rc == SOLCLIENT_OK) ? 0 : 1;
}
