# QNX CAAM Driver And Examples

This directory contains;
- A Makefile for creating the QNX CAAM driver located at IDE/QNX/CAAM-DRIVER/Makefile
- An example TLS server located at IDE/QNX/example-server/
- An example client located at IDE/QNX/example-client
- An example CMAC use located at IDE/QNX/example-cmac

To build either of these, first build wolfSSL with support for use with QNX CAAM. To do this use the configure option --enable-caam=qnx

```
bash
source ~/qnx700/qnxsdp-env.sh
./configure --host=arm-unknown-nto-qnx7.0.0eabi --enable-caam=qnx
make
```

Once the wolfSSL library has been built cd to IDE/QNX/CAAM-DRIVER and run "make". This will produce the wolfCrypt resource manager. It should be started on the device with root permisions. Once wolfCrypt is running on the device with root permisions then any user with access to open a connection to wolfCrypt can make use of the driver.  


### Supported Operations By CAAM Driver
- ECC black key creation
- ECC black key sign / verify / ecdh
- Black blob creation and open
- Red blob creation and open
- Cover keys (turn to black key)
- CMAC with and without black keys
- TRNG used by default to seed Hash DRBG
