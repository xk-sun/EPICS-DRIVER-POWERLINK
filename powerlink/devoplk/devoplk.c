/*tihis is originated from devoplkplc.c which is developed by zimoch*/

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#include <alarm.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <devSup.h>
#include <devLib.h>
#include <errlog.h>

#include <epicsVersion.h>
#include <drvoplk.h>


#include <biRecord.h>
#include <boRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <mbbiDirectRecord.h>
#include <mbboDirectRecord.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <aiRecord.h>
#include <aoRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <waveformRecord.h>

#if ((EPICS_VERSION==3 && EPICS_REVISION>=14) || EPICS_VERSION>3)
/* R3.14 */
#include <postfix.h>
#include <calcoutRecord.h>
#include <cantProceed.h>
#include <epicsExport.h>
#else
/* R3.13 */
#include "compat3_13.h"
#endif
#define isnan(x) ((x)!=(x))

/* suppress compiler warning concerning long long with __extension__ */
#if (!defined __GNUC__) || (__GNUC__ < 2) || (__GNUC__ == 2 && __GNUC_MINOR__ < 8)
#define __extension__
#endif

#ifndef epicsUInt64
#if (LONG_MAX > 2147483647L)
#define epicsUInt64 unsigned long
#define CONV64 "%016lx"
#else
#define epicsUInt64 unsigned long long
#define CONV64 "%016llx"
#endif
#endif

#define oplkMEM_TIME 100

typedef struct {              /* Private structure to save IO arguments */
    oplkcn *station;    /* Card id */
    unsigned short offs;      /* Offset (in bytes) within memory block */
    unsigned short bit;       /* Bit number (0-15) for bi/bo */
    unsigned short dtype;     /* Data type */
    unsigned short dlen;      /* Data length (in bytes) */
    epicsInt32 hwLow;         /* Hardware Low limit */
    epicsInt32 hwHigh;        /* Hardware High limit */
} oplkmemPrivate_t;

static char cvsid_devoplk[] =
    "$Id: devoplk.c,v 1 2017/02/17 10:17:14  $";

STATIC long oplkReport();

STATIC int oplkIoParse(char* recordName, char *parameters, oplkmemPrivate_t *);
STATIC long oplkGetInIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);
STATIC long oplkGetOutIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt);

struct devsup {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN io;
};

/* bi for status bit ************************************************/

STATIC long oplkInitRecordStat(biRecord *);
STATIC long oplkReadStat(biRecord *);

struct devsup oplkStat =
{
    5,
    NULL,
    NULL,
    oplkInitRecordStat,
    oplkGetInIntInfo,
    oplkReadStat
};

epicsExportAddress(dset, oplkStat);

/* bi ***************************************************************/

STATIC long oplkInitRecordBi(biRecord *);
STATIC long oplkReadBi(biRecord *);

struct devsup oplkBi =
{
    5,
    NULL,
    NULL,
    oplkInitRecordBi,
    oplkGetInIntInfo,
    oplkReadBi
};

epicsExportAddress(dset, oplkBi);

/* bo ***************************************************************/

STATIC long oplkInitRecordBo(boRecord *);
STATIC long oplkWriteBo(boRecord *);

struct devsup oplkBo =
{
    5,
    NULL,
    NULL,
    oplkInitRecordBo,
    oplkGetOutIntInfo,
    oplkWriteBo
};

epicsExportAddress(dset, oplkBo);

/* ai ***************************************************************/

STATIC long oplkInitRecordAi(aiRecord *);
STATIC long oplkReadAi(aiRecord *);
STATIC long oplkSpecialLinconvAi(aiRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} oplkAi =
{
    6,
    NULL,
    NULL,
    oplkInitRecordAi,
    oplkGetInIntInfo,
    oplkReadAi,
    oplkSpecialLinconvAi
};

epicsExportAddress(dset, oplkAi);

/* ao ***************************************************************/

STATIC long oplkInitRecordAo(aoRecord *);
STATIC long oplkWriteAo(aoRecord *);
STATIC long oplkSpecialLinconvAo(aoRecord *, int after);

struct {
    long      number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN write;
    DEVSUPFUN special_linconv;
} oplkAo =
{
    6,
    NULL,
    NULL,
    oplkInitRecordAo,
    oplkGetOutIntInfo,
    oplkWriteAo,
    oplkSpecialLinconvAo
};

epicsExportAddress(dset, oplkAo);


/*********  Report routine ********************************************/

