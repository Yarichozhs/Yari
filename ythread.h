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
#ifndef _YTHREAD_H

#include <pthread.h>

#define _YTHREAD_H

#include <ycommon.h>
#include <ynet.h>
#include <ynets.h>

/**
 * @brief Thread context. 
 *        Every worker thread has a context to track it's details. 
 */
struct ythread_ctx_t
{
  pthread_t          hdl;                                 /**< thread handle */
  int                ind;                                  /**< thread index */
  ynet_ctx_t        *pctx;                              /**< network context */
  ylink_t            wlink;                    /**< wait context - wait link */
  ynet_waiter_ctx_t *wctx;                              /**< waiting context */
};

/**
 * @brief Current thread context 
 */
extern __thread ythread_ctx_t *ythread_myctx;

/**
 * @brief Current thread index 
 */
extern __thread int ythread_myind;

#define ythread_self_ctx()  (ythread_myctx)      /**< Current thread context */
#define ythread_self()      (ythread_myind)        /**< Current thread index */

/**
 * @brief Worker thread create
 *        Create worker threads on the fly to start processing incoming 
 *        messages. 
 *
 * @param tctx - Thread context for new thread
 * @param ind  - Thread index for new thread
 * @param wctx - Wait context in which the thread need to reap the requests. 
 *
 * @return 0 on success, -1 on failure with errno set. 
 */
int ythread_create(ythread_ctx_t *tctx, int ind, ynet_waiter_ctx_t *wctx);

/**
 * @brief Join the given thread. 
 *
 * @param tctx - Thread context to join. 
 *
 * @return 0 on success, -1 on failure with errno set. 
 */
int ythread_join(ythread_ctx_t *tctx);

#endif /* ythread.h */
