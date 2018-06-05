#!../../bin/linux-x86_64/powerlink


< envPaths

cd ${TOP}

# Load drivers and dbd files ==============

## Register all support components
dbLoadDatabase "dbd/powerlink.dbd"
powerlink_registerRecordDeviceDriver pdbbase

#var oplkDebug <level>
#level=-1:  no output
#level=0:   errors only
#level=1:   startup messages
#level=2: + output record processing
#level=3: + inputput record processing
#level=4: + driver calls
#level=5: + io printout
#be careful using level>1 since many messages may introduce delays

oplkcnConfigure testcn:1,1,1
#oplkcnstationConfigure testcn:2,1,1,1
#oplkcnstationConfigure testcn:3,1,1,1
#oplkcnstationConfigure testcn:4,1,1,1
#oplkcnstationConfigure testcn:5,1,1,1
#oplkcnstationConfigure testcn:6,1,1,1
#oplkcnstationConfigure testcn:7,1,1,1
#oplkcnstationConfigure testcn:8,1,1,1
#selectNIC test
#oplkMain test
## Load record instances
#dbLoadTemplate "db/userHost.substitutions"
#dbLoadRecords "db/test1.db","oplkcn-name=testcn:1"
#dbLoadRecords "db/test2.db","oplkcn-name=testcn:2"

dbLoadTemplate "db/oplk.template"
## Set this to see messages from mySub
#var mySubDebug 1

## Run this to trace the stages of iocInit
#traceIocInit

cd ${TOP}/iocBoot/${IOC}
iocInit

#var oplkDebug 1

## Start any sequence programs
#seq sncExample, "user=kanHost"
dbl
