#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #define close closesocket
  #define ssize_t int
  typedef int socklen_t;
  /* strcasestr not on Windows — provide our own */
  static char *strcasestr_win(const char *h, const char *n) {
      if (!*n) return (char*)h;
      for (; *h; h++) {
          if (tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
              const char *hp = h, *np = n;
              while (*hp && *np && tolower((unsigned char)*hp)==tolower((unsigned char)*np)) { hp++; np++; }
              if (!*np) return (char*)h;
          }
      }
      return NULL;
  }
  #define strcasestr strcasestr_win
  #define SIGPIPE 0
  static void signal(int s, void *h) { (void)s; (void)h; }
  #define SIG_IGN NULL
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <sys/types.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <signal.h>
#endif

#include "../include/http.h"
#include "../include/value.h"
#include "../include/vm.h"
#include "../include/alz_stdlib.h"

/* ═══════════════════════════════════════════════════════════════════════════
   UTILITIES
═══════════════════════════════════════════════════════════════════════════ */

HttpMethod http_method_parse(const char *s) {
    if (!s) return HTTP_UNKNOWN;
    if (strcmp(s, "GET")    == 0) return HTTP_GET;
    if (strcmp(s, "POST")   == 0) return HTTP_POST;
    if (strcmp(s, "PUT")    == 0) return HTTP_PUT;
    if (strcmp(s, "DELETE") == 0) return HTTP_DELETE;
    if (strcmp(s, "PATCH")  == 0) return HTTP_PATCH;
    return HTTP_UNKNOWN;
}

const char *http_method_str(HttpMethod m) {
    switch (m) {
        case HTTP_GET:    return "GET";
        case HTTP_POST:   return "POST";
        case HTTP_PUT:    return "PUT";
        case HTTP_DELETE: return "DELETE";
        case HTTP_PATCH:  return "PATCH";
        default:          return "UNKNOWN";
    }
}

const char *http_status_str(int code) {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 409: return "Conflict";
        case 422: return "Unprocessable Entity";
        case 429: return "Too Many Requests";
        case 500: return "Internal Server Error";
        default:  return "Unknown";
    }
}

