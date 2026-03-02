#include "output/out_web.h"
#include "output/out_html.h"
#include "output/layout.h"
#include "core/scan.h"
#include "core/graph.h"
#include "config.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ---------- portability ------------------------------------------------- */

#ifndef RI_DARWIN
/* Linux may lack strcasestr when compiled with strict C99. */
static char *ri_strcasestr(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (strncasecmp(haystack, needle, nlen) == 0)
            return (char *)haystack;
    }
    return NULL;
}
#define strcasestr ri_strcasestr
#endif

/* ---------- shutdown flag ----------------------------------------------- */

static volatile sig_atomic_t g_shutdown;

static void sigint_handler(int sig)
{
    (void)sig;
    g_shutdown = 1;
}

/* ---------- helpers ----------------------------------------------------- */

static void url_decode(char *dst, const char *src, size_t dst_sz)
{
    char *end = dst + dst_sz - 1;
    while (*src && dst < end) {
        if (*src == '%' && src[1] && src[2]) {
            unsigned int ch;
            if (sscanf(src + 1, "%2x", &ch) == 1) {
                *dst++ = (char)ch;
                src += 3;
                continue;
            }
        }
        if (*src == '+') { *dst++ = ' '; src++; continue; }
        *dst++ = *src++;
    }
    *dst = '\0';
}

/* Find value for key in url-encoded body, decode into buf. Returns 1 if found. */
static int form_get_param(const char *body, const char *key,
                          char *buf, size_t buf_sz)
{
    size_t klen = strlen(key);
    const char *p = body;
    while ((p = strstr(p, key)) != NULL) {
        /* Verify it's at start of body or preceded by '&' */
        if (p != body && *(p - 1) != '&') { p += klen; continue; }
        if (p[klen] != '=') { p += klen; continue; }
        const char *val = p + klen + 1;
        const char *amp = strchr(val, '&');
        size_t vlen = amp ? (size_t)(amp - val) : strlen(val);
        /* Temp copy for decode */
        char raw[1024];
        if (vlen >= sizeof(raw)) vlen = sizeof(raw) - 1;
        memcpy(raw, val, vlen);
        raw[vlen] = '\0';
        url_decode(buf, raw, buf_sz);
        return 1;
    }
    buf[0] = '\0';
    return 0;
}

static int form_get_checkbox(const char *body, const char *key)
{
    char tmp[8];
    return form_get_param(body, key, tmp, sizeof(tmp));
}

/* ---------- HTTP helpers ------------------------------------------------ */

static void send_response(int fd, const char *status, const char *ctype,
                          const char *body, size_t body_len)
{
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "\r\n", status, ctype, body_len);
    write(fd, hdr, (size_t)hlen);
    if (body_len > 0)
        write(fd, body, body_len);
}

static void send_error(int fd, const char *status, const char *msg)
{
    send_response(fd, status, "text/plain", msg, strlen(msg));
}

/* ---------- chunked transfer helpers ----------------------------------- */

static void send_chunk(int fd, const char *data, size_t len)
{
    if (len == 0) return;
    char hdr[32];
    int hlen = snprintf(hdr, sizeof(hdr), "%zx\r\n", len);
    write(fd, hdr, (size_t)hlen);
    write(fd, data, len);
    write(fd, "\r\n", 2);
}

static void send_chunk_str(int fd, const char *str)
{
    send_chunk(fd, str, strlen(str));
}

static void send_chunked_end(int fd)
{
    write(fd, "0\r\n\r\n", 5);
}

/* ---------- base64 encoder --------------------------------------------- */

static char *base64_encode(const unsigned char *data, size_t len)
{
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t out_len = 4 * ((len + 2) / 3);
    char *out = malloc(out_len + 1);
    if (!out) return NULL;

    size_t i, j;
    for (i = 0, j = 0; i + 2 < len; i += 3, j += 4) {
        unsigned int v = ((unsigned int)data[i] << 16) |
                         ((unsigned int)data[i+1] << 8) |
                          (unsigned int)data[i+2];
        out[j]   = tbl[(v >> 18) & 0x3F];
        out[j+1] = tbl[(v >> 12) & 0x3F];
        out[j+2] = tbl[(v >>  6) & 0x3F];
        out[j+3] = tbl[ v        & 0x3F];
    }
    if (i < len) {
        unsigned int v = (unsigned int)data[i] << 16;
        if (i + 1 < len) v |= (unsigned int)data[i+1] << 8;
        out[j]   = tbl[(v >> 18) & 0x3F];
        out[j+1] = tbl[(v >> 12) & 0x3F];
        out[j+2] = (i + 1 < len) ? tbl[(v >> 6) & 0x3F] : '=';
        out[j+3] = '=';
        j += 4;
    }
    out[j] = '\0';
    return out;
}

