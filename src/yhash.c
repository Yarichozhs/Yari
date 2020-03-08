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
#include <yhash.h>
#include <xxhash.h>
#include <ytrace.h>

#ifdef TEST_HASH
#define ytrace_msg printf
#endif

static yhobj_t  *yhobj_freed;                          /**< free object list */
yhtab_t         *yhtab_global;                /**< global hash table pointer */

/**
 * Create a heap object for given key and data 
 */
static yhobj_t * yhobj_create(uint64_t hash, char *key, int klen,
                              char *data, int dlen)
{
  int      len;
  yhobj_t *obj;
  yhdata_t *kobj;
  yhdata_t *vobj;
  
  len = yhobj_size(klen, dlen);
  obj = (yhobj_t *)malloc(len);

  obj->len  = len;
  obj->hash = hash;
  obj->next = NULL;

  ylock_init(&obj->lock);

  kobj = yhobj_key(obj);

  kobj->len = klen;
  memcpy(kobj->data, key, klen);

  vobj = yhobj_val(obj);
  vobj->len = dlen;
  memcpy(vobj->data, data, dlen);
  
  return obj;
}

/**
 * Dump given heap object
 */
void yhobj_dump(char *prefix, yhobj_t *obj)
{
  yhdata_t *kobj;
  yhdata_t *vobj;

  kobj = yhobj_key(obj);
  vobj = yhobj_val(obj);

  ytrace_msg(YTRACE_LEVEL1, "%syhobj %p [%.*s](%d) -> [%.*s](%d) :",
             prefix, obj,
             kobj->len, kobj->data, kobj->len, 
             vobj->len, vobj->data, vobj->len);

  ytrace_msg(YTRACE_LEVEL1,
            "len = %d : lock = 0x%x : hash = 0x%lx : next = %p\n",
             obj->len, obj->lock, obj->hash, obj->next);
}

/**
 * Create heap table
 */
yhtab_t * yhtab_create(int ncnt, int smax)
{
  yhtab_t *ht;
  int      tcnt;

  ht = (yhtab_t *)malloc(sizeof(yhtab_t) + smax * sizeof(yhslot_t *));

  ylock_init(&ht->lock);

  ht->smax = smax;
  ht->ncnt = ncnt;
  ht->nbit = 0;

  for (tcnt = ncnt; tcnt > 1; tcnt = tcnt >> 1) 
    ht->nbit++;

  ht->scnt = 1;

  ht->sarr[0] = (yhslot_t *)malloc(sizeof(yhslot_t) * ncnt);

  memset(ht->sarr[0], 0, sizeof(yhslot_t) * ncnt);

  ytrace_msg(YTRACE_LEVEL1, "yhtab_create : ht = %p : arr[0] = %p\n",
             ht, ht->sarr[0]);

  return ht;
}

void yhtab_dump(yhtab_t *ht)
{
  int       ind;
  int       ind2;
  yhslot_t *slot;
  yhobj_t  *obj;
  char     *prefix = " ";

  ytrace_msg(YTRACE_LEVEL1, "\nDumping hash table %p\n", ht);
  
  ytrace_msg(YTRACE_LEVEL1, "%sncnt = %d, nbit = %d : scnt = %d, smax = %d\n",
            prefix, ht->ncnt, ht->nbit, ht->scnt, ht->smax);
 
  for (ind = 0; ind < ht->scnt; ind++)
  {
    for (ind2 = 0; ind2 < ht->ncnt; ind2++)
    {
      slot = &ht->sarr[ind][ind2]; 

      if (slot->obj == NULL)
        continue;

      ytrace_msg(YTRACE_LEVEL1,
                 "%sdumping slot index (%d, %d) : cnt = %d : lock = %d : "
                 "first obj = %p\n", prefix, ind, ind2, slot->cnt, 
                 slot->lock, slot->obj);

      for (obj = slot->obj; obj; obj = obj->next)
      {
        yhobj_dump(prefix, obj);
      }
    }
  }

  ytrace_msg(YTRACE_LEVEL1, "Dumping hash table done\n");
}

