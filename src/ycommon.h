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
#ifndef _YCOMMON_H

#define _GNU_SOURCE      
#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <unistd.h>
#include <sys/syscall.h> 

#define _YCOMMON_H

#define TRUE  (1)
#define FALSE (0)

/* commands */
#define CMD_NONE       0
#define CMD_GET        1
#define CMD_SET        2
#define CMD_CLIENT   100
#define CMD_QUIT     101
#define CMD_UNKNOWN  999

typedef int ycmd_t;

#define CMD_GET_STR "GET"
#define CMD_SET_STR "SET"

/* Internal states */
enum ystate_t
{
  YSTATE_FREE = 0,
  YSTATE_RUNNING,
  YSTATE_WAITING
};
typedef enum ystate_t ystate_t;

#define MSG_MAX (4096)

#define y_error(x) ((errno = (x)), -1)

struct ybuf_t
{
  int    max;
  int    fre;
  char  *bp;
  char  *sp; /* start pointer */
  char  *ep;
  char   buf[MSG_MAX];
};
typedef struct ybuf_t ybuf_t;

#define ybuf_init(b) \
        do \
        { \
          (b)->max = MSG_MAX; \
          (b)->fre = MSG_MAX; \
          (b)->bp  = (b)->sp  = (b)->buf; \
          (b)->ep  = (b)->bp; \
        } \
        while (FALSE)

#define ybuf_rem(b) ((b)->ep - (b)->sp)

#define ybuf_mark(s, m) \
        do \
        { \
        }  \
        while (FALSE)



#define min(x, y) ( (x) < (y) ? (x) : (y) )

typedef uint64_t hash_t;
typedef struct ythread_ctx_t ythread_ctx_t;

struct ylink_t
{
  union 
  {
    size_t  val;
    void   *ptr;
  } data;

  struct ylink_t *next;
  struct ylink_t *prev;
};
typedef struct ylink_t ylink_t;

struct ylink_head_t
{
  size_t  count;
  ylink_t head;
};
typedef struct ylink_head_t ylink_head_t;

#define ylink_init(l) do { (l)->next = (l); (l)->prev = (l); } while (FALSE)
#define ylink_next(l)    (((l)->next != (l)) ? (l)->next : NULL)
#define ylink_prev(l)    (((l)->prev != (l)) ? (l)->prev : NULL)
#define ylink_is_null(l) (((l)->prev == (l)))

#define ylink_add_prev(l, n) \
        do \
        { \
          (n)->next = (l); (n)->prev = (l)->prev; \
          (l)->prev = (n); (n)->prev->next = (n); \
        } \
        while (FALSE)

#define ylink_add_next(l, n) \
        do \
        { \
          (n)->prev = (l); (n)->next = (l)->next; \
          (l)->next = (n); (n)->next->prev = (n); \
        } \
        while (FALSE)

#define ylink_rem(l) \
        do \
        { \
          (l)->prev->next = (l)->next; \
          (l)->next->prev = (l)->prev; \
          (l)->next = (l);  (l)->prev = (l); \
        } \
        while (FALSE)

#define ylink_head_init(h)  \
        do \
        { \
          (h)->count = 0;  \
          ylink_init(&(h)->head);  \
        } \
        while (FALSE)

#define ylink_head_is_null(h) ((h)->count)

#define ylink_head_add_first(h, l) \
        do \
        { \
          (h)->count++; \
          ylink_add_next(&(h)->head, l); \
        } \
        while (FALSE)

#define ylink_head_add_last(h, l) \
        do \
        { \
          (h)->count++; \
          ylink_add_prev(&(h)->head, l); \
        } \
        while (FALSE)

#define ylink_head_rem(h, l) \
        do \
        { \
          (h)->count--; \
          ylink_rem(l); \
        } \
        while (FALSE)

#define ylink_head_first(h) (ylink_next(&(h)->head))
#define ylink_head_next(h, l) \
          (((ylink_next(l) == &(h)->head)) ? NULL : ylink_next(l))

#define ylink_head_count(h) ((h)->count)

#define offsetof(s, m) (uint64_t)(&((s *)0)->m)

#define ylink_get_obj(l, s, m)  ((s *)((uint8_t *)l - offsetof(s, m)))

size_t ytime_get(void);

#endif /* common.h */