/* ---------- form page --------------------------------------------------- */

static const char FORM_PAGE[] =
"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>visual-traceroute</title>"
"<style>"
"body{margin:0;background:#0a0a14;color:#e0e0e0;font-family:system-ui,sans-serif;"
"display:flex;justify-content:center;align-items:center;min-height:100vh}"
".card{background:#16162a;border:1px solid #2a2a4a;border-radius:12px;"
"padding:32px 36px;width:380px;box-shadow:0 8px 32px rgba(0,0,0,0.5)}"
"h1{margin:0 0 6px;font-size:22px;color:#fff}"
".sub{font-size:13px;color:#888;margin-bottom:24px}"
"label{display:block;font-size:13px;color:#aaa;margin:14px 0 4px}"
"input[type=text],input[type=number]{width:100%;box-sizing:border-box;"
"padding:8px 10px;background:#0d0d1a;border:1px solid #333;border-radius:6px;"
"color:#fff;font-size:14px}"
"input:focus{outline:none;border-color:#5577ff}"
".row{display:flex;gap:12px}"
".row>div{flex:1}"
".checks{margin:16px 0;display:flex;flex-wrap:wrap;gap:10px 18px}"
".checks label{display:flex;align-items:center;gap:5px;margin:0;cursor:pointer}"
".checks input{accent-color:#5577ff}"
"button{width:100%;padding:10px;margin-top:20px;background:#5577ff;"
"color:#fff;border:none;border-radius:6px;font-size:15px;cursor:pointer;"
"font-weight:600;letter-spacing:0.3px}"
"button:hover{background:#6688ff}"
".foot{text-align:center;margin-top:14px;font-size:11px;color:#555}"
"</style></head><body>"
"<form class=\"card\" action=\"/scan\" method=\"POST\" target=\"_blank\">"
"<h1>visual-traceroute</h1>"
"<div class=\"sub\">Network Discovery &amp; 3D Visualization</div>"
"<label for=\"target\">Target host (blank for local-only scan)</label>"
"<input type=\"text\" id=\"target\" name=\"target\" placeholder=\"e.g. 8.8.8.8 or example.com\">"
"<div class=\"row\">"
"<div><label for=\"depth\">Max depth</label>"
"<input type=\"number\" id=\"depth\" name=\"depth\" value=\"1\" min=\"0\" max=\"64\"></div>"
"<div><label>IP version</label>"
"<select name=\"ipver\" style=\"width:100%;padding:8px 10px;background:#0d0d1a;"
"border:1px solid #333;border-radius:6px;color:#fff;font-size:14px\">"
"<option value=\"any\">Any</option><option value=\"4\">IPv4 only</option>"
"<option value=\"6\">IPv6 only</option></select></div></div>"
"<div class=\"checks\">"
"<label><input type=\"checkbox\" name=\"no_mdns\"> No mDNS</label>"
"<label><input type=\"checkbox\" name=\"no_arp\"> No ARP</label>"
"<label><input type=\"checkbox\" name=\"subnet_scan\"> Subnet scan</label>"
"<label><input type=\"checkbox\" name=\"hop_scan\"> Hop scan</label>"
"</div>"
"<button type=\"submit\">Scan &amp; Visualize</button>"
"<div class=\"foot\">Result opens in a new tab. Ctrl-C in terminal to stop.</div>"
"</form></body></html>";

/* ---------- progress page (streamed as first chunk) -------------------- */

static const char PROGRESS_PAGE[] =
"<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>Scanning... - visual-traceroute</title>"
"<style>"
"body{margin:0;background:#0a0a14;color:#e0e0e0;font-family:system-ui,sans-serif;"
"display:flex;justify-content:center;align-items:center;min-height:100vh}"
".card{background:#16162a;border:1px solid #2a2a4a;border-radius:12px;"
"padding:32px 36px;width:420px;box-shadow:0 8px 32px rgba(0,0,0,0.5);"
"text-align:center}"
"h1{margin:0 0 6px;font-size:22px;color:#fff}"
".sub{font-size:13px;color:#888;margin-bottom:20px}"
"@keyframes spin{to{transform:rotate(360deg)}}"
".spinner{width:40px;height:40px;margin:0 auto 16px;border:3px solid #2a2a4a;"
"border-top-color:#5577ff;border-radius:50%;animation:spin 0.8s linear infinite}"
"#status{font-size:14px;color:#aac;margin-bottom:12px;min-height:20px}"
"#log{text-align:left;background:#0d0d1a;border:1px solid #2a2a4a;"
"border-radius:8px;padding:10px 12px;max-height:240px;overflow-y:auto;"
"font-size:12px;font-family:'SF Mono',Menlo,Consolas,monospace;"
"line-height:1.6;color:#8a8aaa}"
"#log .ts{color:#555;margin-right:6px}"
"</style></head><body>"
"<div class=\"card\">"
"<h1>Scanning...</h1>"
"<div class=\"sub\">Network Discovery &amp; 3D Visualization</div>"
"<div class=\"spinner\"></div>"
"<div id=\"status\">Initializing...</div>"
"<div id=\"log\"></div>"
"</div>"
"<script>"
"var t0=Date.now();"
"function ts(){var s=((Date.now()-t0)/1000).toFixed(1);return '<span class=\"ts\">'+s+'s</span>';}"
"function updateProgress(msg){"
"document.getElementById('status').textContent=msg;"
"var log=document.getElementById('log');"
"log.innerHTML+=ts()+msg+'<br>';"
"log.scrollTop=log.scrollHeight;}"
"function finishWithHtml(b64){"
"var html=atob(b64);"
"document.open();document.write(html);document.close();}"
"</script>\n";

