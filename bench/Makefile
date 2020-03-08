CC=gcc
LD=gcc
CP=cp
MKDIR=mkdir

CC_FLAG=-g -fPIC
#CC_FLAG=-O2 -fPIC

#FENCE=/usr/lib64/libefence.so.0.0
FENCE=

LIBPATH=/usr/lib64
LIBS=-L$(LIBPATH) -lpthread $(FENCE) -L../lib -Wl,-rpath-link=../lib

IPATH=-I../src -I./

KVBENCH=kvbench

YARI_CLIENT_SO=-lyari

all: $(KVBENCH)

.PHONY: all

KVBENCH_OBJS=kvbench.o redislib.o

$(KVBENCH): $(KVBENCH_OBJS)
	$(LD) -o $@ $^ $(LIBS) $(YARI_CLIENT_SO)

%.o: %.c
	$(CC) $(CC_FLAG) -c $< $(IPATH)

clean:
	rm -f $(KVBENCH_OBJS)
