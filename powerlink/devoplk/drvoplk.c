/* $Author: xksun $ */
/* $Date: 2018/05/22  $ */
/* $Id: drvoplk.c,v 1.0 2018/05/22 xksun $ */
/* $Name:  $ */
/* $Revision: 1.0 $ */
 
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(vxWorks) || defined(__vxworks)
#include <sockLib.h>
#include <taskLib.h>
#include <selectLib.h>
#include <taskHookLib.h>
#define in_addr_t unsigned long
#else
#include <fcntl.h>
#endif

#ifdef __rtems__
#include <sys/select.h>
#endif

#include <drvSup.h>
#include <devLib.h>
#include <errlog.h>
#include <epicsVersion.h>

#include "drvoplk.h"

#if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)
/* R3.14 */
#include <dbAccess.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTimer.h>
#include <epicsEvent.h>
#include <epicsExport.h>
#else
/* R3.13 */
#include "compat3_13.h"
#endif

#define RECONNECT_DELAY   5.0  /* delay before reconnect [s] */
#define UNIX_DOMAIN "/tmp/LINUX.domain"

static int sockfd;
static char cvsid[] __attribute__((unused)) =
"$Id: drvoplk.c,v 1.0 2018/05/22 xksun $";

STATIC long oplkIoReport(int level); 
STATIC long oplkInit();
void oplkMain ();
STATIC void oplkRecvThread( );
STATIC void oplkSendThread( );
STATIC void oplkSignal(void* event);
oplkcn* oplkcnList = NULL;
static epicsTimerQueueId timerqueue = NULL;
static short bigEndianIoc;

struct {
    long number;
    long (*report)();
    long (*init)();
} oplk = {
    2,
    oplkIoReport,
    oplkInit
};
epicsExportAddress(drvet, oplk);

int oplkDebug = 0;
epicsExportAddress(int, oplkDebug);

struct oplkcn {
    struct oplkcn* next;
    char* name;
    int inSize;
    int outSize;
    char* inBuffer;
    char* outBuffer;
    int swapBytes;
    epicsMutexId mutex;
    epicsMutexId io;
    epicsTimerId timer;
    epicsEventId outTrigger;
    IOSCANPVT inScanPvt;
    IOSCANPVT outScanPvt;
};

