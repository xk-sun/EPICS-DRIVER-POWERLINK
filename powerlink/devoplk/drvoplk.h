/* $Author: zimoch $ */ 
/* $Date: 2005/03/11 15:16:13 $ */ 
/* $Id: drvS7plc.h,v 1.3 2005/03/11 15:16:13 zimoch Exp $ */  
/* $Name:  $ */ 
/* $Revision: 1.3 $ */ 

#ifndef drvoplk_h
#define drvoplk_h

#include <dbScan.h>

#ifndef __GNUC__
#define __attribute__(a)
#endif

#ifndef DEBUG
#define STATIC static
#else
#define STATIC
#endif

/*  driver initialisation define  */

typedef struct oplkcn oplkcn;

extern int oplkDebug;

void oplkDebugLog(int level, const char *fmt, ...)
__attribute__((format(printf, 2, 3)));

oplkcn *oplkOpen(char *name);
IOSCANPVT oplkGetInScanPvt(oplkcn *station);
IOSCANPVT oplkGetOutScanPvt(oplkcn *station);
//void shutdownPowerlink(void);
//void shutdownApp(void);



int oplkReadArray(
    oplkcn *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* pdata
);

int oplkWriteMaskedArray(
    oplkcn *station,
    unsigned int offset,
    unsigned int dlen,
    unsigned int nelem,
    void* pdata,
    void* pmask
);

#define oplkWriteArray(station, offset, dlen, nelem, pdata) \
    oplkWriteMaskedArray((station), (offset), (dlen), (nelem), (pdata), NULL)

#define oplkWriteMasked(station, offset, dlen, pdata, mask) \
    oplkWriteMaskedArray((station), (offset), (dlen), 1, (pdata), (mask))

/*#define oplkWrite(station, offset, dlen, pdata) \
    oplkWriteMaskedArrayao((station), (offset), (dlen), 1, (pdata), NULL)*/

#define oplkWrite(station, offset, dlen, pdata) \
    oplkWriteao((station), (offset), (dlen), 1, (pdata))

#define oplkRead(station, offset, dlen, pdata) \
    oplkReadArray((station), (offset), (dlen), 1, (pdata))

/************************************************************************/
/* * DRV driver error codes */
#define M_drvLib (1003<<16U)
#define drvError(CODE) (M_drvLib | (CODE))

#define S_drv_OK 0 /* success */
#define S_drv_badParam drvError(1) /*driver: bad parameter*/
#define S_drv_noMemory drvError(2) /*driver: no memory*/
#define S_drv_noDevice drvError(3) /*driver: device not configured*/
#define S_drv_invSigMode drvError(4)/*driver: signal mode conflicts with device config*/
#define S_drv_cbackChg drvError(5) /*driver: specified callback differs from previous config*/
#define S_drv_alreadyQd drvError(6)/*driver: a read request is already queued for the channel*/
#define S_drv_noConn drvError(7) /*driver:   connection to plc lost*/

#endif /* drvoplk_h */
