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
#include <fcntl.h>
#include <sys/eventfd.h>

#include <ytrace.h>
#include <ynet.h>
#include <ynets.h>
#include <ythread.h>
#include <ycommand.h>

typedef struct epoll_event epoll_event;

#define YNET_MAX_ENTRIES (4096)
#define YNEVENT (1024)

/**
 * ynet_ctx_arr   - Maximum network contexts
 * ynet_conn_ctx  - Maximum incoming connections
 * 
 * These are based on socket fds. Hence the ulimit max fds should be less
 * than this value. 
 */
static ynet_ctx_t       ynet_ctx_arr[YNET_MAX_ENTRIES];
static ynet_conn_ctx_t  ynet_conn_ctx[YNET_MAX_ENTRIES];
static int              ynet_conn_ctx_max = YNET_MAX_ENTRIES;

typedef struct ynet_lsnr_ctx_t ynet_lsnr_ctx_t;

static int ynet_waiter_wakeup(ynet_waiter_ctx_t *wctx, 
                              ythread_ctx_t *myctx, int n);

/**
 * Create event context. Refer ynets.h for details. 
 */
int ynet_event_create(ynet_event_ctx_t *ectx, int maxevents)
{
  ylock_init(&ectx->lock);
 
  ectx->events = (epoll_event *)malloc(maxevents * sizeof(epoll_event));

  if (!ectx->events)
    return y_error(ENOMEM);

  ectx->max = maxevents;

  ynet_event_ctx_reset(ectx);

  return 0;
}

/**
 * Create waiter context. Refer ynets.h for details. 
 */
int ynet_waiter_create(ynet_waiter_ctx_t *wctx, ynet_event_ctx_t *ectx)
{
  int         sfd;
  ynet_ctx_t *nctx;

  sfd = epoll_create(1024);

  if (sfd < 0)
  {
    ytrace_msg(YTRACE_ERROR, "epoll_create failed : %d\n", errno);
    return y_error(errno);
  }

  nctx = &ynet_ctx_arr[sfd];

  ynet_ctx_init(nctx, YNET_CLASS_WAIT, sfd);

  ylock_init(&wctx->lock);

  wctx->tctx = NULL;
  wctx->nctx = nctx;
  wctx->ectx = ectx;
  wctx->gen  = 0;

  ylink_head_init(&wctx->whead);

  ytrace_msg(YTRACE_DEFAULT, "wait channel created : fd = %d\n", nctx->sfd);
  ytrace_msg(YTRACE_LEVEL1,  "wait channel context : wctx = %p, nctx = %p\n",
            wctx, nctx);

  return 0;
}

/**
 * @brief  Queue 'n' events to global event context. Should be a quick
 *         operation, as other threads might wait on this event list to
 *         dequeue the same. 
 *
 * @return Number of events enqueued into the global enqueue list. 
 */
static int ynet_event_enqueue(ynet_event_ctx_t *ectx,
                              struct epoll_event *events, int n)
{
  int   enq = 0;
  int   ind1;
  int   ind2;

  ytrace_msg(YTRACE_LEVEL1,
            "ynet_event_enqueue : %p : n = %d : head = %d, tail = %d\n",
            ectx, n, ectx->head, ectx->tail);

  if (ynet_event_ctx_nfree(ectx) >= n)
  {
    for (ind1 = 0; ind1 < n; ind1++)
    {
      ind2 = ectx->tail;
      ectx->events[ind2] = events[ind1];
      ectx->tail = (ectx->tail + 1) % ectx->max;
    }

    ytrace_msg(YTRACE_LEVEL1,
              "ynet_event_enqueue : added %d events (head = %d, tail = %d)\n",
              n, ectx->head, ectx->tail);
    enq = n;
  }
  
  return enq;
}

