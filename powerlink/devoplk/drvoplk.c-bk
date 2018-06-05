/* $Author: zimoch $ */
/* $Date: 2013/01/16 10:17:33 $ */
/* $Id: drvoplk.c,v 1.17 2018/05/21 10:17:33 xksun $ */
/* $Name:  $ */
/* $Revision: 1.17 $ */
 
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
"$Id: drvoplk.c,v 1.0 2018/05/21 10:17:33 xksun $";

STATIC long oplkIoReport(int level); 
STATIC long oplkInit();
void oplkMain ();
//STATIC void s7plcSendThread(s7plcStation* station);
STATIC void oplkReceiveThread( );
//STATIC int s7plcWaitForInput(s7plcStation* station, double timeout);
//STATIC int s7plcEstablishConnection(s7plcStation* station);
//STATIC void s7plcCloseConnection(s7plcStation* station);
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
    int outputChanged;
    IOSCANPVT inScanPvt;
    IOSCANPVT outScanPvt;
    epicsThreadId sendThread;
    epicsThreadId recvThread;
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
   /* if (level == 1)
    {
        printf("S7plc stations:\n");
        for (station = s7plcStationList; station;
            station=station->next)
        {
            printf("  Station %s ", station->name);
            if (station->connStatus)
            {
                printf("connected via file descriptor %d to\n",
                    station->socket);
            }
            else
            {
                printf("disconnected from\n");
            }
            printf("  plc with address %s on port %d\n",
                station->serverIP, station->serverPort);
            printf("    inBuffer  at address %p (%d bytes)\n",
                station->inBuffer,  station->inSize);
            printf("    outBuffer at address %p (%d bytes)\n",
                station->outBuffer,  station->outSize);
            printf("    swap bytes %s\n",
                station->swapBytes
                    ? ( bigEndianIoc ? "ioc:motorola <-> plc:intel" : "ioc:intel <-> plc:motorola" )
                    : ( bigEndianIoc ? "no, both motorola" : "no, both intel" ) );
            printf("    receive timeout %g sec\n",
                station->recvTimeout);
            printf("    send intervall  %g sec\n",
                station->sendIntervall);
        }
    }*/
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

int oplkcnConfigure(char *name,  int inSize, int outSize, int bigEndian)
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
    station->swapBytes = bigEndian ^ bigEndianIoc;
    station->mutex = epicsMutexMustCreate();
    station->io = epicsMutexMustCreate();
    station->outputChanged = 0;
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
    station->recvThread = NULL;
    station->sendThread = NULL;

    /* append station to list */
    *pstation = station;
    pstation = &station->next;

    return 0;
}