/* ---------- progress callback for chunked streaming -------------------- */

static void web_progress_cb(const char *msg, void *ctx)
{
    int fd = *(int *)ctx;
    /* Escape for JS string: backslash, quotes, angle brackets */
    char escaped[1024];
    size_t j = 0;
    for (size_t i = 0; msg[i] && j < sizeof(escaped) - 6; i++) {
        switch (msg[i]) {
        case '\\': escaped[j++] = '\\'; escaped[j++] = '\\'; break;
        case '\'': escaped[j++] = '\\'; escaped[j++] = '\''; break;
        case '"':  escaped[j++] = '\\'; escaped[j++] = '"';  break;
        case '<':  escaped[j++] = '\\'; escaped[j++] = 'x'; escaped[j++] = '3'; escaped[j++] = 'c'; break;
        case '>':  escaped[j++] = '\\'; escaped[j++] = 'x'; escaped[j++] = '3'; escaped[j++] = 'e'; break;
        case '\n': escaped[j++] = ' '; break;
        default:   escaped[j++] = msg[i]; break;
        }
    }
    escaped[j] = '\0';

    char chunk[1200];
    int len = snprintf(chunk, sizeof(chunk),
        "<script>updateProgress(\"%s\")</script>\n", escaped);
    if (len > 0)
        send_chunk(fd, chunk, (size_t)len);
}

/* ---------- request handler --------------------------------------------- */

