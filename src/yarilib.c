/*
 *  Yari - In memory Key Value Store 
 *  Copyright (C) 2017  Yari 
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <yarilib.h>
#include <ycommand.h>

int yari_connect(yari_ctx_t *ctx, char *ip, int port)
{
  struct sockaddr_in serv_addr;
  struct sockaddr_in *srv = NULL;

  if (port == 0)
    port = YNET_SER_PORT;

  if (ip)
  {
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port        = htons(port);
    srv = &serv_addr;
  }

  return ynet_connect(&ctx->ictx, srv);
}

int yari_set(yari_ctx_t *ctx, char *key, int klen, char *val, int vlen)
{
  return ycmd_client_process_set(&ctx->ictx, key, klen, val, vlen, 0);
}

int yari_get(yari_ctx_t *ctx, char *key, int klen, char *val, int *vlen)
{
  return ycmd_client_process_get(&ctx->ictx, key, klen, val, vlen);
}

int yari_close(yari_ctx_t *ctx)
{
  return 0;
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
  yari_ctx_t  ctx;

  if (yari_connect(&ctx, NULL, 0) < 0)
  {
    printf("yari_connect failed\n");
    return 0;
  }

  if (yari_set(&ctx, key1, klen1, val1, vlen1) < 0)
  {
    printf("yari_get failed\n");
    return 0;
  }
  printf("main : set [%.*s] = [%.*s] %d\n", klen1, key1, vlen1, val1, vlen1);

  if (yari_get(&ctx, key1, klen1, out, &len) < 0)
  {
    printf("yari_get failed\n");
    return 0;
  }
  printf("main : get [%.*s] = [%.*s] %d\n", klen1, key1, len, out, len);

  if (yari_set(&ctx, key1, klen1, val2, vlen2) < 0)
  {
    printf("yari_get failed\n");
    return 0;
  }
  printf("main : set [%.*s] = [%.*s] %d\n", klen1, key1, vlen2, val2, vlen2);

  if (yari_get(&ctx, key1, klen1, out, &len) < 0)
  {
    printf("yari_get failed\n");
    return 0;
  }

  printf("main : get [%.*s] = [%.*s] %d\n", klen1, key1, len, out, len);

  yari_close(&ctx);
}
#endif