#if (EPICS_REVISION>=14)
static const iocshArg oplkcnConfigureArg0 = { "name", iocshArgString };
static const iocshArg oplkcnConfigureArg1 = { "inSize", iocshArgInt };
static const iocshArg oplkcnConfigureArg2 = { "outSize", iocshArgInt };
static const iocshArg oplkcnConfigureArg3 = { "bigEndian", iocshArgInt };
static const iocshArg * const oplkcnConfigureArgs[] = {
    &oplkcnConfigureArg0,
    &oplkcnConfigureArg1,
    &oplkcnConfigureArg2,
    &oplkcnConfigureArg3
};
static const iocshFuncDef oplkcnConfigureDef = { "oplkcnConfigure", 4, oplkcnConfigureArgs };
static void oplkcnConfigureFunc (const iocshArgBuf *args)
{
    int status = oplkcnConfigure(
        args[0].sval, args[1].ival, args[2].ival,
        args[3].ival);
        
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
   // connStatus = station->connStatus;
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
  //  if (!connStatus) return S_drv_noConn;
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
//    epicsUInt16 connStatus;

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
   // connStatus = station->connStatus;
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
        station->outputChanged=1;
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
    //epicsUInt16 connStatus;
    //mask = NULL;
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
        station->outputChanged=1;
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
    epicsThreadId sendThread = NULL;
    epicsThreadId recvThread = NULL;
    char recvBuf[10];
    oplkcn *station;


    
    oplkDebugLog(1, "oplkMain: main thread started\n");
    
    
    
	/* watch loop to restart dead threads and reopen sockets*/
	
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
        
        char  *name1,*name2,*name3,*name4,*name5,*name6,*name7,*name8;
        name1 = "testcn:1";
        name2 = "testcn:2";
        name3 = "testcn:3";
        name4 = "testcn:4";
        name5 = "testcn:5";
        name6 = "testcn:6";
        name7 = "testcn:7";
        name8 = "testcn:8";


    printf("The Receive Thread is starting:\n");
        
    while (1)
    {
        int received;
        int sent;
	char recvBuf[10];

        for(station = oplkcnList; station; station = station->next)
        {
                if(strcmp(station->name, name1) == 0)
                {
                        char cn1RecvBuf[station->inSize];
                        char cn1SendBuf[station->outSize];
                        
			epicsMutexMustLock(station->mutex);
			memcpy(cn1SendBuf, station->outBuffer, station->outSize);
			printf("output is %d\n", cn1SendBuf[0]);
			epicsMutexUnlock(station->mutex);
			sent = write(sockfd, cn1SendBuf, sizeof(cn1SendBuf));
			scanIoRequest(station->outScanPvt);
                        
			received = read(sockfd, recvBuf, sizeof(recvBuf));
			cn1RecvBuf[0] = recvBuf[0];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn1RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
                        scanIoRequest(station->inScanPvt);
                        
                }
        }
    	
    }

}
   	/* Receive Thread
	if(recvThread && epicsThreadIsSuspended(recvThread))
	{
		printf("recv thread is dead\n");
		close(sockfd);
		recvThread = 0;
	}
	
	if(!recvThread)
	{
		printf("starting recv thread\n");
		recvThread = epicsThreadCreate(
                    Recvthreadname,
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackBig),
                    (EPICSTHREADFUNC)oplkReceiveThread,
                    NULL);
		if(!recvThread)
		{
			printf("cannot start recv thread\n");
		}
	}*/
	
	//epicsThreadSleep(RECONNECT_DELAY);

    
/*   while (1)
    {    watch loop to restart dead threads and reopen sockets*/
        
      /*  for (station = s7plcStationList; station; station=station->next)
        {            
             establish connection with server */
           /* epicsMutexMustLock(station->io);
            if (station->socket == -1)
            {
                 create station socket */
               /* if ((station->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
                {
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! socket(AF_INET, SOCK_STREAM, 0) failed: %s\n",
                        station->name, strerror(errno));
                    abort();
                }
                s7plcDebugLog(1,
                    "s7plcMain %s: Connect to %s:%d on socket %d\n",
                    station->name,
                    station->serverIP, station->serverPort, station->socket);
                if (s7plcEstablishConnection(station) < 0)
                {
                    s7plcDebugLog(1,
                        "s7plcMain %s: connect(%d, %s:%d) failed: %s. Retry in %g seconds\n",
                        station->name,
                        station->socket, station->serverIP, station->serverPort,
                        strerror(errno), (double)RECONNECT_DELAY);
                    if (close(station->socket) && errno != ENOTCONN)
                    {
                        s7plcDebugLog(1,
                            "s7plcMain %s: close(%d) failed (ignored): %s\n",
                            station->name, station->socket, strerror(errno));
                    }
                    station->socket=-1;
                }
            }
            epicsMutexUnlock(station->io);

             check whether station threads are running */
            
           /* sprintf (threadname, "%.15sS", station->name);
            if (station->sendThread && epicsThreadIsSuspended(station->sendThread))
            {     if suspended delete it */
               /* s7plcDebugLog(0,
                    "s7plcMain %s: send thread %s %p is dead\n",
                    station->name, threadname, station->sendThread);
                 maybe we should cleanup the semaphores ? */
               /* s7plcCloseConnection(station);
                station->sendThread = 0;
            }
            if (!station->sendThread && station->outSize)
            {
                s7plcDebugLog(1,
                    "s7plcMain %s: starting send thread %s\n",
                    station->name, threadname);
                station->sendThread = epicsThreadCreate(
                    threadname,
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackBig),
                    (EPICSTHREADFUNC)s7plcSendThread,
                    station);
                if (!station->sendThread)
                {
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! could not start send thread %s\n",
                        station->name, threadname);
                    abort();
                }
            }
            
            sprintf (threadname, "%.15sR", station->name);
            if (station->recvThread && epicsThreadIsSuspended(station->recvThread))
            {     if suspended delete it */
              /*  s7plcDebugLog(0,
                    "s7plcMain %s: recv thread %s %p is dead\n",
                    station->name, threadname, station->recvThread);
                 maybe we should cleanup the semaphores ? */
               /* s7plcCloseConnection(station);
                station->recvThread = 0;
            }
            if (!station->recvThread && station->inSize)
            {
                s7plcDebugLog(1,
                    "s7plcMain %s: starting recv thread %s\n",
                    station->name, threadname);
                station->recvThread = epicsThreadCreate(
                    threadname,
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackBig),
                    (EPICSTHREADFUNC)s7plcReceiveThread,
                    station);
                if (!station->recvThread)
                {
                    fprintf(stderr, "s7plcMain %s: FATAL ERROR! could not start recv thread %s\n",
                        station->name, threadname);
                    abort();
                }
            }
            
        }
        epicsThreadSleep(RECONNECT_DELAY);
    }        
}*/

