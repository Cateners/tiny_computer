
- Open MCUEXPRESSO and set the workspace to wolfssl/IDE/MCUEXPRESSO
- File -> Open Projects From File System... -> Directory : and set the browse to wolfssl/IDE/MCUEXPROSSO directory then click "select directory"
- Select MCUEXPRESSO\wolfssl, MCUEXPRESSO\benchmark and MCUEXPRESSO\wolfcrypt_test then click "Finish"
- Right click the projects -> SDK Management -> Refresh SDK Components and click "yes"
- MCUEXPRESSO fails to generate the fils for wolfssl/MIMXRT685S, just copy the files from either benchmark or wolfcrypt_test into the directory
- increase the size of configTOTAL_HEAP_SIZE in FreeRTOSConfig.h to be 200000 for wolfcrypt_test and benchmark projects
- (note board files need to be recreated .... this can be done by creating a new project that has the same settings and copying over the generated board/* files)
- Build the projects

