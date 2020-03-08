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
#include <yhash.h>

/**
 * Internal token object. 
 */
struct ytoken_t
{
  int   len;
  char *str;
};
typedef struct ytoken_t ytoken_t;

/**
 * send result in given network context.
 */
#define ycmd_send_res(ctx, val) \
        do \
	{ \
	  char c = val; \
	   write(ctx->sfd, &c, 1); \
	} \
	while (FALSE)

/**
 * send success
 */
#define ycmd_send_ok(ctx)  ycmd_send_res(ctx, 0)

/* 
 * send failure 
 */
#define ycmd_send_err(ctx) ycmd_send_res(ctx, -1)

#define CMD_PREFIX_1   '#'
#define CMD_PREFIX_2   ':'
#define CMD_SUFFIX_1   '~'

/**
 * Dump given token.
 */

static void ycmd_token_dump(int level, char *str, ytoken_t *tok)
{
  ytrace_msg(level,
           "ycmd_token_dump [%s] : %p : len = %d : str = %p [%.*s]\n",
            str, tok, tok->len, tok->str, tok->len, tok->str);
}

#define ycmd_token_dump(level, str, tok) \
        if (level <= ytrace_level) \
	  ycmd_token_dump_int(level, str, tok);

/**
 * Dump given buffer 
 */
#define ycmd_ybuf_dump(level, str, buf, full) \
        if (level <= ytrace_level) \
	  ycmd_ybuf_dump_int(level, str, buf, full);

void ycmd_ybuf_dump_int(int level, char *str, ybuf_t *buf, int full)
{
  int rem;
  char *bp;

  if (full)
  {
    bp  = buf->bp;
    rem = (buf->ep - buf->bp);
  }
  else 
  {
    bp = buf->sp;
    rem = ybuf_rem(buf);
  }

  ytrace_msg(level,
            "ycmd_ybuf_dump [%s] : %p : fre = %d : sp = %p : ep = %p: str = %p : [%.*s]\n",
            str, buf, buf->fre, buf->sp, buf->ep, buf->buf, rem, bp);
}

/**
 * Encode a string.
 */
static inline int ycmd_token_str_enc(char *dp, int dl, char *sp, int sl)
{
  return snprintf(dp, dl, "%c%c%d%c%c%c%.*s%c",
                  CMD_PREFIX_1, CMD_PREFIX_2, sl, CMD_SUFFIX_1, 
		  CMD_PREFIX_1, CMD_PREFIX_2, sl, sp, CMD_SUFFIX_1);
}

static inline int ycmd_encode_str_int(ybuf_t *buf, char *str, int len)
{
  int   ind;
  char *cp;

  if (buf->fre < (len + 3))
    return y_error(EINVAL);

  cp  = buf->ep;

  *cp = CMD_PREFIX_1; cp++;
  *cp = CMD_PREFIX_2; cp++;

  for (ind = 0; ind < len; ind++) 
    cp[ind] = str[ind];

  cp += ind;

  *cp = CMD_SUFFIX_1; cp++;

  buf->ep   = cp;
  buf->fre -= (len + 3);

  return 0;
}

/**
 * Encode integer
 */
static inline int ycmd_encode_int(ybuf_t *buf, int val)
{
  int  len;
  int  n = 16;
  char istr[n];

  for (len = 0; val && len < n ; len++)
  {
    istr[n - len - 1] = '0' + val % 10;
    val = val / 10;
  }

  if (len == n)
    return y_error(EINVAL);

  if (len == 0)
  {
    istr[n - 1] = '0';
    len++;
  }

  return ycmd_encode_str_int(buf, &istr[n - len], len);
}

/**
 * Encode string
 */
static inline int ycmd_encode_str(ybuf_t *buf, char *str, int len)
{
  int ret;

  if ((ret = ycmd_encode_int(buf, len)) != 0)
    return ret;

  return ycmd_encode_str_int(buf, str, len);
}

/**
 * Decode integer
 */
