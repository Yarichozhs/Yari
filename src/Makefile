
CC=gcc
LD=gcc
CP=cp
MKDIR=mkdir

CC_FLAG=-g -fPIC
#CC_FLAG=-O2 -fPIC

#FENCE=/usr/lib64/libefence.so.0.0
FENCE=

LIBPATH=/usr/lib64
LIBS=-L$(LIBPATH) -lpthread $(FENCE)


IPATH=.

BINDIR=../bin/
LIBDIR=../lib/

YARI_SERVER=yari_server
YARI_CLIENT=yari_client
YARI_CLIENT_SO=libyari.so

all: $(YARI_SERVER) $(YARI_CLIENT) $(YARI_CLIENT_SO)
	$(MKDIR) -p $(LIBDIR)
	$(MKDIR) -p $(BINDIR)
	$(CP) $(YARI_SERVER) $(BINDIR)
	$(CP) $(YARI_CLIENT) $(BINDIR)
	$(CP) $(YARI_CLIENT_SO) $(LIBDIR)

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
