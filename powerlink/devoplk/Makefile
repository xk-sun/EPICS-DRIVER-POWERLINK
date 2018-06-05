TOP=..

include $(TOP)/configure/CONFIG

# Allow for debugging
HOST_OPT = NO

# Want local functions non-static? Define DEBUG 
#CFLAGS += -DDEBUG

# library

LIBRARY = oplk
LIB_SRCS += drvoplk.c
LIB_SRCS += devoplk.c
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
#HTMLS += s7plc.html
INSTALL_DBDS += $(INSTALL_DBD)/oplk.dbd

include $(TOP)/configure/RULES