static inline int ycmd_decode_int(ybuf_t *buf, int *val)
{
  int    ind;
  char  *cp;
  int    nval = 0;
  int    rem = ybuf_rem(buf);

  *val = 0;

  if (rem <= 3)
    return y_error(EINVAL);
  
  cp = buf->sp;

  if (cp[0] != CMD_PREFIX_1 || cp[1] != CMD_PREFIX_2)
    return y_error(EINVAL);

  for (ind = 2; ind < rem - 1; ind++)
  {
    if (cp[ind] == CMD_SUFFIX_1)
      break;

    if (!(cp[ind] >= '0' && cp[ind] <= '9'))
      return y_error(EINVAL);
    
    nval = nval * 10 + (cp[ind] - '0');
  }

  if ((ind == 2) || (cp[ind] != CMD_SUFFIX_1))
    return y_error(EINVAL);

  ind++;

  *val = nval;

  buf->sp  += ind;

  return 0;
}

/**
 * Decode string
 */
static inline int ycmd_decode_str(ybuf_t *buf, ytoken_t *val)
{
  char  *cp;
  int    nval = 0;
  int    len;
  int    ret;
  int    rem;

  if ((ret = ycmd_decode_int(buf, &len)) != 0)
    return ret;

  rem = ybuf_rem(buf);

  if (rem <= 3)
    return y_error(EINVAL);   /* TODO : Need to retrieve the second buffer */
  
  cp = buf->sp;

  if (cp[0] != CMD_PREFIX_1 || cp[1] != CMD_PREFIX_2)
    return y_error(EINVAL);

  if (cp[len + 2] != CMD_SUFFIX_1)
    return y_error(EINVAL);

  val->str = buf->sp + 2;
  val->len = len;

  buf->sp  += (len + 3);

  return 0;
}

#define YCMD_CMD_CMP(str, len, cmd) \
        ((len == sizeof(cmd) - 1) && (strncasecmp(str, cmd, len) == 0))

ycmd_t ycmd_get_ycmd_id(ytoken_t *tok)
{
  switch (tok->str[0])
  {
    case 'G':
    case 'g':
    {
      if (YCMD_CMD_CMP(tok->str, tok->len, CMD_GET_STR))
        return CMD_GET;
      
      return CMD_GET;                             /* default for 'g' */
    }
    case 'S':
    case 's':
      if (YCMD_CMD_CMP(tok->str, tok->len, CMD_SET_STR))
        return CMD_SET;
      break;

    case 'Q':
    case 'q':
      return CMD_QUIT;
  }

  return CMD_UNKNOWN;
}

int ycmd_server_process_set(ynet_ctx_t *ctx, ybuf_t *buf)
{
  int     ret;
  ytoken_t key;
  ytoken_t val;
  yhobj_t *obj;
  ybuf_t   out;

  if ((ret = ycmd_decode_str(buf, &key)) != 0)
    return ret;

  if ((ret = ycmd_decode_str(buf, &val)) != 0)
    return ret;

  /* TODO handle big values */

  ret = yhtab_set(&obj, yhtab_global, key.str, key.len, val.str, val.len, YLOCK_EXCL);

  if (ret != 0)
    return ret;

  yhobj_unlock(obj, YLOCK_EXCL);

  /* TODO handle big values */

  ybuf_init(&out);

  ycmd_encode_int(&out, ret);

  ynet_send(ctx, &out);

  return ret;
}

int ycmd_server_process_get(ynet_ctx_t *ctx, ybuf_t *buf)
{
  int     ret;
  ytoken_t key;
  yhobj_t *obj;
  yhdata_t *val;
  ybuf_t   out;

  ytrace_msg(YTRACE_LEVEL1, "ycmd_server_process_get : enter \n");

  if ((ret = ycmd_decode_str(buf, &key)) != 0)
    return ret;

  obj = NULL;

  ret = yhtab_get(&obj, yhtab_global, key.str, key.len, YLOCK_SHARED);

  ybuf_init(&out);

  ycmd_encode_int(&out, ret);

  if (ret == 0)
  {
    /* TODO handle big values */
    val = yhobj_val(obj);

    ycmd_encode_str(&out, val->data, val->len);

    yhobj_unlock(obj, YLOCK_SHARED);
  }
  else if (obj)
    ytrace_msg(YTRACE_LEVEL1, "Something wrong in ycmd_server_process_get : %p\n", obj);

  ycmd_ybuf_dump(YTRACE_LEVEL1, "ycmd_server_process_get_send_buf", &out, FALSE);

  ynet_send(ctx, &out);

  return ret;
}


