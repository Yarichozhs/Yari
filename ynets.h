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
#ifndef _YNETS_H

#include <sys/epoll.h>

#define _YNETS_H

#include <ycommon.h>
#include <ynet.h>
#include <ylock.h>

/**
 * @file ynets.h - Network Server Related Interfaces
 * 
 */

/**
 * @section - Event Queue Management
 *            Tracks list of events in a circular fashion. Used for global
 *            event queue and per-connection queued events. 
 */

#define YNET_EVENT_CTX_MAX (1024)                /**< maximum event contexts */

/** 
 * @struct ynet_event_ctx_t
 *
 * @brief  Event context tracking the pending events in circular list.
 */
struct ynet_event_ctx_t
{
  ylock_t   lock;
  int       max;
  int       tail;
  int       head;
  struct epoll_event *events;
};
typedef struct ynet_event_ctx_t ynet_event_ctx_t;

/**
 * Macros tracking the circular list in event context.
 */
#define ynet_event_ctx_nused(ctx) \
          (((ctx)->head + (ctx)->max - (ctx)->tail) % (ctx)->max)
#define ynet_event_ctx_nfree(ctx) \
          ((ctx)->max - ynet_event_ctx_nused(ctx))
#define ynet_event_ctx_is_empty(ctx) ((ctx)->head == (ctx)->tail)
#define ynet_event_ctx_reset(ctx)    ((ctx)->head = (ctx)->tail = 0)

/** 
 * @struct ynet_waiter_ctx_t
 * 
 * @brief  Waiter context. It tracks one listening port and events 
 *         associated with that. 
 */
struct ynet_waiter_ctx_t
{
  ylock_t           lock;                                    /* lock object */
  size_t            gen;                                /* generation count */
  ynet_ctx_t       *nctx;          /* network context for this wait context */
  ythread_ctx_t    *tctx; /* current thread context waiting on this context */
  ynet_event_ctx_t *ectx;     /* event context to queue the incoming events */
  ylink_head_t      whead;                              /* thread wait head */
};
typedef struct ynet_waiter_ctx_t ynet_waiter_ctx_t;

/**
 * @struct ynet_conn_ctx_t
 * 
 * @brief  connection context, one per incoming connection. It is simply 
 *         based on the accepted socket descriptor. 
 */
struct ynet_conn_ctx_t
{
  ylock_t           lock;                                     /* lock object */
  int               init;                                     /* initialized */
  ystate_t          state;                                  /* current state */
  ynet_ctx_t       *nctx;             /* network context for this connection */
  ynet_event_ctx_t  ectx;         /* event context queue for this connection */
};
typedef struct ynet_conn_ctx_t ynet_conn_ctx_t;

/**
 * @brief Initialize event context. Create event objects based on maxevents.
 * 
 * @param ectx      - Context to initialize
 * @param maxevents - Number of event objects to create internally
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_event_create(ynet_event_ctx_t *ectx, int maxevents);

/**
 * @brief Initialize waiter context.
 * 
 * @param wctx  - Context to initialize
 * @param ectx  - Event context to be associated with this waiter context
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_waiter_create(ynet_waiter_ctx_t *wctx, ynet_event_ctx_t *ectx);

/**
 * @brief Allocate a listener context
 * 
 * @param wctx  - Waiter context to be associated with this lister
 * 
 * @return Valid context on success, NULL on failure with errno set. 
 */
ynet_ctx_t * ynet_lsnr_create(ynet_waiter_ctx_t *wctx);

/**
 * @brief Allocate a post context
 * 
 * @return Valid context on success, NULL on failure with errno set. 
 */
ynet_ctx_t * ynet_post_create(void);

/**
 * @brief Post a context
 * 
 * @param pctx  - Post context
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_post(ynet_ctx_t *pctx);

/**
 * @brief Wait on a Post context
 * 
 * @param pctx  - Post context
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_wait(ynet_ctx_t *pctx);

/**
 * @brief Make current thread to wait on internal context. Waits 
 *        till a event is retreived.
 * 
 * @param tctx  - Thread context
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_thread_wait(ythread_ctx_t *tctx);

/**
 * @brief Process events queued in current thread.
 * 
 * @param tctx  - Thread context
 * 
 * @return 0 on success, -1 on failure with errno set. 
 */
int ynet_thread_process(ythread_ctx_t *tctx);

#endif /* ynets.h */