/*STATIC void s7plcSendThread (s7plcStation* station)
{
    char*  sendBuf = callocMustSucceed(1, station->outSize, "s7plcSendThread ");

    s7plcDebugLog(1, "s7plcSendThread %s: started\n",
            station->name);

    while (1)
    {
        epicsTimerStartDelay(station->timer, station->sendIntervall);
        s7plcDebugLog(2, "s7plcSendThread %s: look for data to send\n",
            station->name);

        if (interruptAccept && station->socket != -1)
        {
            if (station->outputChanged)
            {
                epicsMutexMustLock(station->mutex);
                memcpy(sendBuf, station->outBuffer, station->outSize);
                station->outputChanged = 0; 
                epicsMutexUnlock(station->mutex);

                s7plcDebugLog(4, "send %d bytes\n", station->outSize);
                epicsMutexMustLock(station->io);
                if (station->socket != -1)
                {
                    int written;
                    s7plcDebugLog(2,
                        "s7plcSendThread %s: sending %d bytes\n",
                        station->name, station->outSize);
                    written=send(station->socket, sendBuf, station->outSize, 0);
                    if (written < 0)
                    {
                        s7plcDebugLog(0,
                            "s7plcSendThread %s: send(%d, ..., %d, 0) failed: %s\n",
                            station->name,
                            station->socket, station->outSize, strerror(errno));
                        s7plcCloseConnection(station);
                    }
                }
                epicsMutexUnlock(station->io);
            }
             notify all "I/O Intr" output records 
            s7plcDebugLog(2,
                "s7plcSendThread %s: send cycle done, notify all output records\n",
                station->name);
            scanIoRequest(station->outScanPvt);
        }
        epicsEventMustWait(station->outTrigger);
    }
}*/