int ycmd_server_process(ynet_ctx_t *ctx)
{
  ybuf_t   buf;
  int     cmn;
  int     ret = y_error(EINVAL);
  ytoken_t cmd;

  ybuf_init(&buf);

  if ((ret = ynet_recv(ctx, &buf)) != 0)
    return ret;

  ytrace_msg(YTRACE_LEVEL1, "ycmd_server_process : enter\n");

  /* ycmd_ybuf_dump("ycmd_server_process_1", &buf, TRUE); */

  if ((ret = ycmd_decode_int(&buf, &cmn)) != 0)
    return ret;

  /* ycmd_token_dump("ycmd_server_process_ycmd_token", &cmd); */

  ytrace_msg(YTRACE_LEVEL1, "ycmd_server_process : cmd = %d \n", cmn);

  switch (cmn)
  {
    case CMD_GET:
    {
      ret = ycmd_server_process_get(ctx, &buf);
      break;
    }
    case CMD_SET:
    {
      ret = ycmd_server_process_set(ctx, &buf);
      break;
    }
    default:
    {
      ycmd_send_err(ctx);
    }
  }
}

#define ycmd_is_ws(c) ((c) == ' ' || (c) == '\n')

static inline void ycmd_token_skip_ws(ybuf_t *buf)
{
  int   ind;
  int   rem = ybuf_rem(buf);
  char *cp  = buf->sp;

  for (ind = 0; (ind < rem) && ycmd_is_ws(cp[ind]); ind++);

  buf->sp  += ind;
}

static inline int ycmd_token_get(ybuf_t *buf, ytoken_t *tok, int consume)
{
  int   ind;
  int   rem;
  char *cp;

  ycmd_token_skip_ws(buf);

  rem = ybuf_rem(buf);

  if (!rem)
    return -1;

  cp  = buf->sp;

  for (ind = 0; (ind < rem) && !ycmd_is_ws(cp[ind]); ind++);

  tok->len = ind;
  tok->str = buf->sp;

  if (consume)
    buf->sp  += ind;

  return 0;
}

int ycmd_client_process_set(ynet_ctx_t *sctx, char *key, int klen, char *val, int vlen, int expiry)
{
  ybuf_t sbuf;
  ybuf_t rbuf;
  int   ret;

  ybuf_init(&sbuf);
  ybuf_init(&rbuf);

  ytrace_msg(YTRACE_LEVEL1, "ycmd_client_process_set : key = [%.*s] val = [%.*s]\n", klen, key, vlen, val);

  if ((ret = ycmd_encode_int(&sbuf, CMD_SET)) != 0)
    return ret;

  if ((ret = ycmd_encode_str(&sbuf, key, klen)) != 0)
    return ret;

  if ((ret = ycmd_encode_str(&sbuf, val, vlen)) != 0)
    return ret;

  ycmd_ybuf_dump(YTRACE_LEVEL1, "ycmd_client_process_set", &sbuf, TRUE); 

  if ((ret = ynet_send(sctx, &sbuf)) != 0)
    return ret;

  if ((ret = ynet_recv(sctx, &rbuf)) != 0)
    return ret;

  return 0;
}

