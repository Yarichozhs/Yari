#include <stdio.h>
#define _GNU_SOURCE
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <redislib.h>
#include <yarilib.h>

#define NTHREAD (1024)

#define NOPN_DEFAULT (10000)

#define KEY_LEN_MAX (32)
#define VAL_LEN_MAX (512)

#define KEY_CNT_MAX (32)
#define VAL_CNT_MAX (32)

char *test_str[]=
{ 
  "redis",
  "yari"
};

#define TEST_REDIS (0)
#define TEST_YARI  (1)

struct thread_t
{
  int          ind;
  int          err;
  pthread_t    hdl;
  volatile int ready;
  volatile int done;
  volatile int end;
  size_t       time_diff;
};
typedef struct thread_t thread_t;

int       test_type;
int       nthread = 4;
thread_t  thr_ctx_arr[NTHREAD];

int nopn = NOPN_DEFAULT;

int klen = KEY_LEN_MAX;
int vlen = VAL_LEN_MAX;
int kcnt = KEY_CNT_MAX;
int vcnt = VAL_CNT_MAX;

__thread char key[KEY_CNT_MAX][KEY_LEN_MAX];
__thread char val[VAL_CNT_MAX][VAL_LEN_MAX];
__thread int  key_off;
__thread int  val_off;

int keyl[KEY_CNT_MAX];
int vall[VAL_CNT_MAX];

#define REDIS_SERVER_IP    "127.0.0.1"
#define REDIS_SERVER_PORT  6379

__thread redis_ctx_t redis_ctx;
__thread yari_ctx_t  yari_ctx;

char server_host[128];
int  server_port;

__thread void *test_ctx;
int (*test_get)(void *ctx, char *key, int klen, char *val, int *vlen);
int (*test_set)(void *ctx, char *key, int klen, char *val, int vlen);

static size_t get_cur_time()                    /* current time in microsecs */
{
  struct timeval s;
  gettimeofday(&s, NULL);
  return (s.tv_sec * 1000000 + s.tv_usec);
}

int process_init(void)
{
  int ind;

  for (ind = 0; ind < KEY_CNT_MAX; ind++)
  {
    keyl[ind] = ind % KEY_LEN_MAX + 1;
  }

  for (ind = 0; ind < VAL_CNT_MAX; ind++)
  {
    vall[ind] = ind % VAL_LEN_MAX + 1;
  }

  if (test_type == TEST_REDIS)
  {
    test_get = (int (*)(void *, char *, int, char *, int *))redis_get;
    test_set = (int (*)(void *, char *, int, char *, int))redis_set;
  }
  else if (test_type == TEST_YARI)
  {
    test_get = (int (*)(void *, char *, int, char *, int *))yari_get;
    test_set = (int (*)(void *, char *, int, char *, int))yari_set;
  }
}

void thread_init(int thr)
{
  int ind;
  int ind2;
  char str_seq[]="abcdefghijklnopqrstuvqxyz1234567890";
  int  str_len = sizeof(str_seq) - 1;

  for (ind = 0; ind < KEY_CNT_MAX; ind++)
  {
    snprintf(key[ind], keyl[ind] + 1, "key_%d_%d_%s", thr, ind, str_seq);

    /* printf("thread [%d] : key [%d] = [%.*s] %d\n", thr, ind,  keyl[ind],  key[ind], keyl[ind]); */
  }

  for (ind = 0; ind < VAL_CNT_MAX; ind++)
  {
    for (ind2 = 0; ind2 < vall[ind]; ind2++)
    {
      val[ind][ind2] = str_seq[(thr + ind * vall[ind] + ind2)%str_len];
    }

    //printf("thread [%d] : val [%d] = [%.*s] %d\n", thr, ind,  vall[ind],  val[ind], vall[ind]);
  }

  key_off = random() % KEY_CNT_MAX;
  val_off = random() % VAL_CNT_MAX;

  if (test_type == TEST_REDIS)
  {
    if (redis_connect(&redis_ctx, server_host[0] ? server_host : NULL, 0) < 0)
    {
      printf("thread [%d] : redis connect failed\n", thr);
      exit(0);
    }

    test_ctx = (void *)&redis_ctx;
  }
  else if (test_type == TEST_YARI)
  {
    if (yari_connect(&yari_ctx, server_host[0] ? server_host : NULL, 0) < 0)
    {
      printf("thread [%d] : yari connect failed\n", thr);
      exit(0);
    }

    test_ctx = (void *)&yari_ctx;
  }

  //printf("thread_init : %d : done\n", thr);
}