STATIC void oplkReceiveThread ( )
{
    char recvBuf[10];
    oplkcn *station;
// cn name
    char  *name1,*name2,*name3,*name4,*name5,*name6,*name7,*name8;
    name1 = "testcn:1";
    name2 = "testcn:2";
    name3 = "testcn:3";
    name4 = "testcn:4";
    name5 = "testcn:5";
    name6 = "testcn:6";
    name7 = "testcn:7";
    name8 = "testcn:8";


    printf("The Receive Thread is starting:\n");    

    while (1)
    {
        int received;

        
	received = read(sockfd, recvBuf, sizeof(recvBuf));
	if(received < 0)
	{
		printf("receive data failed \n");
	}
	for(station = oplkcnList; station; station = station->next)
	{
		if(strcmp(station->name, name1) == 0)
		{
			char cn1RecvBuf[station->inSize];
			
			cn1RecvBuf[0] = recvBuf[0];
			epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn1RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
			scanIoRequest(station->inScanPvt);

			
		}
		
		
		else if(strcmp(station->name, name2) == 0)
		{
			char cn2RecvBuf[station->inSize];

                        cn2RecvBuf[0] = recvBuf[1];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn2RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
			scanIoRequest(station->inScanPvt);

		}

		else if(strcmp(station->name, name3) == 0)
                {
                        char cn3RecvBuf[station->inSize];

                        cn3RecvBuf[0] = recvBuf[2];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn3RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
			scanIoRequest(station->inScanPvt);

                }

		else if(strcmp(station->name, name4) == 0)
                {
                        char cn4RecvBuf[station->inSize];

                        cn4RecvBuf[0] = recvBuf[3];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn4RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
			scanIoRequest(station->inScanPvt);

                }
		
		else if(strcmp(station->name, name5) == 0)
                {
                        char cn5RecvBuf[station->inSize];

                        cn5RecvBuf[0] = recvBuf[4];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn5RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);

                        scanIoRequest(station->inScanPvt);

                }

		else if(strcmp(station->name, name6) == 0)
                {
                        char cn6RecvBuf[station->inSize];

                        cn6RecvBuf[0] = recvBuf[5];
                        epicsMutexMustLock(station->mutex);
                        memcpy(station->inBuffer, cn6RecvBuf, station->inSize);
                        epicsMutexUnlock(station->mutex);
			scanIoRequest(station->inScanPvt);

                }

	}
    }
}
	/* check (with timeout) for data arrival from server 
        while (station->socket != -1 && input < station->inSize)
        {
             Don't lock here! We need to be able to send while we wait 
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: waiting for input for %g seconds\n",
                station->name, timeout);
            status = s7plcWaitForInput(station, timeout);
            epicsMutexMustLock(station->io);
            if (status > 0)
            {
                /* data available; read data from server plc 
                received = recv(station->socket, recvBuf+input, station->inSize-input, 0);
                s7plcDebugLog(3,
                    "s7plcReceiveThread %s: received %d bytes\n",
                    station->name, received);
                if (received <= 0)
                {
                    s7plcDebugLog(0,
                        "s7plcReceiveThread %s: recv(%d, ..., %d, 0) failed: %s\n",
                        station->name,
                        station->socket, station->inSize-input,
                        strerror(errno));
                    s7plcCloseConnection(station);
                    epicsMutexUnlock(station->io);
                    break;
                }
                input += received;
            }
            if (input > station->inSize)
            {
                 input complete, check for excess bytes 
                if (status > 0)
                {
                    s7plcDebugLog(0,
                        "s7plcReceiveThread %s: %d bytes excess data received\n",
                        station->name, input - station->inSize);
                    s7plcCloseConnection(station);
                }
                epicsMutexUnlock(station->io);
                break;
            }
            if (status <= 0 && timeout > 0.0)
            {
                s7plcDebugLog(0,
                    "s7plcReceiveThread %s: read error after %d of %d bytes: %s\n",
                    station->name,
                    input, station->inSize, strerror(errno));
                s7plcCloseConnection(station);
                epicsMutexUnlock(station->io);
                break;
            }
            epicsMutexUnlock(station->io);
/*            timeout = (input < station->inSize)? 0.1 : 0.0; 
        }
        if (station->socket != -1)
        {
            epicsMutexMustLock(station->mutex);
            memcpy(station->inBuffer, recvBuf, station->inSize);
            station->connStatus = 1;
            epicsMutexUnlock(station->mutex);
             notify all "I/O Intr" input records 
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: receive successful, notify all input records\n",
                station->name);
            scanIoRequest(station->inScanPvt);
        }
        else
        {
            s7plcDebugLog(3,
                "s7plcReceiveThread %s: connection down, sleeping %g seconds\n",
                station->name, station->recvTimeout/4);
             lost connection. Wait some time 
            epicsThreadSleep(station->recvTimeout/4);
        }
    }
}*/

/*STATIC int s7plcWaitForInput(s7plcStation* station, double timeout)
{
    static struct timeval to;
    int socket;
    int iSelect;
    fd_set socklist;

    socket = station->socket;
    FD_ZERO(&socklist);
    FD_SET(socket, &socklist);
    to.tv_sec=(int)timeout;
    to.tv_usec=(int)((timeout-to.tv_sec)*1000000);
     select returns when either the socket has data or the timeout elapsed 
    errno = 0;
    while ((iSelect=select(socket+1,&socklist, 0, 0,&to)) < 0)
    {
        if (errno != EINTR)
        {
            s7plcDebugLog(0,
                "s7plcWaitForInput %s: select(%d, %f sec) failed: %s\n",
                station->name, station->socket, timeout,
                strerror(errno));
            return -1;
        }
    }
    if (iSelect==0 && timeout > 0)             timed out 
    {
        s7plcDebugLog(0,
            "s7plcWaitForInput %s: select(%d, %f sec) timed out\n",
            station->name, station->socket, timeout);
        errno = ETIMEDOUT;
    }
    return iSelect;
}*/

