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
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
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

/* ---------- scan ID counter -------------------------------------------- */

static unsigned int g_scan_counter;

static void make_scan_id(char *buf, size_t len)
{
    snprintf(buf, len, "%08x", g_scan_counter++);
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

static void send_redirect(int fd, const char *location)
{
    char hdr[512];
    int hlen = snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 303 See Other\r\n"
        "Location: %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n", location);
    write(fd, hdr, (size_t)hlen);
}

/* ---------- file-based progress callback ------------------------------- */

static void file_progress_cb(const char *msg, void *ctx)
{
    const char *log_path = (const char *)ctx;
    FILE *f = fopen(log_path, "a");
    if (f) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

/* ---------- child scan process ----------------------------------------- */

static void run_scan_child(const ri_config_t *cfg, const char *scan_dir,
                           int listen_fd)
{
    close(listen_fd);

    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/progress.log", scan_dir);

    char result_path[512];
    snprintf(result_path, sizeof(result_path), "%s/result.html", scan_dir);

    char done_path[512];
    snprintf(done_path, sizeof(done_path), "%s/done", scan_dir);

    char error_path[512];
    snprintf(error_path, sizeof(error_path), "%s/error", scan_dir);

    ri_scan_set_progress(file_progress_cb, log_path);

    ri_graph_t *g = ri_scan_run(cfg);
    if (!g) {
        FILE *f = fopen(error_path, "w");
        if (f) { fprintf(f, "Scan failed\n"); fclose(f); }
        ri_scan_set_progress(NULL, NULL);
        _exit(1);
    }

    file_progress_cb("Computing MST...", log_path);
    ri_graph_kruskal_mst(g);

    file_progress_cb("Computing 3D layout...", log_path);
    ri_layout_3d(g);
    ri_layout_force_refine(g, 50);

    file_progress_cb("Generating visualization...", log_path);
    int rc = ri_out_html(g, result_path);
    ri_graph_destroy(g);

    if (rc != 0) {
        FILE *f = fopen(error_path, "w");
        if (f) { fprintf(f, "HTML generation failed\n"); fclose(f); }
        ri_scan_set_progress(NULL, NULL);
        _exit(1);
    }

    /* Write done sentinel */
    FILE *f = fopen(done_path, "w");
    if (f) { fprintf(f, "done\n"); fclose(f); }

    ri_scan_set_progress(NULL, NULL);
    _exit(0);
}

/* ---------- progress page builder -------------------------------------- */

static char *build_progress_page(const char *scan_id)
{
    const char *fmt =
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
        "border-top-color:#5577ff;border-radius:50%%;animation:spin 0.8s linear infinite}"
        "#status{font-size:14px;color:#aac;margin-bottom:12px;min-height:20px}"
        "#log{text-align:left;background:#0d0d1a;border:1px solid #2a2a4a;"
        "border-radius:8px;padding:10px 12px;max-height:240px;overflow-y:auto;"
        "font-size:12px;font-family:'SF Mono',Menlo,Consolas,monospace;"
        "line-height:1.6;color:#8a8aaa}"
        "</style></head><body>"
        "<div class=\"card\">"
        "<h1>Scanning...</h1>"
        "<div class=\"sub\">Network Discovery &amp; 3D Visualization</div>"
        "<div class=\"spinner\" id=\"spinner\"></div>"
        "<div id=\"status\">Initializing...</div>"
        "<div id=\"log\"></div>"
        "</div>"
        "<script>"
        "var sid='%s',seen=0;"
        "function poll(){"
        "fetch('/api/status/'+sid).then(function(r){return r.json();})"
        ".then(function(d){"
        "var log=document.getElementById('log');"
        "for(var i=seen;i<d.lines.length;i++){"
        "log.innerHTML+='<div>'+d.lines[i].replace(/</g,'&lt;')+'</div>';}"
        "seen=d.lines.length;"
        "log.scrollTop=log.scrollHeight;"
        "if(d.lines.length>0)document.getElementById('status').textContent=d.lines[d.lines.length-1];"
        "if(d.status==='done'){window.location='/result/'+sid;return;}"
        "if(d.status==='error'){document.getElementById('status').textContent='Scan failed!';"
        "document.getElementById('spinner').style.display='none';return;}"
        "setTimeout(poll,2000);"
        "}).catch(function(){setTimeout(poll,3000);});}"
        "poll();"
        "</script></body></html>";

    size_t len = strlen(fmt) + 32;
    char *page = malloc(len);
    if (page)
        snprintf(page, len, fmt, scan_id);
    return page;
}

/* ---------- temp dir cleanup ------------------------------------------- */

static void cleanup_temp_dirs(void)
{
    DIR *d = opendir("/tmp");
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, "vt-", 3) != 0) continue;
        char path[512];
        /* Remove known files inside the scan directory */
        const char *files[] = {
            "progress.log", "result.html", "done", "error"
        };
        for (int i = 0; i < 4; i++) {
            snprintf(path, sizeof(path), "/tmp/%s/%s", ent->d_name, files[i]);
            unlink(path);
        }
        snprintf(path, sizeof(path), "/tmp/%s", ent->d_name);
        rmdir(path);
    }
    closedir(d);
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
"<form class=\"card\" action=\"/scan\" method=\"POST\">"
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
"<div class=\"foot\">Ctrl-C in terminal to stop server.</div>"
"</form></body></html>";