void oplkDebugLog(int level, const char *fmt, ...)
{
    va_list args;
    
    if (level > oplkDebug) return;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

STATIC long oplkIoReport(int level)
{
    oplkcn *station;

    printf("%s\n", cvsid);
    
    return 0;
}

STATIC long oplkInit()
{
    if (!oplkcnList) {
        errlogSevPrintf(errlogInfo,
            "oplkInit: no stations configured\n");
        return 0;
    }
    oplkDebugLog(1, "oplkInit: starting main thread\n");
    epicsThreadCreate(
        "oplkMain",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(epicsThreadStackBig),
        (EPICSTHREADFUNC)oplkMain,
        NULL);
    return 0;
}

STATIC void oplkSignal(void* event)
{
    epicsEventSignal((epicsEventId)event);
}

int oplkcnConfigure(char *name,  int inSize, int outSize)
{
    oplkcn* station;
    oplkcn** pstation;
    
    union {short s; char c [sizeof(short)];} u;
    u.s=1;
    bigEndianIoc = !u.c[0];    
    
    /* find last station in list */
    for (pstation = &oplkcnList; *pstation; pstation = &(*pstation)->next);
    
    station = callocMustSucceed(1,
        sizeof(oplkcn) + inSize + outSize + strlen(name)+1 , "oplkcnConfigure");
    station->next = NULL;
    station->inSize = inSize;
    station->outSize = outSize;
    station->inBuffer = (char*)(station+1);
    station->outBuffer = (char*)(station+1)+inSize;
    station->name = (char*)(station+1)+inSize+outSize;
    strcpy(station->name, name);
    station->swapBytes = 1 ^ bigEndianIoc;
    station->mutex = epicsMutexMustCreate();
    station->io = epicsMutexMustCreate();
    if (station->outSize)
    {
        station->outTrigger = epicsEventMustCreate(epicsEventEmpty);
        if (!timerqueue)
        {
            timerqueue = epicsTimerQueueAllocate(1, epicsThreadPriorityHigh);
        }
        station->timer = epicsTimerQueueCreateTimer(timerqueue,
            oplkSignal, station->outTrigger);

    }
    scanIoInit(&station->inScanPvt);
    scanIoInit(&station->outScanPvt);

    /* append station to list */
    *pstation = station;
    pstation = &station->next;

    return 0;
}

#if (EPICS_REVISION>=14)
static const iocshArg oplkcnConfigureArg0 = { "name", iocshArgString };
static const iocshArg oplkcnConfigureArg1 = { "inSize", iocshArgInt };
static const iocshArg oplkcnConfigureArg2 = { "outSize", iocshArgInt };
static const iocshArg * const oplkcnConfigureArgs[] = {
    &oplkcnConfigureArg0,
    &oplkcnConfigureArg1,
    &oplkcnConfigureArg2,
};
static const iocshFuncDef oplkcnConfigureDef = { "oplkcnConfigure", 3, oplkcnConfigureArgs };
static void oplkcnConfigureFunc (const iocshArgBuf *args)
{
    int status = oplkcnConfigure(
        args[0].sval, args[1].ival, args[2].ival
        );
        
    if (status) exit(1);
}

static void oplkRegister ()
{
    iocshRegister(&oplkcnConfigureDef, oplkcnConfigureFunc);
}

epicsExportRegistrar(oplkRegister);
#endif

oplkcn *oplkOpen(char *name)
{
    oplkcn *station;

    for (station = oplkcnList; station; station = station->next)
    {
        if (strcmp(name, station->name) == 0)
        {
            return station;
        }
    }
    errlogSevPrintf(errlogFatal,
        "oplkOpen: station %s not found\n", name);
    return NULL;
}



IOSCANPVT oplkGetInScanPvt(oplkcn *station)
{
    return station->inScanPvt;
}

IOSCANPVT oplkGetOutScanPvt(oplkcn *station)
{
    return station->outScanPvt;
}

int oplkReadArray(
    oplkcn *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* data
)
{
    unsigned int elem, i;
    unsigned char byte;

    if (offset+dlen > station->inSize)
    {
       errlogSevPrintf(errlogMajor,
        "oplkRead %s/%u: offset out of range\n",
        station->name, offset);
       return S_drv_badParam;
    }
    if (offset+nelem*dlen > station->inSize)
    {
       errlogSevPrintf(errlogMajor, 
        "oplkRead %s/%u: too many elements (%u)\n",
        station->name, offset, nelem);
       return S_drv_badParam;
    }
    oplkDebugLog(4,
        "oplkReadArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->mutex);
    for (elem = 0; elem < nelem; elem++)
    {
        oplkDebugLog(5, "data in:");
        for (i = 0; i < dlen; i++)
        {
            if (station->swapBytes)
                byte = station->inBuffer[offset + elem*dlen + dlen - 1 - i];
            else
                byte = station->inBuffer[offset + elem*dlen + i];
            ((char*)data)[elem*dlen+i] = byte;
            oplkDebugLog(5, " %02x", byte);
        }
        oplkDebugLog(5, "\n");
    }    
    epicsMutexUnlock(station->mutex);
    return S_drv_OK;
}

int oplkWriteMaskedArray(
    oplkcn *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* data,
    void* mask
)
{
    unsigned int elem, i;
    unsigned char byte;

    if (offset+dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "oplkWrite %s/%d: offset out of range\n",
            station->name, offset);
        return -1;
    }
    if (offset+nelem*dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "oplkWrite %s/%d: too many elements (%u)\n",
            station->name, offset, nelem);
        return -1;
    }
    oplkDebugLog(4,
        "oplkWriteMaskedArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->mutex);
    for (elem = 0; elem < nelem; elem++)
    {
        oplkDebugLog(5, "data out:");
        for (i = 0; i < dlen; i++)
        {
            byte = ((unsigned char*)data)[elem*dlen+i];
            if (mask)
            {
                oplkDebugLog(5, "(%02x & %02x)",
                    byte, ((unsigned char*)mask)[i]);
                byte &= ((unsigned char*)mask)[i];
            }
            if (station->swapBytes)
            {
                if (mask)
                {
                    oplkDebugLog(5, " | (%02x & %02x) =>",
                        station->outBuffer[offset + elem*dlen + dlen - 1 - i],
                        ~((unsigned char*)mask)[i]);
                    byte |=
                        station->outBuffer[offset + elem*dlen + dlen - 1 - i]
                        & ~((unsigned char*)mask)[i];
                }
                oplkDebugLog(5, " %02x", byte);
                station->outBuffer[offset + elem*dlen + dlen - 1 - i] = byte;
            }
            else
            {
                oplkDebugLog(5, " %02x", byte);
                station->outBuffer[offset + elem*dlen + i] = byte;
            }
        }
        oplkDebugLog(5, "\n");
    }    
    epicsMutexUnlock(station->mutex);
//    if (!connStatus) return S_drv_noConn;
    return S_drv_OK;
}

int oplkWriteao(
    oplkcn *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* data
    )
{
    unsigned int elem, i;
    unsigned char byte;
    if (offset+dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "oplkWrite %s/%d: offset out of range\n",
            station->name, offset);
        return -1;
    }
    if (offset+nelem*dlen > station->outSize)
    {
        errlogSevPrintf(errlogMajor,
            "oplk %s/%d: too many elements (%u)\n",
            station->name, offset, nelem);

          return -1;
    }
    oplkDebugLog(4,
        "oplkWriteMaskedArray (station=%p, offset=%u, dlen=%u, nelem=%u)\n",
        station, offset, dlen, nelem);
    epicsMutexMustLock(station->mutex);
    for (elem = 0; elem < nelem; elem++)
    {
        oplkDebugLog(5, "data out:");
        for (i = 0; i < dlen; i++)
        {
	    byte = ((unsigned char*)data)[elem*dlen+i];
            if (station->swapBytes)
            {
               station->outBuffer[offset + elem*dlen + dlen - 1 - i] = byte;
	    }
            else
	    {
	       oplkDebugLog(5, " %02x", byte);
                station->outBuffer[offset + elem*dlen + i] = byte;
            }
	}
	oplkDebugLog(5, "\n");
    }
    epicsMutexUnlock(station->mutex);
    return S_drv_OK;
}

