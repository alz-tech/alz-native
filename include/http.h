#ifndef ALZ_HTTP_H
#define ALZ_HTTP_H

#include <stddef.h>
#include "value.h"
#include "vm.h"

/* ── HTTP Method ─────────────────────────────────────────────────────────── */
typedef enum {
    HTTP_GET,
    HTTP_POST,
    HTTP_PUT,
    HTTP_DELETE,
    HTTP_PATCH,
    HTTP_UNKNOWN,
} HttpMethod;

/* ── Parsed HTTP Request ─────────────────────────────────────────────────── */
typedef struct {
    HttpMethod  method;
    char        path[512];
    char        query[512];     /* ?key=val&... */
    char        body[65536];    /* request body */
    size_t      body_len;
    char        content_type[128];

    /* Headers (simple key=val store) */
    char        header_keys[32][64];
    char        header_vals[32][256];
    int         header_count;
} HttpRequest;

/* ── HTTP Response ───────────────────────────────────────────────────────── */
typedef struct {
    int         status;
    char        content_type[128];
    char       *body;
    size_t      body_len;
} HttpResponse;

/* ── Route handler ───────────────────────────────────────────────────────── */
typedef struct {
    HttpMethod  method;
    char        path[512];
    /* Bytecode chunk index or function name to call */
    char        handler[128];
} Route;

/* ── Server ──────────────────────────────────────────────────────────────── */
#define ALZ_MAX_ROUTES 64

typedef struct {
    int         port;
    int         sock_fd;
    Route       routes[ALZ_MAX_ROUTES];
    int         route_count;
    VM         *vm;
} AlzServer;

/* ── API ─────────────────────────────────────────────────────────────────── */
AlzServer  *server_new(VM *vm, int port);
void        server_add_route(AlzServer *srv, HttpMethod method,
                             const char *path, const char *handler);
void        server_start(AlzServer *srv);   /* blocks — event loop */
void        server_free(AlzServer *srv);

/* ── Helpers ─────────────────────────────────────────────────────────────── */
HttpMethod  http_method_parse(const char *s);
const char *http_method_str(HttpMethod m);
const char *http_status_str(int code);

/* Parse raw HTTP request bytes into HttpRequest */
int         http_parse_request(const char *raw, size_t len, HttpRequest *req);

/* Build raw HTTP response string */
char       *http_build_response(HttpResponse *res, size_t *out_len);

/* URL decode: %20 → space etc */
void        url_decode(char *dst, const char *src, size_t max);

/* JSON helpers */
char       *alz_to_json(AlzValue *val);
AlzValue   *alz_from_json(const char *json);

#endif
