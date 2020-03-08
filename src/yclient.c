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
#include <ycommon.h>

#include <ycommand.h>
#include <ytrace.h>
#include <ynet.h>

ynet_ctx_t sctx;

int main()
{
  ybuf_t  buf;
  int    len;

  ynet_connect(&sctx, NULL);  /* TODO : need to send server address */

  while (TRUE)
  {
    printf("yari > ");
    
    ybuf_init(&buf);

    if (fgets(buf.buf, sizeof(buf.buf), stdin) == NULL)
      break;

    len     = strlen(buf.buf);
    buf.ep  = buf.sp + len - 1;
    buf.fre = (len - 1);

    if (ycmd_client_process(&sctx, &buf) != 0)
      break;
  }

  printf("Disconnected from Yari\n");

  return 0;
}
