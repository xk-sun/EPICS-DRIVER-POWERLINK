TOP=..

include $(TOP)/configure/CONFIG

# Allow for debugging
HOST_OPT = NO

# Want local functions non-static? Define DEBUG 
#CFLAGS += -DDEBUG

# library

LIBRARY = oplk

# test oplk-lib
oplkmn_DIR = /home/test/work/openPOWERLINK2.2.2/stack/lib/linux/x86_64
LIB_LIBS += oplkmn

oplkmn_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmn
LIB_LIBS += oplkmn

oplkmnapp-kernelintf_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmnapp-kernelintf
LIB_LIBS += oplkmnapp-kernelintf

oplkmnapp-userintf_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmnapp-userintf
LIB_LIBS += oplkmnapp-userintf

oplkmndrv-pcap_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmndrv-pcap
LIB_LIBS += oplkmndrv-pcap




#pthread_DIR = /usr/lib/x86_64-linux-gnu
#testS7plc_LIBS += pthread
#rt_DIR = /usr/lib/x86_64-linux-gnu
#testS7plc_LIBS += rt
#pcap_DIR = /usr/lib/x86_64-linux-gnu
#testS7plc_LIBS += pcap
LIB_SYS_LIBS += pthread
LIB_SYS_LIBS += rt
LIB_SYS_LIBS += pcap


LIB_SRCS += drvoplk.c
LIB_SRCS += devoplk.c

# test oplk
#LIB_SRCS += app.c
LIB_SRCS += event.c
LIB_SRCS += pcap-console.c
LIB_SRCS += system-linux.c
LIB_SRCS += getopt.c 
LIB_SRCS += console-linux.c
LIB_SRCS += printlog.c 


LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
# test oplk-lib
#oplkmn_DIR = /home/oplk-dev/work/openPOWERLINK-V2.2.2/stack/lib/linux/x86_64
#LIB_LIBS += oplkmn

#oplkmn_DIR = /home/oplk-dev/work/openPOWERLINK-V2.2.2/stack/build/linux/proj/linux/liboplkmn
#LIB_LIBS += oplkmn

#oplkmnapp-kernelintf_DIR = /home/oplk-dev/work/openPOWERLINK-V2.2.2/stack/build/linux/proj/linux/liboplkmnapp-kernelintf
#LIB_LIBS += oplkmnapp-kernelintf

#oplkmnapp-userintf_DIR = /home/oplk-dev/work/openPOWERLINK-V2.2.2/stack/build/linux/proj/linux/liboplkmnapp-userintf
#LIB_LIBS += oplkmnapp-userintf

#oplkmndrv-pcap_DIR = /home/oplk-dev/work/openPOWERLINK-V2.2.2/stack/build/linux/proj/linux/liboplkmndrv-pcap
#LIB_LIBS += oplkmndrv-pcap


#HTMLS += s7plc.html
INSTALL_DBDS += $(INSTALL_DBD)/oplk.dbd

include $(TOP)/configure/RULES











