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
#ifndef _YHASH_H

#include <ycommon.h>
#include <ylock.h>

#define _YHASH_H

#define YHTAB_NCNT_DEFAULT (1024*1024)
#define YHTAB_SMAX_DEFAULT (1024)

struct yhdata_t
{
  int  len;
  char data[1];
};
typedef struct yhdata_t yhdata_t;

struct yhobj_t
{
  int             len;
  ylock_t         lock;
  uint64_t        hash;
  struct yhobj_t *next;
};
typedef struct yhobj_t yhobj_t;

struct yhslot_t
{
  ylock_t   lock;
  int       cnt;
  yhobj_t  *obj;
};
typedef struct yhslot_t yhslot_t;

struct yhtab_t
{
  ylock_t     lock;
  int         gcn;

  int         ncnt;                          /* number of slots in each sarr */
  int         nbit;

  int         scnt;                                /* current length of sarr */
  int         smax;                                    /* max length of sarr */

  yhslot_t   *sarr[1];
};
typedef struct yhtab_t yhtab_t;


#define yhobj_size(klen, dlen) \
          (sizeof(yhobj_t) + klen + dlen + 2 * sizeof(yhdata_t))

#define yhobj_key(obj) \
          (yhdata_t *)((char *)obj + sizeof(yhobj_t))
#define yhobj_val(obj) \
          (yhdata_t *)((char *)yhobj_key(obj) + \
               sizeof(yhdata_t)+(yhobj_key(obj))->len)

#define yhtab_lock(ht, mode)    ylock_acq(&(ht)->lock, mode)
#define yhtab_unlock(ht, mode)  ylock_rel(&(ht)->lock, mode)

#define yhobj_lock(ho, mode)    ylock_acq(&(ho)->lock, mode)
#define yhobj_unlock(ho, mode)  ylock_rel(&(ho)->lock, mode)

#define yhslot_lock(sl, mode)   ylock_acq(&ht->lock, mode)
#define yhslot_unlock(sl, mode) ylock_rel(&ht->lock, mode)

#define yhtab_sind(ht, hash) \
          (((hash) >> ((ht)->nbit)) % (ht)->scnt)
#define yhtab_slot(ht, sind, hash) \
          (&(((ht)->sarr[sind])[(hash) % (ht)->ncnt]))

#define hash_compute(key, len) (uint64_t)(XXH64((void *)key, len, 0))

extern yhtab_t *yhtab_global;

yhtab_t * yhtab_create(int ncnt, int smax);

int yhtab_get(yhobj_t **robj, yhtab_t *ht, char *key, int klen,
              ylock_mode_t lmode);

int yhtab_set(yhobj_t **robj, yhtab_t *ht, char *key, int klen, 
              char *val, int vlen, ylock_mode_t lmode);

#endif