/*STATIC int s7plcEstablishConnection(s7plcStation* station)
{
    struct sockaddr_in    serverAddr;     server socket address 
    struct timeval    to;
#if (!defined(vxWorks)) && (!defined(__vxworks))
    long opt;
#endif

    s7plcDebugLog(1, "s7plcEstablishConnection %s: fd=%d, IP=%s port=%d\n",
        station->name,
        station->socket, station->serverIP, station->serverPort);

     build server socket address 
    memset((char *) &serverAddr, 0, sizeof (serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(station->serverPort);
    serverAddr.sin_addr.s_addr = inet_addr(station->serverIP);

     connect to server 
    to.tv_sec=(int)(CONNECT_TIMEOUT);
    to.tv_usec=(int)(CONNECT_TIMEOUT-to.tv_sec)*1000000;
#if defined(vxWorks) || defined(__vxworks)
    if (connectWithTimeout(station->socket,
        (struct sockaddr *) &serverAddr, sizeof (serverAddr), &to) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: connectWithTimeout(%d,...,%g sec) failed: %s\n",
            station->name, station->socket, CONNECT_TIMEOUT, strerror(errno));
        return -1;
    }
#else
     connect in non-blocking mode 
    if((opt = fcntl(station->socket, F_GETFL, NULL)) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_GETFL, NULL) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
    opt |= O_NONBLOCK;
    if(fcntl(station->socket, F_SETFL, opt) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_SETFL, O_NONBLOCK) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
    if (connect(station->socket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
             start timeout 
            int status;
            socklen_t lon = sizeof(status);
            fd_set fdset;

            FD_ZERO(&fdset);
            FD_SET(station->socket, &fdset);
             wait for connection 
            while ((status = select(station->socket+1, NULL, &fdset, NULL, &to)) < 0)
            {
                if (errno != EINTR)
                {
                    s7plcDebugLog(0,
                        "s7plcEstablishConnection %s: select(%d, %f sec) failed: %s\n",
                        station->name, station->socket, CONNECT_TIMEOUT, strerror(errno));
                    return -1;
                }
            }
            if (status == 0)
            {
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: select(%d, %f sec) timed out\n",
                    station->name, station->socket, CONNECT_TIMEOUT);
                errno = ETIMEDOUT;
                return -1;
            }
             get background error status 
            if (getsockopt(station->socket, SOL_SOCKET, SO_ERROR, &status, &lon) < 0)
            {
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: getsockopt(%d,...) failed: %s\n",
                    station->name,
                    station->socket, strerror(errno));
                return -1;
            }
            if (status)
            {
                errno = status;
                s7plcDebugLog(0,
                    "s7plcEstablishConnection %s: background connect(%d,...) failed: %s\n",
                    station->name,
                    station->socket, strerror(errno));
                return -1;
            }
        }
        else
        {
            s7plcDebugLog(0,
                "s7plcEstablishConnection %s: connect(%d,...) failed: %s\n",
                station->name,
                station->socket, strerror(errno));
            return -1;
        }
    }
     connected 
    opt &= ~O_NONBLOCK;
    if(fcntl(station->socket, F_SETFL, opt) < 0)
    {
        s7plcDebugLog(0,
            "s7plcEstablishConnection %s: fcntl(%d, F_SETFL, ~O_NONBLOCK) failed: %s\n",
            station->name,
            station->socket, strerror(errno));
        return -1;
    }
#endif
    return 0;
}*/

/*STATIC void s7plcCloseConnection(s7plcStation* station)
{
    station->connStatus = 0;
    if (station->socket>0)
    {
        if (shutdown(station->socket, 2) < 0)
        {
            s7plcDebugLog(0,
                "s7plcCloseConnection %s: shutdown(%d, 2) failed (ignored): %s\n",
                station->name,
                station->socket, strerror(errno));
        }
        if (close(station->socket) && errno != ENOTCONN)
        {
            s7plcDebugLog(0,
                "s7plcCloseConnection %s: close(%d) failed (ignored): %s\n",
                station->name,
                station->socket, strerror(errno));
        }
        station->socket = -1;
    }
     notify all "I/O Intr" input records 
    scanIoRequest(station->inScanPvt);
}*/