STATIC long oplkReport()
{
   printf("devoplkmem version: %s\n", cvsid_devoplk);
   return 0;
}

/*********  Support for "I/O Intr" for input records ******************/

STATIC long oplkGetInIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    oplkmemPrivate_t* p = record->dpvt;
    if (p == NULL)
    {
        recGblRecordError(S_db_badField, record,
            "oplkGetInIntInfo: uninitialized record");
        return -1;
    }
    *ppvt = oplkGetInScanPvt(p->station);
    return 0;
}

/*********  Support for "I/O Intr" for output records ****************/

STATIC long oplkGetOutIntInfo(int cmd, dbCommon *record, IOSCANPVT *ppvt)
{
    oplkmemPrivate_t* p = record->dpvt;
    if (p == NULL)
    {
        recGblRecordError(S_db_badField, record,
            "oplkGetInIntInfo: uninitialized record");
        return -1;
    }
    *ppvt = oplkGetOutScanPvt(p->station);
    return 0;
}

/***********************************************************************
 *   Routine to parse IO arguments
 *   IO address line format:
 *
 *    <devName>/<a>[+<o>] [T=<datatype>] [B=<bitnumber>] [L=<hwLow|strLen>] [H=<hwHigh>]
 *
 *   where: <devName>   - symbolic device name
 *          <a+o>       - address (byte number) within memory block
 *          <params>    - parameters to be passed to a particular
 *                        devSup parsering routine
 *          <datatype>  - INT8, INT16, INT32,
 *                        UINT16 (or UNSIGN16), UINT32 (or UNSIGN32),
 *                        REAL32 (or FLOAT), REAL64 (or DOUBLE),
 *                        STRING,TIME
 *          <bitnumber> - least significant bit is 0
 *          <hwLow>     - raw value that mapps to EGUL
 *          <hwHigh>    - raw value that mapps to EGUF
 **********************************************************************/

