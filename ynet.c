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
#include <sys/types.h>   
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>

#include <ycommon.h>
#include <ytrace.h>
#include <ynet.h>

/**
 * Connect to given address. Refer ynet.h for details
 */
int ynet_connect(ynet_ctx_t *ctx, struct sockaddr_in *srv)
{
  char      buffer[256];
  struct sockaddr_in serv_addr;

  ctx->sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (ctx->sfd < 0)
  {
    ytrace_msg(YTRACE_ERROR, "socket creation failed : %d\n", errno);
    return -1;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(YNET_SER_PORT);

  if (connect(ctx->sfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0)
  {
    ytrace_msg(YTRACE_ERROR, "connect failed : %d\n", errno);
    close(ctx->sfd);
    ctx->sfd = -1;

    return -1;
  }

  ytrace_msg(YTRACE_LEVEL1, "connected to port = %d, fd = %d\n",
             serv_addr.sin_port, ctx->sfd);

  return 0;
  
}

/**
 * Send buffer to given network context. Refer ynet.h for details. 
 */
int ynet_send(ynet_ctx_t *ctx, ybuf_t *buf)
{
  int rc;
  int rem = ybuf_rem(buf);

  ytrace_msg(YTRACE_LEVEL1, "ynet_send : enter\n",  ctx->sfd, rem);

  rc = write(ctx->sfd, (void *)buf->sp, rem);

  if (rc < 0)
    return y_error(errno);

  buf->sp += rc;

  ytrace_msg(YTRACE_LEVEL1, "ynet_send : sfd = %d : sent %d bytes\n",  ctx->sfd, rc);

  return 0;
}

/**
 * Receive from given network context. Refer ynet.h for details. 
 */
int ynet_recv(ynet_ctx_t *ctx, ybuf_t *buf)
{
  int   rc;

  ytrace_msg(YTRACE_LEVEL1, "sfd = %d, buf bp = %p, fre = %d \n",
             ctx->sfd, buf->ep, buf->fre);

  rc = read(ctx->sfd, (void *)buf->ep, buf->fre);

  if (rc < 0)
    return y_error(errno);

  buf->fre -= rc;
  buf->ep  += rc;

  ytrace_msg(YTRACE_LEVEL1, "buf bp (2) = %p, %d rc = %d\n",
             buf->ep, buf->fre,rc);

  return 0;
}

/**
 * Close given network context. Refer ynet.h for details
 */
int ynet_close(ynet_ctx_t *ctx)
{
  close(ctx->sfd);
  return 0;
}