int ycmd_client_process_get(ynet_ctx_t *sctx, char *key, int klen, char *val, int *vlen)
{
  ybuf_t sbuf;
  ybuf_t rbuf;
  ybuf_t out;
  int   ret;
  int   cret;
  ytoken_t tval;

  ybuf_init(&sbuf);
  ybuf_init(&rbuf);

  ytrace_msg(YTRACE_LEVEL1, "ycmd_client_process_get : key = [%.*s]\n", klen, key);

  if ((ret = ycmd_encode_int(&sbuf, CMD_GET)) != 0)
    return ret;

  if ((ret = ycmd_encode_str(&sbuf, key, klen)) != 0)
    return ret;

  /* ycmd_ybuf_dump("ycmd_client_process_get", &buf, TRUE); */

  if ((ret = ynet_send(sctx, &sbuf)) != 0)
    return ret;

  ycmd_ybuf_dump(YTRACE_LEVEL1, "ycmd_client_process_get_send_buf", &sbuf, TRUE);  

  if ((ret = ynet_recv(sctx, &rbuf)) != 0)
    return ret;

  ycmd_ybuf_dump(YTRACE_LEVEL1, "ycmd_client_process_get_recv_buf", &rbuf, TRUE);  

  if ((ret = ycmd_decode_int(&rbuf, &cret)) != 0)
    return ret;

  if ((ret = ycmd_decode_str(&rbuf, &tval)) != 0)
    return ret;
 
  if (cret != 0)
    return cret;

  *vlen = tval.len;
  memcpy(val, tval.str, tval.len);

  return 0;
}

int ycmd_client_process(ynet_ctx_t *sctx, ybuf_t *buf)
{
  ycmd_t    cmn;
  ybuf_t    out;
  ytoken_t  cmd;
  ytoken_t  key;
  ytoken_t  val;
  int      len = 0;
  int      ret = y_error(EINVAL);

  if (ycmd_token_get(buf, &cmd, TRUE) < 0)
  {
    printf("ERROR : unknown command [%.*s]\n", (int)ybuf_rem(buf), buf->sp);
    return 0;
  }

  /* ycmd_ybuf_dump("ycmd_client_process_2", buf); */
  /* ycmd_token_dump("cmd", &cmd); */

  cmn = ycmd_get_ycmd_id(&cmd);

  if (cmn == CMD_QUIT)
    return y_error(EINVAL);

  ybuf_init(&out);

  switch (cmn)
  {
    case CMD_GET:
    {
      if (ycmd_token_get(buf, &key, TRUE) < 0)
        break;
      
      ret = ycmd_client_process_get(sctx, key.str, key.len, out.buf, &len);

      break;
    }
    case CMD_SET:
    {
      if (ycmd_token_get(buf, &key, TRUE) < 0)
        break;
      
      if (ycmd_token_get(buf, &val, TRUE) < 0)
        break;
      
      ret = ycmd_client_process_set(sctx, key.str, key.len, val.str, val.len, 0);

      break;
    }
  }

  if (ret == 0)
  {
    if (len)
      printf("%.*s\n", len, out.buf);
    else 
      printf("OK\n");
  }
  else 
  {
    printf("ERROR : in executing command [%d][%.*s]\n", cmn, cmd.len, cmd.str);
  }

  return 0;

#ifdef NOT_NOW

  ytrace_msg(YTRACE_LEVEL1, "ycmd_client_process : cmd = %d [%.*s] \n", cmd,  sbufl, sbuf);


  nl = ycmd_token_str_enc(dp, dl, tp, tl);

  //printf("token  cmd = [%.*s] %d\n", tl, tp, tl);
  dp += nl;
  dl -= nl;

  while (TRUE)
  {
    tl = ycmd_token_get(tp, MSG_MAX, &sp);

    if (!tl)
      break;

    //printf("token = [%.*s] %d\n", tl, tp, tl);

    nl = ycmd_token_str_enc(dp, dl, tp, tl);

    dp += nl;
    dl -= nl;
  }

  *dp = 0;

  dl = dp - &dbuf[0];

  ytrace_msg(YTRACE_LEVEL1, "DEBUG : dest msg [%.*s]\n", dl, dbuf);

  dl = ynet_send(sctx, dbuf, dl);

  if (dl < 0)
  {
    printf("ERROR : send message to server failed\n");
    return;
  }

  dl = ynet_recv(sctx, dbuf, MSG_MAX);

  if (dl < 0)
  {
    printf("ERROR : recevie message from server failed\n");
    return;
  }

  if (dl)
    printf("%*s\n", dl, dbuf);
#endif

  return 0;
}
