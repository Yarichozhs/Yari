

CC=gcc
LD=gcc

CC_FLAG=-g -fPIC
#CC_FLAG=-O2 -fPIC

IPATH=.

YARI_SERVER=yari_server
YARI_CLIENT=yari_client
YARI_CLIENT_SO=libyari.so
KVBENCH=kvbench

#FENCE=/usr/lib64/libefence.so.0.0
FENCE=

LIBPATH=/usr/lib64
LIBS=-L$(LIBPATH) -lpthread $(FENCE)

all: $(YARI_SERVER) $(YARI_CLIENT) $(YARI_CLIENT_SO)

.PHONY: all

YARI_3RD_PARTY_OBJS=xxhash.o

YARI_SERVER_OBJS=yserver.o ynet.o ytrace.o ycommon.o ylock.o ycommand.o yhash.o ythread.o ynets.o $(YARI_3RD_PARTY_OBJS)
YARI_CLIENT_OBJS=yclient.o ynet.o ytrace.o ycommon.o ylock.o ycommand.o yhash.o $(YARI_3RD_PARTY_OBJS)
YARI_CLIENT_SO_OBJS=yarilib.o yclient.o ynet.o ytrace.o ycommon.o ylock.o ycommand.o yhash.o $(YARI_3RD_PARTY_OBJS)

$(YARI_SERVER): $(YARI_SERVER_OBJS)
	$(LD) -o $@ $^ $(LIBS)

$(YARI_CLIENT): $(YARI_CLIENT_OBJS)
	$(LD) -o $@ $^ $(LIBS)

$(YARI_CLIENT_SO): $(YARI_CLIENT_SO_OBJS)
	$(LD) -shared -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CC_FLAG) -c $< -I$(IPATH)

clean:
	rm -f $(YARI_SERVER_OBJS) $(YARI_CLIENT_SO_OBJS) $(YARI_CLIENT_OBJS)
