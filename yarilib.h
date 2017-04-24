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
#ifndef _YARILIB_H

#include <ynet.h>

#define _YARILIB_H

struct yari_ctx_t
{
  ynet_ctx_t ictx;
};
typedef struct yari_ctx_t yari_ctx_t;

int yari_connect(yari_ctx_t *ctx, char *ip, int port);
int yari_set(yari_ctx_t *ctx, char *key, int klen, char *val, int vlen);
int yari_get(yari_ctx_t *ctx, char *key, int klen, char *val, int *vlen);
int yari_close(yari_ctx_t *ctx);

#endif
