#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>   
#include <sys/socket.h>
#include <netinet/in.h>

#include <redislib.h>

#define REDIS_OK_STR "+OK"
#define REDIS_OK_LEN  3

int redis_connect(redis_ctx_t *ctx, char *ip, int port)
{
  char   buffer[256];
  struct sockaddr_in serv_addr;

  if (ip == NULL)
    ip = REDIS_SERVER_DEFAULT_IP;

  if (port == 0)
    port = REDIS_SERVER_DEFAULT_PORT;

  ctx->sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (ctx->sfd < 0)
  {
    printf("socket creation failed : %d\n", errno); 
    return -1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(ip);
  serv_addr.sin_port        = htons(port);

  if (connect(ctx->sfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
  {
    printf("connect failed : %d\n", errno); 
    close(ctx->sfd);
    ctx->sfd = -1;

    return -1;
  }

 // printf("connected to ip = %s port = %d, fd = %d\n", ip, port, ctx->sfd);

  return 0;
}

int redis_get(redis_ctx_t *ctx, char *key, int klen, char *val, int *vlen)
{
  int    rc;
  char  *sp;
  char   buf[1024];

  struct iovec iovec[3];

  /* printf("redis_get : key [%.*s] %d\n", klen, key, klen); */

  iovec[0].iov_base = "GET ";
  iovec[0].iov_len  = strlen(iovec[0].iov_base);

  iovec[1].iov_base = key;
  iovec[1].iov_len  = klen;

  iovec[2].iov_base = "\n";
  iovec[2].iov_len  = 1;

  if ((rc = writev(ctx->sfd, iovec, sizeof(iovec)/sizeof(iovec[0]))) < 0)
    return rc;

  if ((rc = read(ctx->sfd, buf, sizeof(buf))) < 0)
    return 0;

  if ((sp = strstr(buf, "\n")) == NULL)
    return -1;

  if (sscanf(buf, "$%d\n", vlen) != 1)
    return -1;

  if (*vlen < 0)
    *vlen = 0;

  memcpy(val, sp + 1, *vlen);

  /* TODO : handle remaining data from socket */

  return 0;
}

int redis_set(redis_ctx_t *ctx, char *key, int klen, char *val, int vlen)
{
  int    rc;
  char  *sp;
  char   buf[64];

  struct iovec iovec[5];

  //printf("redis_set : key [%.*s] %d [%.*s] %d\n", klen, key, klen, vlen, val, vlen);

  iovec[0].iov_base = "SET ";
  iovec[0].iov_len  = strlen(iovec[0].iov_base);

  iovec[1].iov_base = key;
  iovec[1].iov_len  = klen;

  iovec[2].iov_base = " ";
  iovec[2].iov_len  = 1;

  iovec[3].iov_base = val;
  iovec[3].iov_len  = vlen;

  iovec[4].iov_base = "\n";
  iovec[4].iov_len  = 1;

  if ((rc = writev(ctx->sfd, iovec, sizeof(iovec)/sizeof(iovec[0]))) < 0)
    return rc;

  if ((rc = read(ctx->sfd, buf, sizeof(buf))) < 0)
    return 0;

  if (memcmp(buf, REDIS_OK_STR, REDIS_OK_LEN) != 0)
    return -1;

  return 0;
}

int redis_close(redis_ctx_t *ctx)
{
  int ret;

  ret = close(ctx->sfd);

  if (ret < 0)
    ret;

  ctx->sfd = -1;

  return ret;
}

#ifdef TEST
int main()
{
  char         out[4096];
  int          len;
  char        *key1 = "xyz";
  char        *key2 = "xyz2";
  char        *val1 = "1234";
  char        *val2 = "2345";
  int          klen1 = strlen(key1);
  int          vlen1 = strlen(val1);
  int          vlen2 = strlen(val2);
  redis_ctx_t  ctx;

  if (redis_connect(&ctx, NULL, 0) < 0)
  {
    printf("redis_connect failed\n");
    return 0;
  }

  if (redis_set(&ctx, key1, klen1, val1, vlen1) < 0)
  {
    printf("redis_get failed\n");
    return 0;
  }
  printf("main : set [%.*s] = [%.*s] %d\n", klen1, key1, vlen1, val1, vlen1);

  if (redis_get(&ctx, key1, klen1, out, &len) < 0)
  {
    printf("redis_get failed\n");
    return 0;
  }
  printf("main : get [%.*s] = [%.*s] %d\n", klen1, key1, len, out, len);

  if (redis_set(&ctx, key1, klen1, val2, vlen2) < 0)
  {
    printf("redis_get failed\n");
    return 0;
  }
  printf("main : set [%.*s] = [%.*s] %d\n", klen1, key1, vlen2, val2, vlen2);

  if (redis_get(&ctx, key1, klen1, out, &len) < 0)
  {
    printf("redis_get failed\n");
    return 0;
  }

  printf("main : get [%.*s] = [%.*s] %d\n", klen1, key1, len, out, len);

  redis_close(&ctx);
}
#endif














