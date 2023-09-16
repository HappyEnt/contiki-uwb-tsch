CONTIKI_PROJECT = node
all: $(CONTIKI_PROJECT)

CONTIKI=../../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

# CONTIKI_WITH_IPV6 = 1
CONTIKI_WITH_RIME = 1
APPS += drand rand-sched
MAKE_WITH_ORCHESTRA ?= 0 # force Orchestra from command line
MAKE_WITH_SECURITY ?= 0 # force Security from command line
PROJECT_SOURCEFILES += mtm_control.c


ifeq ($(MAKE_WITH_REDUCED_RANGE),1)
CFLAGS += -DPROJECT_WITH_REDUCED_RANGE=1
endif

ifeq ($(MAKE_WITH_UART_OUTPUT_RANGE),1)
CFLAGS += -DWITH_UART_OUTPUT_RANGE=1 # only output range in this case for now
endif

# LILLE CONFIG
ifeq ($(MAKE_FOR_LILLE),1)
NRF52_RTIMER_USE_HFCLK=1
PRINTF_OVER_UART=1
PROJECT_SOURCEFILES += lille-nodes.c
else ifeq ($(MAKE_FOR_TOULOUSE),1)
NRF52_RTIMER_USE_HFCLK=1
PRINTF_OVER_UART=1
PROJECT_SOURCEFILES += toulouse-nodes.c toulouse_linkaddr_mapping.c
CFLAGS += -DDWM1001_LINKADDR_FROM_FILE=1
else
NRF52_SOFTDEVICE = 132
NRF52_RTIMER_USE_HFCLK=1
ENABLE_SEGGER_RTT=1
PRINTF_OVER_RTT=1
NRF_LOG_OVER_RTT=1
PROJECT_SOURCEFILES += nodes.c
endif

MODULES += core/net/mac/tsch

ifeq ($(MAKE_WITH_ORCHESTRA),1)
APPS += orchestra
CFLAGS += -DWITH_ORCHESTRA=1
else
# add schedule.c to source files
# PROJECT_SOURCEFILES += schedule.c
endif



ifeq ($(MAKE_WITH_SECURITY),1)
CFLAGS += -DWITH_SECURITY=1
endif

include $(CONTIKI)/Makefile.include
