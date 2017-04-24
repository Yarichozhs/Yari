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
#include <ythread.h>
#include <ytrace.h>

/**
 * Globals .
 */
__thread ythread_ctx_t *ythread_myctx;           /**< current thread context */

/**
 * @brief Thread main driver. 
 *        Process net events as long as possible. If none, enter wait. 
 *
 * @param arg - Thread argument, thread context for current thread
 * @return None 
 */
static void * ythread_driver(void *arg)
{
  int            ret;
  ythread_ctx_t *tctx = (ythread_ctx_t *)arg;

  ythread_myctx = tctx;
  ythread_myind = tctx->ind;

  ytrace_msg(YTRACE_DEFAULT, "ythread_driver : %p : started \n", tctx);

  while (TRUE)
  {
    if ((ret = ynet_thread_process(tctx)) < 0)
    {
      ytrace_msg(YTRACE_DEFAULT, "ythread_driver : process : exiting %d\n",
                 ret);
      break;
    }
    
    if ((ret = ynet_thread_wait(tctx)) < 0)
    {
      ytrace_msg(YTRACE_DEFAULT, "ythread_driver : wait : exiting %d\n",
                 ret);
      break;
    }
  }

  return NULL;
}

/**
 * Create thread. Refer ythread.h for details.
 */ 
int ythread_create(ythread_ctx_t *tctx, int ind, ynet_waiter_ctx_t *wctx)
{
  tctx->ind  = ind + 10001;
  tctx->wctx = wctx;

  ylink_init(&tctx->wlink);

  tctx->pctx = ynet_post_create();

  return pthread_create(&tctx->hdl, NULL, ythread_driver, (void *)tctx);
}

/**
 * Join thread. Refer ythread.h for details.
 */ 
int ythread_join(ythread_ctx_t *tctx)
{
  return pthread_join(tctx->hdl, NULL);
}