static void handle_request(int fd, const ri_config_t *defaults)
{
    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return; }
    buf[n] = '\0';

    /* Parse method and path */
    char method[8] = {0}, path[256] = {0};
    sscanf(buf, "%7s %255s", method, path);

    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        send_response(fd, "200 OK", "text/html",
                      FORM_PAGE, sizeof(FORM_PAGE) - 1);
        close(fd);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/scan") == 0) {
        /* Find body after \r\n\r\n */
        const char *body = strstr(buf, "\r\n\r\n");
        if (!body) { send_error(fd, "400 Bad Request", "Malformed"); close(fd); return; }
        body += 4;

        /* Build config from form */
        ri_config_t cfg = *defaults;
        cfg.web_mode = 0; /* don't recurse */

        char tmp[256];
        if (form_get_param(body, "target", tmp, sizeof(tmp)) && tmp[0]) {
            strncpy(cfg.target, tmp, sizeof(cfg.target) - 1);
            cfg.has_target = 1;
        }
        if (form_get_param(body, "depth", tmp, sizeof(tmp)))
            cfg.max_depth = atoi(tmp);
        if (cfg.max_depth < 0) cfg.max_depth = 0;

        if (form_get_param(body, "ipver", tmp, sizeof(tmp))) {
            if (strcmp(tmp, "4") == 0)      { cfg.ipv4_only = 1; cfg.ipv6_only = 0; }
            else if (strcmp(tmp, "6") == 0)  { cfg.ipv6_only = 1; cfg.ipv4_only = 0; }
        }

        cfg.no_mdns     = form_get_checkbox(body, "no_mdns");
        cfg.no_arp       = form_get_checkbox(body, "no_arp");
        cfg.subnet_scan  = form_get_checkbox(body, "subnet_scan");
        cfg.hop_scan     = form_get_checkbox(body, "hop_scan");

        LOG_INFO("Web scan: target=%s depth=%d",
                 cfg.has_target ? cfg.target : "(local)", cfg.max_depth);

        /* --- Begin chunked streaming response --- */
        const char *chunked_hdr =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "\r\n";
        write(fd, chunked_hdr, strlen(chunked_hdr));

        /* Send the progress page as the first chunk */
        send_chunk(fd, PROGRESS_PAGE, sizeof(PROGRESS_PAGE) - 1);

        /* Set up progress callback */
        ri_scan_set_progress(web_progress_cb, &fd);

        /* Run scan pipeline with streaming progress */
        ri_graph_t *g = ri_scan_run(&cfg);
        if (!g) {
            send_chunk_str(fd,
                "<script>updateProgress(\"Scan failed!\")</script>\n");
            send_chunked_end(fd);
            ri_scan_set_progress(NULL, NULL);
            close(fd);
            return;
        }

        web_progress_cb("Computing MST...", &fd);
        ri_graph_kruskal_mst(g);

        web_progress_cb("Computing 3D layout...", &fd);
        ri_layout_3d(g);
        ri_layout_force_refine(g, 50);

        web_progress_cb("Generating visualization...", &fd);
        char *html = ri_out_html_string(g);
        ri_graph_destroy(g);

        if (!html) {
            send_chunk_str(fd,
                "<script>updateProgress(\"HTML generation failed!\")</script>\n");
            send_chunked_end(fd);
            ri_scan_set_progress(NULL, NULL);
            close(fd);
            return;
        }

        /* Base64-encode the visualization HTML and send as final script */
        char *b64 = base64_encode((const unsigned char *)html, strlen(html));
        free(html);

        if (!b64) {
            send_chunk_str(fd,
                "<script>updateProgress(\"Encoding failed!\")</script>\n");
            send_chunked_end(fd);
            ri_scan_set_progress(NULL, NULL);
            close(fd);
            return;
        }

        /* Build the finish script: <script>finishWithHtml("BASE64")</script> */
        size_t b64_len = strlen(b64);
        size_t script_len = 34 + b64_len + 11; /* prefix + b64 + suffix */
        char *script = malloc(script_len + 1);
        if (script) {
            snprintf(script, script_len + 1,
                     "<script>finishWithHtml(\"%s\")</script>\n", b64);
            send_chunk(fd, script, strlen(script));
            free(script);
        }
        free(b64);

        send_chunked_end(fd);
        ri_scan_set_progress(NULL, NULL);
        close(fd);
        return;
    }

    send_error(fd, "404 Not Found", "Not found");
    close(fd);
}

/* ---------- public API -------------------------------------------------- */

int ri_web_serve(const ri_config_t *defaults)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0; /* OS-assigned */

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(srv);
        return 1;
    }

    socklen_t alen = sizeof(addr);
    getsockname(srv, (struct sockaddr *)&addr, &alen);
    int port = ntohs(addr.sin_port);

    if (listen(srv, 5) < 0) {
        perror("listen");
        close(srv);
        return 1;
    }

    char host[256];
    if (defaults->use_ip) {
        /* Detect primary non-loopback IP via UDP connect trick */
        int probe = socket(AF_INET, SOCK_DGRAM, 0);
        if (probe >= 0) {
            struct sockaddr_in remote;
            memset(&remote, 0, sizeof(remote));
            remote.sin_family = AF_INET;
            remote.sin_port = htons(53);
            inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr);
            if (connect(probe, (struct sockaddr *)&remote, sizeof(remote)) == 0) {
                struct sockaddr_in local;
                socklen_t llen = sizeof(local);
                getsockname(probe, (struct sockaddr *)&local, &llen);
                inet_ntop(AF_INET, &local.sin_addr, host, sizeof(host));
            } else {
                strncpy(host, "127.0.0.1", sizeof(host));
            }
            close(probe);
        } else {
            strncpy(host, "127.0.0.1", sizeof(host));
        }
    } else {
        if (gethostname(host, sizeof(host)) != 0)
            strncpy(host, "localhost", sizeof(host));
    }

    printf("visual-traceroute web UI: http://%s:%d/\n", host, port);
    fflush(stdout);

    /* Auto-open browser */
    char cmd[512];
#ifdef RI_DARWIN
    snprintf(cmd, sizeof(cmd), "open http://%s:%d/ 2>/dev/null &", host, port);
#else
    snprintf(cmd, sizeof(cmd), "xdg-open http://%s:%d/ 2>/dev/null &", host, port);
#endif
    (void)system(cmd);

    /* Install Ctrl-C handler */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    while (!g_shutdown) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };

        int ret = select(srv + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue; /* timeout, check g_shutdown */

        int client = accept(srv, NULL, NULL);
        if (client < 0) {
            if (errno == EINTR) continue;
            break;
        }
        handle_request(client, defaults);
    }

    close(srv);
    printf("\nShutting down.\n");
    return 0;
}
