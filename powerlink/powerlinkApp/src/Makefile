TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE


#LIBRARY_IOC += powerlinkSupport

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# Install devXxxSoft.dbd into <top>/dbd
#DBD += xxxSupport.dbd

# Compile and add the code to the support library
#powerlinkSupport_SRCS += xxxRecord.c
#powerlinkSupport_SRCS += devXxxSoft.c

# Link locally-provided code into the support library,
# rather than directly into the IOC application.
#powerlinkSupport_SRCS += dbSubExample.c
#powerlinkSupport_SRCS += powerlinkHello.c
#powerlinkSupport_SRCS += initTrace.c

#powerlinkSupport_LIBS += $(EPICS_BASE_IOC_LIBS)


#=============================
# Build the IOC application

PROD_IOC = powerlink
# powerlink.dbd will be created and installed
DBD += powerlink.dbd

# powerlink.dbd will be made up from these files:
powerlink_DBD += base.dbd
#powerlink_DBD += xxxSupport.dbd
#powerlink_DBD += dbSubExample.dbd
#powerlink_DBD += powerlinkHello.dbd
#powerlink_DBD += initTrace.dbd

# powerlink_registerRecordDeviceDriver.cpp derives from powerlink.dbd
powerlink_SRCS += powerlink_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
powerlink_SRCS_DEFAULT += powerlinkMain.cpp
powerlink_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#powerlink_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Link in the code from the support library
#powerlink_LIBS += powerlinkSupport

# NOTE: To build SNL programs, SNCSEQ must be defined
# in the <top>/configure/RELEASE file

#ifneq ($(SNCSEQ),)
    # Build sncExample into powerlinkSupport
   # sncExample_SNCFLAGS += +r
  # powerlink_DBD += sncExample.dbd
  #  powerlinkSupport_SRCS += sncExample.stt
  #  powerlinkSupport_LIBS += seq pv
  #  powerlink_LIBS += seq pv

    # Build sncProgram as a standalone program
  #  PROD_HOST += sncProgram
  #  sncProgram_SNCFLAGS += +m
  #  sncProgram_SRCS += sncProgram.st
  #  sncProgram_LIBS += seq pv
  #  sncProgram_LIBS += $(EPICS_BASE_HOST_LIBS)
#endif

ifneq ($(oplk),)
    powerlink_DBD += oplk.dbd

    #oplkmn_DIR = /home/test/work/openPOWERLINK2.2.2/stack/lib/linux/x86_64
    #powerlink_LIBS += oplkmn

    #oplkmn_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmn
    #powerlink_LIBS += oplkmn

    #oplkmnapp-kernelintf_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmnapp-kernelintf
    #powerlink_LIBS += oplkmnapp-kernelintf

    #oplkmnapp-userintf_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmnapp-userintf
    #powerlink_LIBS += oplkmnapp-userintf

    #oplkmndrv-pcap_DIR = /home/test/work/openPOWERLINK2.2.2/stack/build/linux/proj/linux/liboplkmndrv-pcap
    #powerlink_LIBS += oplkmndrv-pcap

    powerlink_LIBS += oplk
endif

#powerlink_SYS_LIBS += pthread
#powerlink_SYS_LIBS += rt
#powerlink_SYS_LIBS += pcap


# Finally link to the EPICS Base libraries
powerlink_LIBS += $(EPICS_BASE_IOC_LIBS)

#=============================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

