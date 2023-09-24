CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

CONTIKI=../../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# CONTIKI_WITH_IPV6 = 1
CONTIKI_WITH_RIME = 1
MAKE_WITH_ORCHESTRA ?= 0 # force Orchestra from command line
MAKE_WITH_SECURITY ?= 0 # force Security from command line
PROJECT_SOURCEFILES += slotframe_evaluation_scheduler.c

ifeq ($(MAKE_FOR_LILLE),1)
CFLAGS += -DTESTBED_LILLE=1
else ifeq ($(MAKE_FOR_TOULOUSE),1)
CFLAGS += -DTESTBED_TOULOUSE=1
PROJECT_SOURCEFILES += toulouse-nodes.c toulouse_linkaddr_mapping.c
CFLAGS += -DDWM1001_LINKADDR_FROM_FILE=1
endif

ifeq ($(MAKE_WITH_REDUCED_RANGE),1)
CFLAGS += -DPROJECT_WITH_REDUCED_RANGE=1
endif

ifeq ($(MAKE_WITH_UART_OUTPUT_RANGE),1)
CFLAGS += -DWITH_UART_OUTPUT_RANGE=1 # only output range in this case for now
endif

# this experiment only works in the lille testbed
NRF52_RTIMER_USE_HFCLK=1
PRINTF_OVER_UART=1

MODULES += core/net/mac/tsch

include $(CONTIKI)/Makefile.include
