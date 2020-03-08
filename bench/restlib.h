#ifndef _RESTLIB_H

#define REST_SERVER_DEFAULT_IP    "127.0.0.1"
#define REST_SERVER_DEFAULT_PORT  8090

#define _RESTLIB_H

struct rest_ctx_t
{
  char server[512];
  char collection[512];
};
typedef struct rest_ctx_t rest_ctx_t;

int rest_connect(rest_ctx_t *ctx, char *ip, int port);
int rest_get(rest_ctx_t *ctx, char *key, int klen, char *val, int *vlen);
int rest_set(rest_ctx_t *ctx, char *key, int klen, char *val, int vlen);
int rest_close(rest_ctx_t *ctx);

#endif /* restlib.h */