STATIC int oplkIoParse(char* recordName, char *par, oplkmemPrivate_t *priv)
{
    char devName[255];
    char *p = par, separator;
    int nchar, i;
    int status = 0;

    struct {char* name; int dlen; epicsType type;} datatypes [] =
    {
        { "INT8",     1, epicsInt8T    },
        
        { "UINT8",    1, epicsUInt8T   },
        { "UNSIGN8",  1, epicsUInt8T   },
        { "BYTE",     1, epicsUInt8T   },
        { "CHAR",     1, epicsUInt8T   },
        
        { "INT16",    2, epicsInt16T   },
        { "SHORT",    2, epicsInt16T   },
        
        { "UINT16",   2, epicsUInt16T  },
        { "UNSIGN16", 2, epicsUInt16T  },
        { "WORD",     2, epicsUInt16T  },
        
        { "INT32",    4, epicsInt32T   },
        { "LONG",     4, epicsInt32T   },
        
        { "UINT32",   4, epicsUInt32T  },
        { "UNSIGN32", 4, epicsUInt32T  },
        { "DWORD",    4, epicsUInt32T  },

        { "REAL32",   4, epicsFloat32T },
        { "FLOAT32",  4, epicsFloat32T },
        { "FLOAT",    4, epicsFloat32T },

        { "REAL64",   8, epicsFloat64T },
        { "FLOAT64",  8, epicsFloat64T },
        { "DOUBLE",   8, epicsFloat64T },

        { "TIME",     1, oplkMEM_TIME    },
        { "BCD",      1, oplkMEM_TIME    }
    };

    /* Get rid of leading whitespace and non-alphanumeric chars */
    while (!isalnum((unsigned char)*p))
        if (*p++ == '\0') return S_drv_badParam;

    /* Get device name */
    nchar = strcspn(p, "/");
    strncpy(devName, p, nchar);
    devName[nchar] = '\0';
    p += nchar;
    separator = *p++;
    oplkDebugLog(1, "oplkIoParse %s: station=%s\n", recordName, devName);

    priv->station = oplkOpen(devName);
    if (!priv->station)
    {
        errlogSevPrintf(errlogFatal, "oplkIoParse %s: device not found\n",
            recordName);
        return S_drv_noDevice;
    }

    /* Check station offset */
    if (separator == '/')
    {
        priv->offs = strtol(p, &p, 0);
        separator = *p++;
        /* Handle any number of optional +o additions to the offs */
        while (separator == '+')
        {
            priv->offs += strtol(p, &p, 0);
            separator = *p++;
        }
    }
    else
    {
        priv->offs = 0;
    }

    oplkDebugLog(1, "oplkIoParse %s: offs=%d\n", recordName, priv->offs);

    /* set default values for parameters */
    if (!priv->dtype && !priv->dlen)
    {
        priv->dtype = epicsInt16T;
        priv->dlen = 2;
    }
    priv->bit = 0;
    priv->hwLow = 0;
    priv->hwHigh = 0;
    
    /* allow whitespaces before parameter for device support */
    while ((separator == '\t') || (separator == ' '))
        separator = *p++;

    /* handle parameter for device support if present */
    nchar = 0;
    if (separator != '\'') p--; /* quote is optional*/
    
    /* parse parameters */
    while (p && *p)
    {
        switch (*p)
        {
            case ' ':
            case '\t':
                p++;
                break;
            case 'T': /* T=<datatype> */
                p+=2; 
                
                if (strncmp(p,"STRING",6) == 0)
                {
                    priv->dtype = epicsStringT;
                    p += 6;
                }
                else
                {
                    static int maxtype =
                        sizeof(datatypes)/sizeof(*datatypes);
                    for (i = 0; i < maxtype; i++)
                    {
                        nchar = strlen(datatypes[i].name);
                        if (strncmp(p, datatypes[i].name, nchar) == 0)
                        {
                            priv->dtype = datatypes[i].type;
                            priv->dlen = datatypes[i].dlen;
                            p += nchar;
                            break;
                        }
                    }
                    if (i == maxtype)
                    {
                        errlogSevPrintf(errlogFatal,
                            "oplkIoParse %s: invalid datatype %s\n",
                            recordName, p);
                        return S_drv_badParam;
                    }
                }
                break;
            case 'B': /* B=<bitnumber> */
                p += 2;
                priv->bit = strtol(p,&p,0);
                break;
            case 'L': /* L=<low raw value> (converts to EGUL)*/
                p += 2;
                priv->hwLow = strtol(p,&p,0);
                break;
            case 'H': /* L=<high raw value> (converts to EGUF)*/
                p += 2;
                priv->hwHigh = strtol(p,&p,0);
                break;
            case '\'':
                if (separator == '\'')
                {
                    p = 0;
                    break;
                }
            default:
                errlogSevPrintf(errlogFatal,
                    "oplkIoParse %s: unknown parameter '%c'\n",
                    recordName, *p);
                return S_drv_badParam;
        }
    }
    
    /* for T=STRING L=... means length, not low */
    if (priv->dtype == epicsStringT && priv->hwLow)
    {
        priv->dlen = priv->hwLow;
        priv->hwLow = 0;
    }
    
    /* check if bit number is in range */
    if (priv->bit && priv->bit >= priv->dlen*8)
    {
        errlogSevPrintf(errlogFatal,
            "oplkIoParse %s: invalid bit number %d (>%d)\n",
            recordName, priv->bit, priv->dlen*8-1);
        return S_drv_badParam;
    }
    
    /* get default values for L and H if user did'n define them */
    switch (priv->dtype)
    {
        case epicsUInt8T:
            if (priv->hwHigh > 0xFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x00;
            if (!priv->hwHigh) priv->hwHigh = 0xFF;
            break;
        case epicsUInt16T:
            if (priv->hwHigh > 0xFFFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0x0000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFF;
            break;
        case epicsUInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x00000000;
            if (!priv->hwHigh) priv->hwHigh = 0xFFFFFFFF;
            break;
        case epicsInt8T:
            if (priv->hwHigh > 0x7F) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFFFF81;
            if (!priv->hwHigh) priv->hwHigh = 0x0000007F;
            break;
        case epicsInt16T:
            if (priv->hwHigh > 0x7FFF) status = S_drv_badParam;
            if (!priv->hwHigh) priv->hwLow = 0xFFFF8001;
            if (!priv->hwHigh) priv->hwHigh = 0x00007FFF;
            break;
        case epicsInt32T:
            if (!priv->hwHigh) priv->hwLow = 0x80000001;
            if (!priv->hwHigh) priv->hwHigh = 0x7FFFFFFF;
            break;
        default:
            if (priv->hwHigh || priv->hwLow) {
                errlogSevPrintf(errlogMinor,
                    "oplkIoParse %s: L or H makes"
                    " no sense with this data type\n",
                    recordName);
            } 
            break;   
    }
    oplkDebugLog(1, "oplkIoParse %s: dlen=%d\n",recordName, priv->dlen);
    oplkDebugLog(1, "oplkIoParse %s: B=%d\n",   recordName, priv->bit);
    oplkDebugLog(1, "oplkIoParse %s: L=%#x\n",  recordName, priv->hwLow);
    oplkDebugLog(1, "oplkIoParse %s: H=%#x\n",  recordName, priv->hwHigh);

    if (status)
    {
        errlogSevPrintf(errlogMinor,
            "oplkIoParse %s: L or H out of range for this data type\n",
            recordName);
        return status;
    }
    
    return 0;
}

/* bi for status bit ************************************************/

STATIC long oplkInitRecordStat(biRecord *record)
{
    oplkmemPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordStat: illegal INP field type");
        return S_db_badField;
    }
    priv = (oplkmemPrivate_t *)callocMustSucceed(1, sizeof(oplkmemPrivate_t),
        "oplkInitRecordStat");
    status = oplkIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordStat: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    record->dpvt = priv;
    return 0;
}


STATIC long oplkReadStat(biRecord *record)
{
    int status;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    /* psudo-read (0 bytes) just to get the connection status */
    status = oplkReadArray(priv->station, 0, 0, 0, NULL);
    if (status == S_drv_noConn)
    {
        record->rval = 0;
        return 0;
    }
    if (status)
    {
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        record->rval = 0;
        return status;
    }
    record->rval = 1;
    return 0;
}

/* bi ***************************************************************/

STATIC long oplkInitRecordBi(biRecord *record)
{
    oplkmemPrivate_t *priv;
    int status;

    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordBi: illegal INP field type");
        return S_db_badField;
    }
    priv = (oplkmemPrivate_t *)callocMustSucceed(1,
        sizeof(oplkmemPrivate_t), "oplkInitRecordBi");
    status = oplkIoParse(record->name,
        record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordBi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "oplkInitRecordBi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 0;
}

STATIC long oplkReadBi(biRecord *record)
{
    int status;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            status = oplkRead(priv->station, priv->offs,
                1, &rval8);
            oplkDebugLog(3, "bi %s: read 8bit %02x\n",
                record->name, rval8);
            rval32 = rval8;
            break;
        case epicsInt16T:
        case epicsUInt16T:
            status = oplkRead(priv->station, priv->offs,
                2, &rval16);
            oplkDebugLog(3, "bi %s: read 16bit %04x\n",
                record->name, rval16);
            rval32 = rval16;
            break;
        case epicsInt32T:
        case epicsUInt32T:
            status = oplkRead(priv->station, priv->offs,
                4, &rval32);
            oplkDebugLog(3, "bi %s: read 32bit %04x\n",
                record->name, rval32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    record->rval = rval32 & record->mask;
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
    }
    return status;
}

/* bo ***************************************************************/

STATIC long oplkInitRecordBo(boRecord *record)
{
    oplkmemPrivate_t *priv;
    int status;

    if (record->out.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordBo: illegal OUT field");
        return S_db_badField;
    }
    priv = (oplkmemPrivate_t *)callocMustSucceed(1,
        sizeof(oplkmemPrivate_t), "oplkInitRecordBo");
    status = oplkIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordBo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "oplkInitRecordBo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->mask = 1 << priv->bit;
    record->dpvt = priv;
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long oplkWriteBo(boRecord *record)
{
    int status;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;
    epicsUInt8 rval8, mask8;
    epicsUInt16 rval16, mask16;
    epicsUInt32 rval32, mask32;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
            rval8 = record->rval;
            mask8 = record->mask;
            oplkDebugLog(2, "bo %s: write 8bit %02x mask %02x\n",
                record->name, rval8, mask8);
            status = oplkWriteMasked(priv->station, priv->offs,
                1, &rval8, &mask8);
            break;
        case epicsInt16T:
        case epicsUInt16T:
            rval16 = record->rval;
            mask16 = record->mask;
            oplkDebugLog(2, "bo %s: write 16bit %04x mask %04x\n",
                record->name, rval16, mask16);
            status = oplkWriteMasked(priv->station, priv->offs,
                2, &rval16, &mask16);
            break;
        case epicsInt32T:
        case epicsUInt32T:
            rval32 = record->rval;
            mask32 = record->mask;
            oplkDebugLog(2, "bo %s: write 32bit %08x mask %08x\n",
                record->name, rval32, mask32);
            status = oplkWriteMasked(priv->station, priv->offs,
                4, &rval32, &mask32);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

 /* ai ***************************************************************/

STATIC long oplkInitRecordAi(aiRecord *record)
{
    oplkmemPrivate_t *priv;
    int status;

   // epicsAtExit(shutdownPowerlink, NULL);
    if (record->inp.type != INST_IO)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordAi: illegal INP field type");
        return S_db_badField;
    }
    priv = (oplkmemPrivate_t *)callocMustSucceed(1, sizeof(oplkmemPrivate_t),
        "oplkInitRecordAi");
    status = oplkIoParse(record->name, record->inp.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordAi: bad INP field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "oplkInitRecordAi %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    oplkSpecialLinconvAi(record, TRUE);
    return 0;
}

STATIC long oplkReadAi(aiRecord *record)
{
    int status, floatval = FALSE;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;
    signed char sval8;
    unsigned char uval8;
    epicsInt16 sval16;
    epicsUInt16 uval16;
    epicsInt32 sval32;
    epicsUInt32 uval32;
    union {epicsFloat32 f; epicsUInt32 i; } val32;
    __extension__ union {epicsFloat64 f; epicsUInt64 i; } val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal, "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
            status = oplkRead(priv->station, priv->offs,
                1, &sval8);
            oplkDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, sval8);
            record->rval = sval8;
            break;
        case epicsUInt8T:
            status = oplkRead(priv->station, priv->offs,
                1, &uval8);
            oplkDebugLog(3, "ai %s: read 8bit %02x\n",
                record->name, uval8);
            record->rval = uval8;
            break;
        case epicsInt16T:
            status = oplkRead(priv->station, priv->offs,
                2, &sval16);
            oplkDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, sval16);
            record->rval = sval16;
            break;
        case epicsUInt16T:
            status = oplkRead(priv->station, priv->offs,
                2, &uval16);
            oplkDebugLog(3, "ai %s: read 16bit %04x\n",
                record->name, uval16);
            record->rval = uval16;
            break;
        case epicsInt32T:
            status = oplkRead(priv->station, priv->offs,
                4, &sval32);
            oplkDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, sval32);
            record->rval = sval32;
            break;
        case epicsUInt32T:
            status = oplkRead(priv->station, priv->offs,
                4, &uval32);
            oplkDebugLog(3, "ai %s: read 32bit %04x\n",
                record->name, uval32);
            record->rval = uval32;
            break;
        case epicsFloat32T:
            status = oplkRead(priv->station, priv->offs,
                4, &val32);
            oplkDebugLog(3, "ai %s: read 32bit %04x = %g\n",
                record->name, val32.i, val32.f);
            val64.f = val32.f;
            floatval = TRUE;
            break;
        case epicsFloat64T:
            status = oplkRead(priv->station, priv->offs,
                8, &val64);
            __extension__ oplkDebugLog(3, "ai %s: read 64bit " CONV64 " = %g\n",
                record->name, val64.i, val64.f);
            floatval = TRUE;
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        errlogSevPrintf(errlogFatal,
            "%s: read error\n", record->name);
        recGblSetSevr(record, READ_ALARM, INVALID_ALARM);
        return status;
    }
    if (floatval)
    {
        /* emulate scaling */
        if (record->aslo != 0.0) val64.f *= record->aslo;
        val64.f += record->aoff;
        if (record->udf)
            record->val = val64.f;
        else
            /* emulate smoothing */
            record->val = record->val * record->smoo +
                val64.f * (1.0 - record->smoo);
        record->udf = isnan(record->val);
        return 2;
    }
    return 0;
}