void test_get_driver(thread_t *tctx)
{
  int ind;
  char val_out[VAL_LEN_MAX];
  int kind;
  int vind;
  int ret;
  int vlen;

  for (ind = 0; ind < nopn; ind++)
  {
    kind = (ind + key_off) % KEY_CNT_MAX;

    vlen = sizeof(val_out);

   //   printf("[%d] test_get_driver : %d : kind = %d [%.*s] %d\n", tctx->ind, ind, kind, keyl[kind], key[kind], keyl[kind]);

    ret = (*test_get)(test_ctx, key[kind], keyl[kind], val_out, &vlen);

    if (ret < 0)
      tctx->err++;
  }
}

void test_set_driver(thread_t *tctx)
{
  int ind;
  char val_out[VAL_LEN_MAX];
  int kind;
  int vind;
  int ret;
  int vlen;

  for (ind = 0; ind < nopn; ind++)
  {
    kind = (ind + key_off) % KEY_CNT_MAX;

   //   printf("[%d] test_get_driver : %d : kind = %d [%.*s] %d\n", tctx->ind, ind, kind, keyl[kind], key[kind], keyl[kind]);

    ret = (*test_set)(test_ctx, key[kind], keyl[kind], val[kind], vall[kind]);

    if (ret < 0)
      tctx->err++;
  }
}

void (*test_driver)(thread_t *tctx);

volatile int test_ready;
volatile int test_start;

void * thread_driver(void *ctx)
{
  int       ind;
  thread_t *tctx = (thread_t *)ctx;
  size_t beg;
  size_t end;

  thread_init(tctx->ind);

  while (!tctx->end)
  {
    tctx->ready = 0;
    while(!test_ready)
    {
      sleep(1);

      if (tctx->end)
      {
        //printf("Thread %d done\n", tctx->ind);
        pthread_exit(NULL);
      }
    }
    tctx->done  = 0;
    tctx->ready = 1;
    while(!test_start);

    beg = get_cur_time();

    (*test_driver)(tctx);

    end = get_cur_time();

    tctx->time_diff = end - beg;

    tctx->done = 1;
  }

  return NULL;
}

void test_function(char *str, void (*func)(thread_t *))
{
  int ind1;
  thread_t *tctx;
  size_t time_tot = 0;
  test_driver = func;

  test_ready = 1;

  while(TRUE)
  {
    for (ind1 = 0; ind1 < nthread; ind1++)
    {
      tctx = &thr_ctx_arr[ind1];

	if (!tctx->ready)
	  break;
    }

    if (ind1 == nthread)
      break;
  }

  test_ready = 0;

  test_start = 1;

  while(TRUE)
  {
    for (ind1 = 0; ind1 < nthread; ind1++)
    {
      tctx = &thr_ctx_arr[ind1];

	if (!tctx->done)
	  break;
    }

    if (ind1 == nthread)
      break;
  }

  for (ind1 = 0; ind1 < nthread; ind1++)
  {
    time_tot += tctx->time_diff;
  }

  printf("%s : %s : time taken = %-16llu : avg/opn = %3.2f\n", test_str[test_type], str, time_tot, (double)time_tot/(nthread * nopn));
}

void parse_cmd_line(int argc, char *argv[])
{
  int opt;

  while ((opt = getopt_long(argc, argv,
            "a:b:c:d:D:f:i:ln:N:o:h:Op:s:S:t:wV?", NULL, NULL)) != -1)
  {
    switch (opt)
    {
      case 'N':
        nopn = atol(optarg);
        break;
      case 'n':
        nthread = atol(optarg);
        break;
      case 'h':
        strcpy(server_host, optarg);
        break;
      case 't':
        if (strcmp(optarg, "yari") == 0)
	  test_type = TEST_YARI;
        break;
    }
  }

  printf("# operations     = %d\n", nopn);
  printf("# client threads = %d\n", nthread);
  if (server_host[0])
    printf("# server host    = %s\n", server_host);
  printf("test type = %s\n", test_str[test_type]);
}

int main(int argc, char *argv[])
{
  int ind1;
  thread_t *tctx;

  parse_cmd_line(argc, argv);                          /* parse user options */

  process_init();

  for (ind1 = 0 ; ind1 < nthread; ind1++)
  {
    tctx = &thr_ctx_arr[ind1];
    tctx->ind = ind1;
    pthread_create(&tctx->hdl, NULL, thread_driver, (void *)tctx);
  }

  //printf("Test start\n");
  test_function("SET", test_set_driver);
  test_function("GET", test_get_driver);

  for (ind1 = 0 ; ind1 < nthread; ind1++)       /* join and accumulate stats */
  {
    tctx = &thr_ctx_arr[ind1];
    tctx->end = TRUE;
  }

  for (ind1 = 0 ; ind1 < nthread; ind1++)       /* join and accumulate stats */
  {
    tctx = &thr_ctx_arr[ind1];
    pthread_join(tctx->hdl, NULL);
  }

  return 0;
}
