#ifndef _REDISLIB_H

#define REDIS_SERVER_DEFAULT_IP    "127.0.0.1"
#define REDIS_SERVER_DEFAULT_PORT  6379

#define _REDISLIB_H

struct redis_ctx_t
{
  int sfd;
};
typedef struct redis_ctx_t redis_ctx_t;

int redis_connect(redis_ctx_t *ctx, char *ip, int port);
int redis_get(redis_ctx_t *ctx, char *key, int klen, char *val, int *vlen);
int redis_set(redis_ctx_t *ctx, char *key, int klen, char *val, int vlen);
int redis_close(redis_ctx_t *ctx);

#endif /* redislib.h */