STATIC long oplkSpecialLinconvAi(aiRecord *record, int after)
{
    epicsUInt32 hwSpan;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff =
            (priv->hwHigh*record->egul - priv->hwLow*record->eguf)
            / hwSpan;
    }
    return 0;
}               


/* ao ***************************************************************/

STATIC long oplkInitRecordAo(aoRecord *record)
{
    oplkmemPrivate_t *priv;
    int status;

   // epicsAtExit(shutdownApp, NULL);
    if (record->out.type != INST_IO) {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordAo: illegal OUT field");
        return S_db_badField;
    }
    priv = (oplkmemPrivate_t *)callocMustSucceed(1,
        sizeof(oplkmemPrivate_t), "oplkInitRecordAo");
    status = oplkIoParse(record->name,
        record->out.value.instio.string, priv);
    if (status)
    {
        recGblRecordError(S_db_badField, record,
            "oplkInitRecordAo: bad OUT field");
        return S_db_badField;
    }
    assert(priv->station);
    switch (priv->dtype)
    {
        case epicsInt8T:
        case epicsUInt8T:
        case epicsInt16T:
        case epicsUInt16T:
        case epicsInt32T:
        case epicsUInt32T:
        case epicsFloat32T:
        case epicsFloat64T:
            break;
        default:
            errlogSevPrintf(errlogFatal,
                "oplkInitRecordAo %s: illegal data type\n",
                record->name);
            return S_db_badField;
    }
    record->dpvt = priv;
    oplkSpecialLinconvAo(record, TRUE);
    return 2; /* preserve whatever is in the VAL field */
}

