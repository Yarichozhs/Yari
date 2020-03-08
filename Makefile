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

TOPTARGETS := all clean

SUBDIRS := src bench

$(TOPTARGETS): $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)

.PHONY: $(TOPTARGETS) $(SUBDIRS)