/* ---------- request handler --------------------------------------------- */

static void handle_request(int fd, int srv_fd, const ri_config_t *defaults)
{
    char buf[8192];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) { close(fd); return; }
    buf[n] = '\0';

    /* Parse method and path */
    char method[8] = {0}, path[256] = {0};
    sscanf(buf, "%7s %255s", method, path);

    /* GET / — serve form */
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        send_response(fd, "200 OK", "text/html",
                      FORM_PAGE, sizeof(FORM_PAGE) - 1);
        close(fd);
        return;
    }

    /* POST /scan — fork child, redirect to progress page */
    if (strcmp(method, "POST") == 0 && strcmp(path, "/scan") == 0) {
        const char *body = strstr(buf, "\r\n\r\n");
        if (!body) { send_error(fd, "400 Bad Request", "Malformed"); close(fd); return; }
        body += 4;

        ri_config_t cfg = *defaults;
        cfg.web_mode = 0;

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

        /* Create scan ID and temp directory */
        char scan_id[16];
        make_scan_id(scan_id, sizeof(scan_id));

        char scan_dir[256];
        snprintf(scan_dir, sizeof(scan_dir), "/tmp/vt-%s", scan_id);
        if (mkdir(scan_dir, 0700) != 0) {
            send_error(fd, "500 Internal Server Error", "Cannot create temp dir");
            close(fd);
            return;
        }

        LOG_INFO("Web scan [%s]: target=%s depth=%d",
                 scan_id, cfg.has_target ? cfg.target : "(local)", cfg.max_depth);

        pid_t pid = fork();
        if (pid < 0) {
            send_error(fd, "500 Internal Server Error", "Fork failed");
            close(fd);
            return;
        }

        if (pid == 0) {
            /* Child process */
            close(fd);
            run_scan_child(&cfg, scan_dir, srv_fd);
            /* run_scan_child calls _exit() — never returns */
        }

        /* Parent: redirect browser to progress page */
        char location[128];
        snprintf(location, sizeof(location), "/progress/%s", scan_id);
        send_redirect(fd, location);
        close(fd);
        return;
    }

    /* GET /progress/{id} — serve polling progress page */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/progress/", 10) == 0) {
        const char *scan_id = path + 10;
        char scan_dir[256];
        snprintf(scan_dir, sizeof(scan_dir), "/tmp/vt-%s", scan_id);

        /* Check scan dir exists */
        struct stat st;
        if (stat(scan_dir, &st) != 0) {
            send_error(fd, "404 Not Found", "Unknown scan ID");
            close(fd);
            return;
        }

        /* If done, redirect straight to result */
        char done_path[512];
        snprintf(done_path, sizeof(done_path), "%s/done", scan_dir);
        if (stat(done_path, &st) == 0) {
            char location[128];
            snprintf(location, sizeof(location), "/result/%s", scan_id);
            send_redirect(fd, location);
            close(fd);
            return;
        }

        char *page = build_progress_page(scan_id);
        if (!page) {
            send_error(fd, "500 Internal Server Error", "OOM");
            close(fd);
            return;
        }
        send_response(fd, "200 OK", "text/html", page, strlen(page));
        free(page);
        close(fd);
        return;
    }

    /* GET /api/status/{id} — JSON status for polling */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/api/status/", 12) == 0) {
        const char *scan_id = path + 12;
        char scan_dir[256];
        snprintf(scan_dir, sizeof(scan_dir), "/tmp/vt-%s", scan_id);

        struct stat st;
        if (stat(scan_dir, &st) != 0) {
            const char *json = "{\"status\":\"error\",\"lines\":[\"Unknown scan ID\"]}";
            send_response(fd, "200 OK", "application/json", json, strlen(json));
            close(fd);
            return;
        }

        /* Determine status */
        const char *status_str = "running";
        char done_path[512], error_path[512];
        snprintf(done_path, sizeof(done_path), "%s/done", scan_dir);
        snprintf(error_path, sizeof(error_path), "%s/error", scan_dir);
        if (stat(done_path, &st) == 0)
            status_str = "done";
        else if (stat(error_path, &st) == 0)
            status_str = "error";

        /* Read progress.log lines */
        char log_path[512];
        snprintf(log_path, sizeof(log_path), "%s/progress.log", scan_dir);

        /* Build JSON response with lines array */
        char *json = malloc(65536);
        if (!json) {
            send_error(fd, "500 Internal Server Error", "OOM");
            close(fd);
            return;
        }

        int jlen = snprintf(json, 65536, "{\"status\":\"%s\",\"lines\":[", status_str);

        FILE *lf = fopen(log_path, "r");
        if (lf) {
            char line[512];
            int first = 1;
            while (fgets(line, sizeof(line), lf)) {
                /* Strip trailing newline */
                size_t ll = strlen(line);
                while (ll > 0 && (line[ll-1] == '\n' || line[ll-1] == '\r'))
                    line[--ll] = '\0';
                if (ll == 0) continue;

                /* JSON-escape the line */
                char escaped[1024];
                size_t ei = 0;
                for (size_t li = 0; li < ll && ei < sizeof(escaped) - 4; li++) {
                    switch (line[li]) {
                    case '"':  escaped[ei++] = '\\'; escaped[ei++] = '"'; break;
                    case '\\': escaped[ei++] = '\\'; escaped[ei++] = '\\'; break;
                    case '\t': escaped[ei++] = ' '; break;
                    default:
                        if ((unsigned char)line[li] >= 0x20)
                            escaped[ei++] = line[li];
                        break;
                    }
                }
                escaped[ei] = '\0';

                if (jlen < 65000) {
                    jlen += snprintf(json + jlen, 65536 - (size_t)jlen,
                                     "%s\"%s\"", first ? "" : ",", escaped);
                    first = 0;
                }
            }
            fclose(lf);
        }

        jlen += snprintf(json + jlen, 65536 - (size_t)jlen, "]}");

        send_response(fd, "200 OK", "application/json", json, (size_t)jlen);
        free(json);
        close(fd);
        return;
    }

    /* GET /result/{id} — serve the generated HTML */
    if (strcmp(method, "GET") == 0 && strncmp(path, "/result/", 8) == 0) {
        const char *scan_id = path + 8;
        char result_path[512];
        snprintf(result_path, sizeof(result_path), "/tmp/vt-%s/result.html", scan_id);

        FILE *rf = fopen(result_path, "r");
        if (!rf) {
            send_error(fd, "404 Not Found", "Result not ready");
            close(fd);
            return;
        }

        /* Get file size */
        fseek(rf, 0, SEEK_END);
        long fsize = ftell(rf);
        fseek(rf, 0, SEEK_SET);

        if (fsize <= 0 || fsize > 50 * 1024 * 1024) {
            fclose(rf);
            send_error(fd, "500 Internal Server Error", "Bad result file");
            close(fd);
            return;
        }

        char *html = malloc((size_t)fsize);
        if (!html) {
            fclose(rf);
            send_error(fd, "500 Internal Server Error", "OOM");
            close(fd);
            return;
        }

        size_t nread = fread(html, 1, (size_t)fsize, rf);
        fclose(rf);

        send_response(fd, "200 OK", "text/html", html, nread);
        free(html);
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

    /* Auto-reap child processes */
    struct sigaction sa_chld;
    memset(&sa_chld, 0, sizeof(sa_chld));
    sa_chld.sa_handler = SIG_DFL;
    sa_chld.sa_flags = SA_NOCLDWAIT;
    sigaction(SIGCHLD, &sa_chld, NULL);

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
        handle_request(client, srv, defaults);
    }

    close(srv);
    cleanup_temp_dirs();
    printf("\nShutting down.\n");
    return 0;
}
