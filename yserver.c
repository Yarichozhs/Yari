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

#include <ytrace.h>
#include <ynet.h>
#include <ynets.h>
#include <yhash.h>
#include <ythread.h>

#define NTHREAD  (1024)
#define LCTX_MAX (16)
#define WCTX_MAX (16)

ynet_ctx_t *glctx[LCTX_MAX];
ynet_ctx_t *gwctx[LCTX_MAX];
ythread_ctx_t gtctx[NTHREAD];

ynet_event_ctx_t  ynet_event_ctx;
ynet_waiter_ctx_t ynet_waiter_ctx;

void create_ds(void)
{
  int ret;

  if ((ret = ynet_event_create(&ynet_event_ctx, YNET_EVENT_CTX_MAX)) != 0)
  {
    ytrace_msg(YTRACE_ERROR, "waiter context creation failed [%d]\n", ret);
    exit(0);
  }

  if ((ret = ynet_waiter_create(&ynet_waiter_ctx, &ynet_event_ctx)) != 0)
  {
    ytrace_msg(YTRACE_ERROR, "waiter context creation failed [%d]\n", ret);
    exit(0);
  }

  if ((glctx[0] = ynet_lsnr_create(&ynet_waiter_ctx)) == NULL)
    exit(0);

  if ((yhtab_global = yhtab_create(YHTAB_NCNT_DEFAULT, 
                                   YHTAB_SMAX_DEFAULT)) == NULL)
    exit(0);
}

int main()
{
  int i;
  int n = 4;                                            /* number of threads */

  create_ds();

  for (i=0; i<n; i++)
  {
    ythread_create(&gtctx[i], i, &ynet_waiter_ctx);
  }

  for (i=0; i<n; i++)
  {
    ythread_join(&gtctx[i]);
  }

  return 0;
}
