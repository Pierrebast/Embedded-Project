CONTIKI_PROJECT = bulb gateway helper irrigation light subgateway
PROJECT_SOURCEFILES += helper.c
all: $(CONTIKI_PROJECT)

CONTIKI = ..

MAKE_NET = MAKE_NET_NULLNET
include $(CONTIKI)/Makefile.include