static yhobj_t *yhtab_scan_slot(yhslot_t *slot, hash_t hash,
                                char *key, int klen, ylock_mode_t lmode)
{
  yhobj_t  *obj;
  yhdata_t *kobj;
  
  for (obj = slot->obj; obj; obj = obj->next)
  {
    if (obj->hash == hash)
    {
      kobj = yhobj_key(obj);

      if ((kobj->len == klen) && (memcmp(key, kobj->data, klen) == 0))
      {
        yhobj_lock(obj, lmode);
        break;
      }
    }
  }

  return obj;
}

static int yhtab_slot_delete(yhslot_t *slot, yhobj_t *dobj)
{
  yhobj_t *obj;

  if (slot->obj == dobj)
  {
    slot->obj = dobj->next;
  
    /* TODO : take lock on yhobj_freed */
    dobj->next = yhobj_freed;
    yhobj_freed  = dobj;

    return 0;
  }
  
  for (obj = slot->obj; obj; obj = obj->next)
  {
    if (obj->next == dobj)
    {
      obj->next = dobj->next;
      dobj->next = yhobj_freed;
      yhobj_freed = dobj;
      return 0;
    }
  }

  return -1;
}

yhobj_t *yhtab_lookup(yhtab_t *ht, char *key, int klen, ylock_mode_t lmode)
{
  uint64_t hash;
  yhslot_t *slot;
  int      sind;
  yhobj_t  *obj;
  
  hash = hash_compute(key, klen);

  sind = yhtab_sind(ht, hash);

  do 
  {
    slot = yhtab_slot(ht, sind, hash);

    obj  = yhtab_scan_slot(slot, hash, key, klen, lmode);

    if (obj) 
      break;
    
    /* TODO : continue in case of expansion */

    break;
  }
  while (TRUE);

  return obj;
}

int yhtab_get(yhobj_t **robj, yhtab_t *ht, char *key, int klen,
              ylock_mode_t lmode)
{
  int    gcn;
  int    sind;
  int    rlen;
  yhslot_t *slot;
  yhobj_t *obj = NULL;
  yhdata_t *vobj;
  hash_t hash;

  hash = hash_compute(key, klen);

  ytrace_msg(YTRACE_LEVEL1, "\nyhtab_get : key = [%.*s] %d (hash = 0x%x)\n", 
             klen, key, klen, hash);

yhtab_get_retry_1:

  gcn  = ht->gcn;

  sind = yhtab_sind(ht, hash);
  slot = yhtab_slot(ht, sind, hash);

  yhslot_lock(slot, YLOCK_SHARED);

  if (ht->gcn != gcn)
  {
    yhslot_unlock(slot, lmode);
    goto yhtab_get_retry_1;
  }

  obj = yhtab_scan_slot(slot, hash, key, klen, lmode);

  ytrace_msg(YTRACE_LEVEL1, "yhtab_get : sind = %d : slot %p : obj = %p\n", 
             sind, slot, obj);

  yhslot_unlock(slot, lmode);

  *robj = obj;

  return (obj) ? 0 : y_error(EINVAL);
}