void oplkMain ()
{
    char *Recvthreadname = "Recvthread";
    char *Sendthreadname = "Sendthread";
// client socket definition
    int len;
    struct sockaddr_un address;
    int result;
    oplkcn *station;


    
    oplkDebugLog(1, "oplkMain: main thread started\n");
      
    
    /* creat socket */
    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
	printf("cannot create communication socket");
    }
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, "/tmp/UNIX.domain");
    len = sizeof(address);
	
    /* connect server*/
    result = connect(sockfd, (struct sockaddr *)&address, len);
    if(result == -1)
    {	
	printf("cannot connect to the server");
    }
        
    printf("The Receive Thread is starting:\n");
        
    while (1)
    {
        int received;
        int sent;
	char recvBuf[10];

        for(station = oplkcnList; station; station = station->next)
        {
                /*send thread*/
		oplkSendThread (station);
		
		/*receive thread*/
		oplkRecvThread (station);
        }
    	
    }

}
	

STATIC void oplkSendThread (oplkcn* station)
{
	int sent;
	char cnSendBuf[station->outSize];
	
	/*send data to server*/
	epicsMutexMustLock(station->mutex);
        memcpy(cnSendBuf, station->outBuffer, station->outSize);
	epicsMutexUnlock(station->mutex);
	sent = write(sockfd, cnSendBuf, sizeof(cnSendBuf));
	scanIoRequest(station->outScanPvt);

}

STATIC void oplkRecvThread (oplkcn* station)
{
	int received;
	char cnRecvBuf[station->inSize];
	
	/*receive data from server*/
	received = read(sockfd, cnRecvBuf, sizeof(cnRecvBuf));
        epicsMutexMustLock(station->mutex);
        memcpy(station->inBuffer, cnRecvBuf, station->inSize);
        epicsMutexUnlock(station->mutex);
        scanIoRequest(station->inScanPvt);
	
}