void url_decode(char *dst, const char *src, size_t max) {
    size_t i = 0;
    while (*src && i < max - 1) {
        if (*src == '%' && isxdigit((unsigned char)src[1]) && isxdigit((unsigned char)src[2])) {
            char hex[3] = {src[1], src[2], '\0'};
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' '; src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

/* ═══════════════════════════════════════════════════════════════════════════
   REQUEST PARSER
═══════════════════════════════════════════════════════════════════════════ */

int http_parse_request(const char *raw, size_t len, HttpRequest *req) {
    if (!raw || len == 0) return 0;
    memset(req, 0, sizeof(HttpRequest));

    char method_str[16] = {0};
    char full_path[512] = {0};
    const char *p = raw;
    const char *end = raw + len;

    int i = 0;
    while (p < end && *p != ' ' && i < 15) method_str[i++] = *p++;
    if (p >= end) return 0;
    p++;

    i = 0;
    while (p < end && *p != ' ' && *p != '\r' && *p != '\n' && i < 511)
        full_path[i++] = *p++;
    if (p >= end) return 0;

    while (p < end && *p != '\n') p++;
    if (p < end) p++;

    req->method = http_method_parse(method_str);

    char *q = strchr(full_path, '?');
    if (q) { *q = '\0'; url_decode(req->query, q + 1, sizeof(req->query)); }
    url_decode(req->path, full_path, sizeof(req->path));

    req->header_count = 0;
    while (p < end && req->header_count < 32) {
        if (*p == '\r' || *p == '\n') {
            if (*p == '\r') p++;
            if (p < end && *p == '\n') p++;
            break;
        }
        char key[64] = {0}, val[256] = {0};
        i = 0;
        while (p < end && *p != ':' && i < 63) key[i++] = *p++;
        if (p < end && *p == ':') p++;
        while (p < end && *p == ' ') p++;
        i = 0;
        while (p < end && *p != '\r' && *p != '\n' && i < 255) val[i++] = *p++;
        while (p < end && (*p == '\r' || *p == '\n')) p++;

        strncpy(req->header_keys[req->header_count], key, 63);
        strncpy(req->header_vals[req->header_count], val, 255);
        if (strcasecmp(key, "Content-Type") == 0)
            strncpy(req->content_type, val, sizeof(req->content_type)-1);
        req->header_count++;
    }

    if (p < end) {
        req->body_len = end - p;
        if (req->body_len >= sizeof(req->body)) req->body_len = sizeof(req->body) - 1;
        memcpy(req->body, p, req->body_len);
        req->body[req->body_len] = '\0';
    }
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
   RESPONSE BUILDER
═══════════════════════════════════════════════════════════════════════════ */

char *http_build_response(HttpResponse *res, size_t *out_len) {
    const char *status_str = http_status_str(res->status);
    const char *ct = res->content_type[0] ? res->content_type : "application/json";
    char headers[1024];
    int hlen = snprintf(headers, sizeof(headers),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "\r\n",
        res->status, status_str, ct, res->body_len);
    size_t total = hlen + res->body_len;
    char *out = malloc(total + 1);
    memcpy(out, headers, hlen);
    if (res->body && res->body_len > 0) memcpy(out + hlen, res->body, res->body_len);
    out[total] = '\0';
    if (out_len) *out_len = total;
    return out;
}

/* ═══════════════════════════════════════════════════════════════════════════
   JSON
═══════════════════════════════════════════════════════════════════════════ */

static char *json_escape(const char *s) {
    size_t len = strlen(s);
    char *out = malloc(len * 6 + 3);
    size_t j = 0;
    out[j++] = '"';
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
            case '"':  out[j++]='\\'; out[j++]='"';  break;
            case '\\': out[j++]='\\'; out[j++]='\\'; break;
            case '\n': out[j++]='\\'; out[j++]='n';  break;
            case '\r': out[j++]='\\'; out[j++]='r';  break;
            case '\t': out[j++]='\\'; out[j++]='t';  break;
            default:
                if (c < 0x20) { j += sprintf(out+j, "\\u%04x", c); }
                else { out[j++] = c; }
        }
    }
    out[j++] = '"'; out[j] = '\0';
    return out;
}

char *alz_to_json(AlzValue *val) {
    if (!val) return strdup("null");
    char buf[64];
    switch (val->type) {
        case VAL_NULL:    return strdup("null");
        case VAL_BOOL:    return strdup(val->as.boolean ? "true" : "false");
        case VAL_NUMBER:
            if (val->as.number == (long long)val->as.number)
                snprintf(buf, sizeof(buf), "%lld", (long long)val->as.number);
            else
                snprintf(buf, sizeof(buf), "%g", val->as.number);
            return strdup(buf);
        case VAL_STRING:
        case VAL_MODULE:
            return json_escape(val->as.string);
        case VAL_FUNCTION: return strdup("\"<function>\"");
        case VAL_LIST: {
            size_t cap = 64; char *out = malloc(cap); size_t len = 0;
            out[len++] = '[';
            for (size_t i = 0; i < val->as.list->count; i++) {
                char *item = alz_to_json(val->as.list->items[i]);
                size_t ilen = strlen(item);
                while (len + ilen + 4 >= cap) { cap *= 2; out = realloc(out, cap); }
                if (i > 0) out[len++] = ',';
                memcpy(out + len, item, ilen); len += ilen;
                free(item);
            }
            while (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = ']'; out[len] = '\0';
            return out;
        }
        case VAL_OBJECT: {
            size_t cap = 64; char *out = malloc(cap); size_t len = 0;
            int first = 1;
            out[len++] = '{';
            for (size_t i = 0; i < val->as.object->count; i++) {
                if (strncmp(val->as.object->entries[i].key, "__", 2) == 0) continue;
                char *key = json_escape(val->as.object->entries[i].key);
                char *v   = alz_to_json(val->as.object->entries[i].val);
                size_t klen = strlen(key), vlen = strlen(v);
                while (len + klen + vlen + 5 >= cap) { cap *= 2; out = realloc(out, cap); }
                if (!first) out[len++] = ',';
                first = 0;
                memcpy(out + len, key, klen); len += klen;
                out[len++] = ':';
                memcpy(out + len, v, vlen); len += vlen;
                free(key); free(v);
            }
            while (len + 2 >= cap) { cap *= 2; out = realloc(out, cap); }
            out[len++] = '}'; out[len] = '\0';
            return out;
        }
    }
    return strdup("null");
}

static const char *json_skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static AlzValue *json_parse_value(const char **p);

static AlzValue *json_parse_string(const char **p) {
    (*p)++;
    char buf[4096]; size_t i = 0;
    while (**p && **p != '"' && i < sizeof(buf)-1) {
        if (**p == '\\') {
            (*p)++;
            switch (**p) {
                case '"':  buf[i++] = '"';  break;
                case '\\': buf[i++] = '\\'; break;
                case 'n':  buf[i++] = '\n'; break;
                case 'r':  buf[i++] = '\r'; break;
                case 't':  buf[i++] = '\t'; break;
                default:   buf[i++] = **p;  break;
            }
        } else { buf[i++] = **p; }
        (*p)++;
    }
    buf[i] = '\0';
    if (**p == '"') (*p)++;
    return alz_string(buf);
}

static AlzValue *json_parse_array(const char **p) {
    (*p)++;
    AlzValue *list = alz_list();
    *p = json_skip_ws(*p);
    if (**p == ']') { (*p)++; return list; }
    while (**p) {
        *p = json_skip_ws(*p);
        AlzValue *item = json_parse_value(p);
        alz_list_push(list->as.list, item);
        *p = json_skip_ws(*p);
        if (**p == ',') (*p)++;
        else if (**p == ']') { (*p)++; break; }
        else break;
    }
    return list;
}

static AlzValue *json_parse_object(const char **p) {
    (*p)++;
    AlzValue *obj = alz_object();
    *p = json_skip_ws(*p);
    while (**p && **p != '}') {
        *p = json_skip_ws(*p);
        if (**p != '"') break;
        AlzValue *key = json_parse_string(p);
        char *ks = alz_to_string(key); alz_free(key);
        *p = json_skip_ws(*p);
        if (**p == ':') (*p)++;
        *p = json_skip_ws(*p);
        AlzValue *val = json_parse_value(p);
        alz_obj_set(obj->as.object, ks, val);
        free(ks);
        *p = json_skip_ws(*p);
        if (**p == ',') { (*p)++; continue; }
        if (**p == '}') break;
        break;
    }
    if (**p == '}') (*p)++;
    return obj;
}

static AlzValue *json_parse_value(const char **p) {
    *p = json_skip_ws(*p);
    if (!**p) return alz_null();
    if (**p == '"')  return json_parse_string(p);
    if (**p == '[')  return json_parse_array(p);
    if (**p == '{')  return json_parse_object(p);
    if (strncmp(*p, "true",  4) == 0) { *p += 4; return alz_bool(1); }
    if (strncmp(*p, "false", 5) == 0) { *p += 5; return alz_bool(0); }
    if (strncmp(*p, "null",  4) == 0) { *p += 4; return alz_null(); }
    char *end; double n = strtod(*p, &end);
    if (end != *p) { *p = end; return alz_number(n); }
    return alz_null();
}

AlzValue *alz_from_json(const char *json) {
    if (!json) return alz_null();
    const char *p = json;
    return json_parse_value(&p);
}

/* ═══════════════════════════════════════════════════════════════════════════
   SERVER
═══════════════════════════════════════════════════════════════════════════ */

AlzServer *server_new(VM *vm, int port) {
    AlzServer *srv = calloc(1, sizeof(AlzServer));
    srv->port = port; srv->sock_fd = -1; srv->vm = vm;
    return srv;
}

void server_free(AlzServer *srv) {
    if (!srv) return;
    if (srv->sock_fd >= 0) close(srv->sock_fd);
    free(srv);
}

void server_add_route(AlzServer *srv, HttpMethod method,
                      const char *path, const char *handler) {
    if (srv->route_count >= ALZ_MAX_ROUTES) return;
    Route *r = &srv->routes[srv->route_count++];
    r->method = method;
    strncpy(r->path,    path,    sizeof(r->path)-1);
    strncpy(r->handler, handler, sizeof(r->handler)-1);
}

static int route_match(const char *pattern, const char *path, AlzValue *params) {
    const char *pp = pattern, *rp = path;
    while (*pp && *rp) {
        if (*pp == ':') {
            pp++;
            char pname[64] = {0}; int i = 0;
            while (*pp && *pp != '/' && i < 63) pname[i++] = *pp++;
            char pval[256] = {0}; i = 0;
            while (*rp && *rp != '/' && i < 255) pval[i++] = *rp++;
            if (params && params->type == VAL_OBJECT)
                alz_obj_set(params->as.object, pname, alz_string(pval));
        } else {
            if (*pp != *rp) return 0;
            pp++; rp++;
        }
    }
    if (*pp == '/' && !*rp) pp++;
    if (*rp == '/' && !*pp) rp++;
    return (*pp == '\0' && *rp == '\0');
}

static void handle_connection(AlzServer *srv, int client_fd) {
    char raw[66000]; ssize_t total = 0, n;
    while (total < (ssize_t)(sizeof(raw) - 1)) {
        n = recv(client_fd, raw + total, sizeof(raw) - total - 1, 0);
        if (n <= 0) break;
        total += n;
        raw[total] = '\0';
        if (strstr(raw, "\r\n\r\n")) break;
    }
    /* Read body based on Content-Length */
    char *cl_hdr = strcasestr(raw, "content-length:");
    if (cl_hdr) {
        long content_len = strtol(cl_hdr + 15, NULL, 10);
        char *body_start = strstr(raw, "\r\n\r\n");
        if (body_start) {
            body_start += 4;
            long already_read = (raw + total) - body_start;
            long remaining = content_len - already_read;
            while (remaining > 0 && total < (ssize_t)(sizeof(raw) - 1)) {
                n = recv(client_fd, raw + total, remaining, 0);
                if (n <= 0) break;
                total += n; raw[total] = '\0'; remaining -= n;
            }
        }
    }
    raw[total] = '\0';

    HttpRequest req;
    if (!http_parse_request(raw, total, &req)) { close(client_fd); return; }

    printf("  %s %s\n", http_method_str(req.method), req.path);

    Route   *matched    = NULL;
    AlzValue *url_params = alz_object();
    for (int i = 0; i < srv->route_count; i++) {
        Route *r = &srv->routes[i];
        if (r->method != req.method && r->method != HTTP_UNKNOWN) continue;
        if (route_match(r->path, req.path, url_params)) { matched = r; break; }
    }

    HttpResponse res; memset(&res, 0, sizeof(res));
    res.status = 200; strcpy(res.content_type, "application/json");
    char *resp_body = NULL;

    if (!matched) {
        res.status = 404; resp_body = strdup("{\"error\":\"Not Found\"}");
    } else {
        AlzValue *req_obj = alz_object();
        alz_obj_set(req_obj->as.object, "method", alz_string(http_method_str(req.method)));
        alz_obj_set(req_obj->as.object, "path",   alz_string(req.path));
        alz_obj_set(req_obj->as.object, "query",  alz_string(req.query));
        alz_obj_set(req_obj->as.object, "params", url_params);
        if (strstr(req.content_type, "application/json") && req.body_len > 0)
            alz_obj_set(req_obj->as.object, "body", alz_from_json(req.body));
        else
            alz_obj_set(req_obj->as.object, "body", alz_string(req.body));

        vm_set_global(srv->vm, "request", req_obj);
        vm_set_global(srv->vm, "method",  alz_string(http_method_str(req.method)));
        vm_set_global(srv->vm, "path",    alz_string(req.path));
        vm_set_global(srv->vm, "query",   alz_string(req.query));
        vm_set_global(srv->vm, "params",  alz_copy(url_params));
        if (req.body_len > 0 && strstr(req.content_type, "application/json"))
            vm_set_global(srv->vm, "body", alz_from_json(req.body));
        else
            vm_set_global(srv->vm, "body", alz_string(req.body));

        vm_set_global(srv->vm, "__response__", alz_null());

        AlzValue *handler_fn = vm_get_global(srv->vm, matched->handler);
        if (handler_fn && handler_fn->type == VAL_FUNCTION) {
            AlzFunc *fn = handler_fn->as.func;
            size_t saved = srv->vm->var_count;
            vm_run(srv->vm, (Chunk *)fn->chunk);
            for (size_t i = saved; i < srv->vm->var_count; i++) {
                free(srv->vm->vars[i].name);
                alz_free(srv->vm->vars[i].val);
            }
            srv->vm->var_count = saved;
        }

        AlzValue *resp_val = vm_get_global(srv->vm, "__response__");
        resp_body = (resp_val && resp_val->type != VAL_NULL)
                    ? alz_to_json(resp_val)
                    : strdup("{\"ok\":true}");
    }

    res.body = resp_body; res.body_len = strlen(resp_body);
    size_t resp_len;
    char *response = http_build_response(&res, &resp_len);
    send(client_fd, response, resp_len, 0);
    free(response); free(resp_body);
    alz_free(url_params);
    close(client_fd);
}

void server_start(AlzServer *srv) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#else
    signal(SIGPIPE, SIG_IGN);
#endif

    srv->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (srv->sock_fd < 0) { perror("alz: socket"); return; }

    int opt = 1;
    setsockopt(srv->sock_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(srv->port);

    if (bind(srv->sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("alz: bind"); close(srv->sock_fd); return;
    }
    if (listen(srv->sock_fd, 128) < 0) {
        perror("alz: listen"); close(srv->sock_fd); return;
    }

    printf("\n🚀  AlzScript server running\n");
    printf("    http://localhost:%d\n\n", srv->port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(srv->sock_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) { if (errno == EINTR) continue; break; }
        handle_connection(srv, client_fd);
    }
}