int yhtab_set(yhobj_t **robj, yhtab_t *ht, char *key, int klen,
              char *val, int vlen, ylock_mode_t lmode)
{
  int    gcn;
  int    sind;
  int    rlen;
  yhslot_t *slot;
  yhobj_t *obj = NULL;
  yhdata_t *vobj;
  hash_t hash;

  hash = hash_compute(key, klen);

  ytrace_msg(YTRACE_LEVEL1, 
             "\nyhtab_set : key = [%.*s] %d [%.*s] %d (hash = 0x%x)\n", 
              klen, key, klen, vlen, val, vlen, hash);

yhtab_set_retry_1:

  gcn  = ht->gcn;

  sind = yhtab_sind(ht, hash);
  slot = yhtab_slot(ht, sind, hash);

  yhslot_lock(slot, YLOCK_SHARED);

  if (ht->gcn != gcn)
  {
    yhslot_unlock(slot, YLOCK_SHARED);
    goto yhtab_set_retry_1;
  }

  if (obj = yhtab_scan_slot(slot, hash, key, klen, lmode))
  {
    vobj = yhobj_val(obj);

    rlen = obj->len - ((char *)vobj - (char *)obj);

    ytrace_msg(YTRACE_LEVEL1, "yhtab_set : remaining length = %d\n", rlen);

    if (rlen >= vlen)
    {
      vobj->len = vlen;
      memcpy((void *)vobj->data, val, vlen);
    }
    else 
    {
      ytrace_msg(YTRACE_LEVEL1,
                "yhtab_set : deleting exiting object = %p\n", obj);
      yhtab_slot_delete(slot, obj);
      obj = NULL;
    }
  }

  if (obj == NULL)
  {
    obj = yhobj_create(hash, key, klen, val, vlen);

    obj->next = slot->obj;
    slot->obj = obj;
    yhobj_lock(obj, lmode);
  }

  ytrace_msg(YTRACE_LEVEL1, "yhtab_set : sind = %d : slot %p : obj = %p\n",
             sind, slot, obj);

  yhslot_unlock(slot, YLOCK_SHARED);

  *robj = obj;

  return (obj) ? 0 : y_error(EINVAL);
}

int yhtab_resize(yhtab_t *ht, int ncnt)
{
#ifdef NOT_NOW
  int     ocnt;
  int     tcnt;
  int     ind;
  yhobj_t *nslot;
  yhobj_t *oslot;

  ocnt = ht->cnt;

  if (ocnt == ncnt)
    return;

  if (ht->oslot)
  {
    /* TODO : Need to check if all the threads are way past this old array */
    return;
  }

  nslot = (yhobj_t *)malloc(sizeof(yhslot_t) * ncnt);

  tcnt  = min(ncnt, ocnt);

  if (tcnt)
    memcpy(narr, ht->arr, tcnt * sizeof(yhobj_t *));

  if (ncnt > ocnt)
  {
    oarr = ht->arr;
    ht->arr = narr;

    for (ind = 0; ind < ocnt; ind++)
    {
      /* TODO : resize */
    }

    ht->oarr  = oarr;
  }
  else 
  {
    /* shrink */
  }
#endif
  return 0;
}

#ifdef TEST_HASH
int main()
{
  yhtab_t *ht;
  yhobj_t *obj;
  char *key = "test1";
  char *key2 = "test2";
  char *val = "value1";
  char *val2 = "xyz";
  char *val3 = "adsfasdfasdfasdfasdfasdfasdf";

  yhtab_resize(&yhtab, 1024*1024);

  obj = yhobj_create(0x1234, key, strlen(key), val, strlen(val));

  yhobj_dump(" ", obj);
  
  ht = yhtab_create(YHTAB_NCNT_DEFAULT, YHTAB_SMAX_DEFAULT);

  ytrace_msg(YTRACE_LEVEL1, "created hd = %p\n", ht);

  yhtab_set(ht, key, strlen(key), val, strlen(val), YLOCK_NONE);
  yhtab_dump(ht);

  yhtab_set(ht, key, strlen(key), val, strlen(val), YLOCK_NONE);
  yhtab_dump(ht);

  yhtab_set(ht, key, strlen(key), val2, strlen(val2), YLOCK_NONE);
  yhtab_dump(ht);

  yhtab_set(ht, key, strlen(key), val3, strlen(val3), YLOCK_NONE);
  yhtab_dump(ht);

  yhtab_set(ht, key, strlen(key), val, strlen(val), YLOCK_NONE);
  yhtab_dump(ht);

  yhtab_set(ht, key2, strlen(key2), val, strlen(val), YLOCK_NONE);
  yhtab_dump(ht);

  return 0;
}

#endif
