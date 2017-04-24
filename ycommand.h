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
#ifndef _YCOMMAND_H

#define _YCOMMAND_H

#include <ynet.h>

int ycmd_server_process(ynet_ctx_t *ctx);
int ycmd_client_process(ynet_ctx_t *sctx, ybuf_t *buf);

int ycmd_client_process_set(ynet_ctx_t *sctx, char *key, int klen, char *val, int vlen, int expiry);
int ycmd_client_process_get(ynet_ctx_t *sctx, char *key, int klen, char *val, int *vlen);

#endif /* ycommand.h */