static int ynet_wait_ctx_add(ynet_ctx_t *wctx, ynet_ctx_t *nctx)
{
  int flag;
  struct epoll_event event;

  if ((flag = fcntl(nctx->sfd, F_GETFL, 0)) < 0)
  {
    ytrace_msg(YTRACE_ERROR,
              "ynet_wait_ctx_add : fcntl get failed on fd = %d, errno = %d\n",
              nctx->sfd, errno);

    return y_error(errno);
  }

  if (fcntl(nctx->sfd, F_SETFL, flag|O_NONBLOCK) < 0)
  {
    ytrace_msg(YTRACE_ERROR,
              "ynet_wait_ctx_add : fcntl set failed on fd = %d, errno = %d\n",
              nctx->sfd, errno);

    return y_error(errno);
  }

  event.data.fd = nctx->sfd;
  event.events  = EPOLLIN | EPOLLET | EPOLLPRI | EPOLLRDHUP;

  if (epoll_ctl(wctx->sfd, EPOLL_CTL_ADD, nctx->sfd, &event) < 0)
  {
    ytrace_msg(YTRACE_ERROR, 
               "wait add failed wctx = %p(%d), nctx = %p(%d), err = %d\n",
               wctx, wctx->sfd, nctx, nctx->sfd, errno);
    return -1;
  }

  return 0;
}

static int ynet_wait_ctx_rem(ynet_ctx_t *wctx, ynet_ctx_t *nctx)
{
  epoll_ctl(wctx->sfd, EPOLL_CTL_DEL, nctx->sfd, NULL);

  return 0;
}