STATIC long oplkWriteAo(aoRecord *record)
{
    int status;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *)record->dpvt;
    epicsUInt8 rval8;
    epicsUInt16 rval16;
    epicsUInt32 rval32;
    union {epicsFloat32 f; epicsUInt32 i; } val32;
    __extension__ union {epicsFloat64 f; epicsUInt64 i; } val64;

    if (!priv)
    {
        recGblSetSevr(record, UDF_ALARM, INVALID_ALARM);
        errlogSevPrintf(errlogFatal,
            "%s: not initialized\n", record->name);
        return -1;
    }
    assert(priv->station);
    rval32 = record->rval;
    switch (priv->dtype)
    {
        case epicsInt8T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            oplkDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            //printf("rval8 = %d\n",rval8);
            status = oplkWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case epicsUInt8T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval8 = rval32;
            oplkDebugLog(2, "ao %s: write 8bit %02x\n",
                record->name, rval8 & 0xff);
            status = oplkWrite(priv->station, priv->offs,
                1, &rval8);
            break;
        case epicsInt16T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            oplkDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = oplkWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case epicsUInt16T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            rval16 = rval32;
            oplkDebugLog(2, "ao %s: write 16bit %04x\n",
                record->name, rval16 & 0xffff);
            status = oplkWrite(priv->station, priv->offs,
                2, &rval16);
            break;
        case epicsInt32T:
            if (record->rval > priv->hwHigh) rval32 = priv->hwHigh;
            if (record->rval < priv->hwLow) rval32 = priv->hwLow;
            oplkDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = oplkWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        case epicsUInt32T:
            if (rval32 > (epicsUInt32)priv->hwHigh) rval32 = priv->hwHigh;
            if (rval32 < (epicsUInt32)priv->hwLow) rval32 = priv->hwLow;
            oplkDebugLog(2, "ao %s: write 32bit %08x\n",
                record->name, rval32);
            status = oplkWrite(priv->station, priv->offs,
                4, &rval32);
            break;
        case epicsFloat32T:
            /* emulate scaling */
            val32.f = record->oval - record->aoff;
            if (record->aslo != 0) val32.f /= record->aslo;
            oplkDebugLog(2, "ao %s: write 32bit %08x = %g\n",
                record->name, val32.i, val32.f);
            status = oplkWrite(priv->station, priv->offs,
                4, &val32);
            break;
        case epicsFloat64T:
            /* emulate scaling */
            val64.f = record->oval - record->aoff;
            if (record->aslo != 0) val64.f /= record->aslo;
            __extension__ oplkDebugLog(2, "ao %s: write 64bit " CONV64 " = %g\n",
                record->name, val64.i, val64.f);
            status = oplkWrite(priv->station, priv->offs,
                8, &val64);
            break;
        default:
            recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
            errlogSevPrintf(errlogFatal,
                "%s: unexpected data type requested\n",
                record->name);
            return -1;
    }
    if (status == S_drv_noConn)
    {
        recGblSetSevr(record, COMM_ALARM, INVALID_ALARM);
        return status;
    }
    if (status)
    {
        recGblSetSevr(record, WRITE_ALARM, INVALID_ALARM);
    }
    return status;
}

STATIC long oplkSpecialLinconvAo(aoRecord *record, int after)
{
    epicsUInt32 hwSpan;
    oplkmemPrivate_t *priv = (oplkmemPrivate_t *) record->dpvt;

    if (after) {
        hwSpan = priv->hwHigh - priv->hwLow;
        record->eslo = (record->eguf - record->egul) / hwSpan;
        record->eoff = 
            (priv->hwHigh*record->egul -priv->hwLow*record->eguf)
            / hwSpan;
    }
    return 0;
}
