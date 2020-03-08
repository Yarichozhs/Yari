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

#include <limits.h>
#include <linux/futex.h>
#include <sys/time.h>

#include <ylock.h>
#include <ytrace.h>
#include <ythread.h>

/** 
 * Internal lock states. 
 */
#define YLOCK_STATE_SHARED   (1<<29)
#define YLOCK_STATE_EXCL     (1<<30)
#define YLOCK_STATE_WAIT     (1<<31)
#define YLOCK_STATE_MASK     ((1<<29) - 1)
#define YLOCK_STATE_VAL(val) ((val) & YLOCK_STATE_MASK)

/**
 * Compare and Swap 
 */
#define ylock_cas(addr, val, nval) \
         __sync_bool_compare_and_swap((addr), (val), (nval))

/**
 * Wait on memory location for a value change. Refer ylock.h for details
 */
void ylock_mem_wait(int *ptr, int val, size_t timeout)
{
  int ret;

  while (TRUE)
  {
    ret = syscall(SYS_futex, ptr, FUTEX_WAIT, val , NULL, NULL, 0);

    if (ret != 0)
    {
      if (errno == EINTR)
        continue;
    }
    
    break;
  }
}

/**
 * Post memory location for a value change. Refer ylock.h for details
 */
void ylock_mem_post(int *ptr)
{
  int ret;

  while (TRUE)
  {
    ret = syscall(SYS_futex, ptr, FUTEX_WAKE, INT_MAX , NULL, NULL, 0);

    if (ret != 0)
    {
      if (errno == EINTR)
        continue;
    }
    
    break;
  }
}

/**
 * Acquire lock internal. Do not use this directly, Refer ylock.h for details.
 * 
 * Try CAS with proper states. On failure, spin for a while and move to 
 * OS memory wait if required. 
 */
int ylock_acq_int(ylock_t *lock, ylock_mode_t mode, int wait, char *loc)
{
  int           lval;
  int           nval;
  int          *lptr = &lock->val;
  int           cont;
  volatile int  ind1 = 0;
  int           ind2 = 16;                       /* spin count to start with */

  ytrace_msg(YTRACE_LEVEL2, 
            "ylock_acq : %p : lval =  0x%x : mode = %d, wait = %d %s\n",
             lock, lval, mode, wait, loc);

  while (TRUE)
  {
    lval = *lptr;

    ytrace_msg(YTRACE_LEVEL2, 
              "ylock_acq (2): %p : lval =  0x%x : mode = %d, wait = %d %s\n",
               lock, lval, mode, wait, loc);

    if (mode == YLOCK_SHARED)
    {
      if (!(lval & YLOCK_STATE_EXCL))
      {
        nval  = YLOCK_STATE_VAL(lval);
        nval  = YLOCK_STATE_VAL(nval + 1);
        nval |= YLOCK_STATE_SHARED;
        cont  = TRUE;
      }
      else 
        cont  = FALSE;
    }
    else 
    {
      if (lval == 0)
      {
        nval = YLOCK_STATE_EXCL | ythread_self();
        cont  = TRUE;
      }
      else 
        cont = FALSE;
    }

    if (cont)
    {
      if (ylock_cas(lptr, lval, nval))
        break;

      continue;
    }

    if (!wait)
      return y_error(EBUSY);                         /* no-wait, retry error */

    if (ind2 < 2048)                                        /* keep spinning */
    {
      for (ind1 = 0; ind1 < ind2; ind1++);

      ind2 = ind2 << 2;
    }
    else                                             /* enter OS memory wait */
    {
      nval = lval;

      if (!(lval & YLOCK_STATE_WAIT))        /* set the state for post later */
      {
        nval |= YLOCK_STATE_WAIT;

        if (!ylock_cas(lptr, lval, nval))
          continue;
      }

      ylock_mem_wait(lptr, nval, 0);
    }
  }

  lock->owner = ythread_self();                       /* owning the lock now */
  lock->loc   = loc;

  return 0;
}

/**
 * Release lock internal. Do not use this directly, Refer ylock.h for details.
 * 
 * Release the lock and post the process(es) if required. 
 */
int ylock_rel_int(ylock_t *lock, ylock_mode_t mode, char *loc)
{
  int    lval;
  int    nval;
  int   *lptr = &lock->val;
  int    post = TRUE;
  int    checked = FALSE;

  while (TRUE)
  {
    lval = *lptr;

    ytrace_msg(YTRACE_LEVEL2,
              "ylock_rel : %p : lval =  0x%x : mode = %d, "
              "owner = %d : loc = %s\n",
               lock, lval, mode, lock->owner, lock->loc);

    if (mode == YLOCK_SHARED)
    {
      nval  = YLOCK_STATE_VAL(lval);

      if (!((nval > 0) && (lval & YLOCK_STATE_SHARED)))
      {
        ytrace_msg(YTRACE_ERROR,
                  "ylock_rel : 1 : wrong lock %p (%d) (acq = %s, rel = %s)\n",
                   lptr, lval, lock->loc, lock->rloc);

        return y_error(EBADF);
      }

      lock->rloc = loc;

      nval  = YLOCK_STATE_VAL(nval - 1);

      if (nval)
      {
        nval |= YLOCK_STATE_SHARED;

        if (lval & YLOCK_STATE_WAIT)
        {
          nval |= YLOCK_STATE_WAIT;
          post  = FALSE;
        }
      }
    }
    else 
    {
      if (!checked)
      {
        if (!((lock->owner == ythread_self()) && (lval & YLOCK_STATE_EXCL)))
        {
          ytrace_msg(YTRACE_ERROR, 
                    "ylock_rel : 2 : something wrong in lock %p (%d) : "
                    "(acq = %s, rel = %s)\n",
                    lptr, lval, lock->loc, lock->rloc);

          return y_error(EBADF);
        }

        lock->rloc  = loc;

        lock->owner = ~lock->owner;

        checked = TRUE;
      }

      nval = 0;
    }

    if (!ylock_cas(lptr, lval, nval))
      continue;

    if ((lval & YLOCK_STATE_WAIT) && post)
      ylock_mem_post(lptr);

    break;
  }

  return 0;
}

/**
 * Dump lock state.
 * 
 * Internal dump interface. 
 */
void ylock_dump(ylock_t *lock)
{
  int val = lock->val;

  ytrace_msg(YTRACE_DEFAULT,
            "ylock_dump : %p : owner (or last) = %d : val = %d : %s%s%s\n",
            lock, lock->owner, YLOCK_STATE_VAL(val), 
            (val & YLOCK_STATE_WAIT)   ? "[waiting]" : "",
            (val & YLOCK_STATE_SHARED) ? "[shared]"  : "",
            (val & YLOCK_STATE_EXCL)   ? "[excl]"    : "");
}