ynet_ctx_t * ynet_lsnr_create(ynet_waiter_ctx_t *wctx)
{
  int sfd;
  ynet_conn_ctx_t *conn;
  ynet_ctx_t *nctx;
  struct sockaddr_in serv_addr;

  sfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sfd < 0)
  {
    ytrace_msg(YTRACE_ERROR, "socket creation failed : %d\n", errno);
    return NULL;
  }

  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family      = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port        = htons(YNET_SER_PORT);

  if (bind(sfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
  {
    ytrace_msg(YTRACE_ERROR, "bind failed : %d\n", errno);
    close(sfd);
    return NULL;
  }

  listen(sfd, 5);

  nctx = &ynet_ctx_arr[sfd];
  conn = &ynet_conn_ctx[sfd];

  ylock_init(&conn->lock);

  conn->nctx  = nctx;
  conn->state = YSTATE_WAITING;

  ynet_ctx_init(conn->nctx, YNET_CLASS_LSNR, sfd);

  if (!conn->init)
  {
    ynet_event_create(&conn->ectx, YNET_EVENT_CTX_MAX);
    conn->init = 1;
  }

  if (ynet_wait_ctx_add(wctx->nctx, conn->nctx) < 0)
  {
    close(sfd);
    nctx->class = YNET_CLASS_NONE;
    conn->state = YSTATE_FREE;
    return NULL;
  }

  ytrace_msg(YTRACE_DEFAULT, "listen channel created : port = %d, fd = %d\n",
             YNET_SER_PORT, nctx->sfd);
  ytrace_msg(YTRACE_LEVEL1,
            "listen channel context : conn = %p, nctx = %p, wctx = %p\n",
             conn, nctx, wctx);

  return nctx;
}

int ynet_lsnr_destroy(ynet_lsnr_ctx_t *ctx)
{
  return 0;
}

ynet_ctx_t * ynet_post_create(void)
{
  int         efd;
  ynet_ctx_t *pctx; 

  efd = eventfd(0, 0);

  if (efd < 0)
  {
    ytrace_msg(YTRACE_ERROR,
              "ynet_post_create : eventfd create failed : errno = %d\n",
              errno);
    return NULL;
  }

  pctx = &ynet_ctx_arr[efd];

  ynet_ctx_init(pctx, YNET_CLASS_EVENT, efd);

  ytrace_msg(YTRACE_LEVEL1, "ynet_post_create : pctx = %p : efd = %d\n",
             pctx, efd);

  return pctx;
}

int ynet_wait(ynet_ctx_t *pctx)
{
  uint64_t val = 0;

  while (TRUE)
  {
    if (read(pctx->sfd, &val, sizeof(val)) < 0)
    {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      
      ytrace_msg(YTRACE_ERROR, "ynet_wait : read error : errno = %d\n", errno);
      return y_error(errno);
    }

    if (val)
      break;
  }

  return 0;
}

int ynet_post(ynet_ctx_t *pctx)
{
  uint64_t val = 1;

  while (TRUE)
  {
    if (write(pctx->sfd, &val, sizeof(val)) < 0)
    {
      if (errno == EAGAIN || errno == EINTR)
        continue;
      
      ytrace_msg(YTRACE_ERROR, "ynet_wait : write error : errno = %d\n",
                 errno);
      return y_error(errno);
    }

    break;
  }

  return 0;
}


static int ynet_lsnr_process(ynet_waiter_ctx_t *wctx, ynet_ctx_t *ctx)
{
  int              sfd;
  ynet_ctx_t      *nctx;
  ynet_conn_ctx_t *conn;
  struct sockaddr_in cli_addr;
  int              clilen;

  while (TRUE)
  {
    sfd = accept(ctx->sfd, (struct sockaddr *) &cli_addr, &clilen);

    if (sfd == -1)
    {
      if (errno == EINTR)
        continue;
      else if (errno == EWOULDBLOCK)
        break;
      else 
      {
        ytrace_msg(YTRACE_ERROR,
                  "accept returned error in fd = %d, errno = %d\n",
                   ctx->sfd, errno);
        return y_error(errno);
      }
    }

    ytrace_msg(YTRACE_DEFAULT, "new client connected (sfd = %d)\n", sfd);

    nctx = &ynet_ctx_arr[sfd];

    nctx->sfd   = sfd;
    nctx->class = YNET_CLASS_MSG;

    conn = &ynet_conn_ctx[sfd];

    ylock_init(&conn->lock);

    if (!conn->init)
    {
      ynet_event_create(&conn->ectx, YNET_EVENT_CTX_MAX);
      conn->init = 1;
    }

    conn->state = YSTATE_WAITING;
    conn->nctx  = nctx;

    ynet_wait_ctx_add(wctx->nctx, nctx);
  }

  return 0;
}

int ynet_waiter_wait_on_wctx(ynet_waiter_ctx_t *wctx)
{
  int         tmp;
  int         nevn;
  ynet_ctx_t *nctx;
  ynet_ctx_t *tctx;
  ynet_event_ctx_t *ectx;
  epoll_event events[YNEVENT];

  nctx = wctx->nctx;
  ectx = wctx->ectx;
  
  ytrace_msg(YTRACE_LEVEL1,
            "ynet_waiter_wait_on_wctx : epoll wait in fd = %d\n",
             nctx->sfd);

  do 
  {
    nevn = epoll_wait(nctx->sfd, events, YNEVENT, -1);

    if (nevn < 0)
    {
      if ((errno == EINTR) || (errno == EAGAIN))
        continue;

      ytrace_msg(YTRACE_LEVEL1, "epoll exited with error = %d\n", errno);
      return y_error(errno);
    }
    
    break;
  }
  while (TRUE);

  ytrace_msg(YTRACE_LEVEL1, "epoll returned = %d\n", nevn);

  while (TRUE)
  {
    ylock_acq(&ectx->lock, YLOCK_EXCL);

    tmp = ynet_event_enqueue(ectx, events, nevn);

    if (tmp == nevn)
    {
      ynet_waiter_wakeup(wctx, ythread_self_ctx(), nevn);
      ylock_rel(&ectx->lock, YLOCK_EXCL);
      break;
    }

    ylock_rel(&ectx->lock, YLOCK_EXCL);

    ytrace_msg(YTRACE_LEVEL1,
              "ynet_waiter_wait_on_wctx : enqueue is full, sleeping\n");

    sleep(3);                                  /* TODO : sleep on waiter gen */
  }

  return 0;
}

int ynet_waiter_idle(ythread_ctx_t *tctx)
{
  ytrace_msg(YTRACE_LEVEL1, "ynet_waiter_idle : enter wait\n");

  return ynet_wait(tctx->pctx);
}

/* ectx lock should be in exclusive mode */
int ynet_waiter_wakeup(ynet_waiter_ctx_t *wctx, ythread_ctx_t *myctx, int n)
{
  int cnt;
  int wcnt = n;
  ylink_t *link;
  ythread_ctx_t *tctx;

  cnt = ylink_head_count(&wctx->whead);

  ytrace_msg(YTRACE_LEVEL1, "ynet_waiter_wakeup : waking up %d threads\n", n);

  for (link = ylink_head_first(&wctx->whead);
       link && cnt && wcnt;
       cnt--, link = ylink_head_next(&wctx->whead, link))
  {
    tctx = ylink_get_obj(link, ythread_ctx_t, wlink);

    if (tctx == myctx)
      continue;

    ynet_post(tctx->pctx);

    ytrace_msg(YTRACE_LEVEL1,
              "ynet_waiter_wakeup : (2) : woken up %d\n", tctx->ind);

    wcnt--;
  }
}

ynet_conn_ctx_t * ynet_event_conn_dequeue(ynet_event_ctx_t *ectx)
{
  ynet_conn_ctx_t  *conn = NULL;
  ynet_conn_ctx_t  *tcon = NULL;
  ynet_event_ctx_t *tectx;
  epoll_event      *event;

  ylock_acq(&ectx->lock, YLOCK_EXCL);

  while (ectx->head != ectx->tail)
  {
    event = &ectx->events[ectx->head];

    ytrace_msg(YTRACE_LEVEL1, "dequeue event : %p (head = %d) %d\n",
               event, ectx->head, event->data.fd);

    do 
    {
      if (event->data.fd >= ynet_conn_ctx_max)                    /* discard */
        break;
      
      tcon  = &ynet_conn_ctx[event->data.fd];

      ylock_acq(&tcon->lock, YLOCK_EXCL);

      if (tcon->state == YSTATE_FREE) 
      {
        ytrace_msg(YTRACE_ERROR,
                  "ynet_event_conn_dequeue : event in freed fd %d\n",
                   event->data.fd);
        ylock_rel(&tcon->lock, YLOCK_EXCL);
        break;
      }

      tectx = &tcon->ectx;

      if (!ynet_event_ctx_nfree(tectx))
      {
        ytrace_msg(YTRACE_ERROR,
                  "ynet_event_conn_dequeue : discarding event fd %d "
                  "as it is full\n",
                   event->data.fd);
        ylock_rel(&tcon->lock, YLOCK_EXCL);
        break;
      }

      tectx->events[tectx->tail] = *event;
      tectx->tail = (tectx->tail + 1) % tectx->max;

      if (tcon->state == YSTATE_WAITING)
        conn = tcon;                 /* leave the connection locked */
      else 
      {
        ytrace_msg(YTRACE_LEVEL1, 
                  "ynet_event_conn_dequeue : task not in wait state %d\n",
                   tcon->state);
        ylock_rel(&tcon->lock, YLOCK_EXCL);
      }
    }
    while (FALSE);

    ectx->head = (ectx->head + 1) % (ectx->max);  /* dequeued */

    if (conn)
      break;
  }

  ylock_rel(&ectx->lock, YLOCK_EXCL);

  return conn;
}

/* 
 * Function :- 
 * 
 *   connection context conn should have been locked in exclusive mode. 
 * 
 */
int ynet_conn_process(ynet_waiter_ctx_t *wctx, ynet_conn_ctx_t *conn)
{
  ynet_ctx_t       *nctx;
  epoll_event      *event;
  ynet_event_ctx_t *ectx;
  int               ret = 0;

  if (conn->state != YSTATE_WAITING)
  {
    /* TODO : panic */
    ytrace_msg(YTRACE_ERROR, "Wrong connection state : %p : state = %d\n",
               conn, conn->state);
    exit(-1);
  }

  ectx = &conn->ectx;
  nctx = conn->nctx;
  conn->state = YSTATE_RUNNING;

  while (TRUE)
  {
    if (ynet_event_ctx_is_empty(ectx))
      break;

    event = &ectx->events[ectx->head];

    ytrace_msg(YTRACE_LEVEL1,
              "ynet_process : %p : fd = %d : evt = 0x%x : class = %d \n",
               nctx, nctx->sfd, event->events, nctx->class);

    if (event->events & (EPOLLHUP|EPOLLERR|EPOLLRDHUP))
    {
      ytrace_msg(YTRACE_LEVEL1, "ynet_process : hup, closing conn %p\n", conn);

      ynet_event_ctx_reset(ectx);

      ynet_wait_ctx_rem(wctx->nctx, nctx);

      ynet_close(nctx);

      nctx->sfd = 0;
      nctx->class = YNET_CLASS_NONE;

      conn->state = YSTATE_FREE;

      ylock_rel(&conn->lock, YLOCK_EXCL);

      return 0;
    }

    switch (nctx->class)
    {
      case YNET_CLASS_WAIT:
        printf("TODO\n");
        break;
      case YNET_CLASS_LSNR:
        ret = ynet_lsnr_process(wctx, nctx);
        break;
      case YNET_CLASS_MSG:
        ret = ycmd_server_process(nctx);
        break;
      default:
        printf("TODO : %d\n", nctx->class);
    }

    ectx->head = (ectx->head + 1) % (ectx->max);  /* dequeued */
  }

  conn->state = YSTATE_WAITING;

  ylock_rel(&conn->lock, YLOCK_EXCL);

  return ret;
}

int ynet_thread_wait(ythread_ctx_t *tctx)
{
  int wait_on_ctx = FALSE;
  ynet_waiter_ctx_t *wctx = tctx->wctx;

  ylock_acq(&wctx->lock, YLOCK_EXCL);

  ylink_head_add_last(&wctx->whead, &tctx->wlink);

  ytrace_msg(YTRACE_LEVEL2, "ynet_thread_wait : wait queue length %d\n",
             ylink_head_count(&wctx->whead));
  
  if (wctx->tctx == NULL)
  {
    wctx->tctx  = tctx;
    wait_on_ctx = TRUE;
    ytrace_msg(YTRACE_LEVEL1, "ynet_thread_wait : taking over wctx\n");
  }
  
  ylock_rel(&wctx->lock, YLOCK_EXCL);

  if (wait_on_ctx)
    ynet_waiter_wait_on_wctx(wctx);
  else 
    ynet_waiter_idle(tctx);

  ylock_acq(&wctx->lock, YLOCK_EXCL);
  
  ylink_head_rem(&wctx->whead, &tctx->wlink);

  /* Following should be under wait_on_ctx. Just in case */
  if (wctx->tctx == tctx)
  {
    wctx->tctx  = NULL;
  }
  
  ylock_rel(&wctx->lock, YLOCK_EXCL);

  return 0;
}

int ynet_thread_process(ythread_ctx_t *tctx)
{
  ynet_waiter_ctx_t *wctx = tctx->wctx;
  ynet_conn_ctx_t *conn;

  ytrace_msg(YTRACE_LEVEL1, "ynet_thread_process : enter\n");

  while (conn = ynet_event_conn_dequeue(wctx->ectx))
  {
    ytrace_msg(YTRACE_LEVEL1, "ynet_thread_process : processing conn = %p\n",
               conn);
    ynet_conn_process(wctx, conn);   
  }

  return 0;
}
