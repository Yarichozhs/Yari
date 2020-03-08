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
#ifndef _YNET_H

#include <netinet/in.h>

#define _YNET_H

#include <ycommon.h>

/**
 * @brief Server host and port information. 
 */
#define YNET_SER_PORT     22000
#define YNET_SER_HOST    "127.0.0.1"

/**
 * @brief Network class for given context ( associate socket fd with 
 *        this class. 
 */
#define YNET_CLASS_NONE   0
#define YNET_CLASS_WAIT   1
#define YNET_CLASS_LSNR   2
#define YNET_CLASS_MSG    3
#define YNET_CLASS_PIPE   4
#define YNET_CLASS_EVENT  5

struct ynet_ctx_t
{
  int class;
  int sfd;
};
typedef struct ynet_ctx_t ynet_ctx_t;

/**
 * @brief Initialize given context with a class and socket. 
 */
#define ynet_ctx_init(ctx, cls, fd) \
        do                          \
        {                           \
          (ctx)->class = (cls);     \
          (ctx)->sfd   = (fd);      \
        }                           \
        while (FALSE)

/**
 * @brief Connect to given address with given network context. 
 * 
 * @param ctx  - network context 
 * @param srv  - server address to connect to.
 * 
 * @return 0 on success, -1 on failure. 
 */
int ynet_connect(ynet_ctx_t *ctx, struct sockaddr_in *srv);

/**
 * @brief Wait on given network context. 
 * 
 * @param ctx - network context 
 * 
 * @return 0 on success, -1 on failure. 
 */
int ynet_wait(ynet_ctx_t *ctx);

/**
 * @brief Send given buffer content on given network context. 
 * 
 * @param ctx - network context 
 * @param buf - buffer content
 * 
 * @return 0 on success, -1 on failure. 
 */
int ynet_send(ynet_ctx_t *ctx, ybuf_t *buf);

/**
 * @brief Receive from given network context and place the data in buffer. 
 * 
 * @param ctx - network context 
 * @param buf - buffer to receive
 * 
 * @return 0 on success, -1 on failure. 
 */
int ynet_recv(ynet_ctx_t *ctx, ybuf_t *buf);

/**
 * @brief Close given network context
 * 
 * @param ctx - network context 
 * 
 * @return 0 on success, -1 on failure. 
 */
int ynet_close(ynet_ctx_t *ctx);

#endif /* ynet.h */
