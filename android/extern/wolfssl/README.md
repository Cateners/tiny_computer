<a href="https://repology.org/project/wolfssl/versions">
    <img src="https://repology.org/badge/vertical-allrepos/wolfssl.svg" alt="Packaging status" align="right">
</a>

# wolfSSL Embedded SSL/TLS Library

The [wolfSSL embedded SSL library](https://www.wolfssl.com/products/wolfssl/) 
(formerly CyaSSL) is a lightweight SSL/TLS library written in ANSI C and
targeted for embedded, RTOS, and resource-constrained environments - primarily
because of its small size, speed, and feature set.  It is commonly used in
standard operating environments as well because of its royalty-free pricing
and excellent cross platform support. wolfSSL supports industry standards up
to the current [TLS 1.3](https://www.wolfssl.com/tls13) and DTLS 1.2, is up to
20 times smaller than OpenSSL, and offers progressive ciphers such as ChaCha20,
Curve25519, Blake2b and Post-Quantum TLS 1.3 groups. User benchmarking and
feedback reports dramatically better performance when using wolfSSL over
OpenSSL.

wolfSSL is powered by the wolfCrypt cryptography library. Two versions of
wolfCrypt have been FIPS 140-2 validated (Certificate #2425 and
certificate #3389). FIPS 140-3 validation is in progress. For additional
information, visit the [wolfCrypt FIPS FAQ](https://www.wolfssl.com/license/fips/)
or contact fips@wolfssl.com.

## Why Choose wolfSSL?

There are many reasons to choose wolfSSL as your embedded, desktop, mobile, or
enterprise SSL/TLS solution. Some of the top reasons include size (typical
footprint sizes range from 20-100 kB), support for the newest standards
(SSL 3.0, TLS 1.0, TLS 1.1, TLS 1.2, TLS 1.3, DTLS 1.0, and DTLS 1.2), current
and progressive cipher support (including stream ciphers), multi-platform,
royalty free, and an OpenSSL compatibility API to ease porting into existing
applications which have previously used the OpenSSL package. For a complete
feature list, see [Chapter 4](https://www.wolfssl.com/docs/wolfssl-manual/ch4/)
of the wolfSSL manual.

## Notes, Please Read

### Note 1
wolfSSL as of 3.6.6 no longer enables SSLv3 by default.  wolfSSL also no longer
supports static key cipher suites with PSK, RSA, or ECDH. This means if you
plan to use TLS cipher suites you must enable DH (DH is on by default), or
enable ECC (ECC is on by default), or you must enable static key cipher suites
with one or more of the following defines:

```
WOLFSSL_STATIC_DH
WOLFSSL_STATIC_RSA
WOLFSSL_STATIC_PSK
```
Though static key cipher suites are deprecated and will be removed from future
versions of TLS.  They also lower your security by removing PFS.

When compiling `ssl.c`, wolfSSL will now issue a compiler error if no cipher
suites are available. You can remove this error by defining
`WOLFSSL_ALLOW_NO_SUITES` in the event that you desire that, i.e., you're
not using TLS cipher suites.

### Note 2
wolfSSL takes a different approach to certificate verification than OpenSSL
does. The default policy for the client is to verify the server, this means
that if you don't load CAs to verify the server you'll get a connect error,
no signer error to confirm failure (-188).

If you want to mimic OpenSSL behavior of having `SSL_connect` succeed even if
verifying the server fails and reducing security you can do this by calling:

```c
wolfSSL_CTX_set_verify(ctx, WOLFSSL_VERIFY_NONE, NULL);
```

before calling `wolfSSL_new();`. Though it's not recommended.

### Note 3
The enum values SHA, SHA256, SHA384, SHA512 are no longer available when
wolfSSL is built with `--enable-opensslextra` (`OPENSSL_EXTRA`) or with the
macro `NO_OLD_SHA_NAMES`. These names get mapped to the OpenSSL API for a
single call hash function. Instead the name `WC_SHA`, `WC_SHA256`, `WC_SHA384` and
`WC_SHA512` should be used for the enum name.


# wolfSSL Release 5.5.4 (Dec 21, 2022)

Release 5.5.4 of wolfSSL embedded TLS has bug fixes and new features including:

## New Feature Additions

* QUIC related changes for HAProxy integration and config option
* Support for Analog Devices MAXQ1080 and MAXQ1065
* Testing and build of wolfSSL with NuttX
* New software based entropy gatherer with configure option --enable-entropy-memuse
* NXP SE050 feature expansion and fixes, adding in RSA support and conditional compile of AES and CMAC
* Support for multi-threaded sniffer

## Improvements / Optimizations

### Benchmark and Tests
* Add alternate test case for unsupported static memory API when testing mutex allocations
* Additional unit test cases added for AES CCM 256-bit
* Initialize and free AES object with benchmarking AES-OFB
* Kyber with DTLS 1.3 tests added
* Tidy up Espressif ESP32 test and benchmark examples
* Rework to be able to run API tests individually and add display of time taken per test

### Build and Port Improvements
* Add check for 64-bit ABI on MIPS64 before declaring a 64-bit CPU
* Add support to detect SIZEOF_LONG in armclang and diab
* Added in a simple example working on Rx72n
* Update azsphere support to prevent compilation of file included inline
* --enable-brainpool configure option added and default to on when custom curves are also on
* Add RSA PSS salt defines to engine builds if not FIPS v2

### Post Quantum
* Remove kyber-90s and route all Kyber through wolfcrypt
* Purge older version of NTRU and SABER from wolfSSL

### SP Math
* Support static memory build with sp-math
* SP C, SP int: improve performance
* SP int: support mingw64 again
* SP int: enhancements to guess 64-bit type and check on NO_64BIT macro set before using long long
* SP int: check size required when using sp_int on stack
* SP: --enable-sp-asm now enables SP by default if not set
* SP: support aarch64 big endian

### DTLS
* Allow DTLS 1.3 to compile when FIPS is enabled
* Allow for stateless DTLS client hello parsing

### Misc.
* Easier detection of DRBG health when using Intel’s RDRAND by updating the structures status value
* Detection of duplicate known extensions with TLS
* PKCS#11 handle a user PIN that is a NULL_PTR, compile time check in finding keys, add initialization API
* Update max Cert Policy size based on RFC 5280
* Add Android CA certs path for wolfSSL_CTX_load_system_CA_certs()
* Improve logic for enabling system CA certs on Apple devices
* Stub functions to allow for cpuid public functions with non-intel builds
* Increase RNG_SECURITY_STRENGTH for FIPS
* Improvements in OpenSSL Compat ERR Queue handling
* Support ASN1/DER CRLs in LoadCertByIssuer
* Expose more ECC math functions and improve async shared secret
* Improvement for sniffer error messages
* Warning added that renegotiation in TLS 1.3 requires session ticket
* Adjustment for TLS 1.3 post auth support
* Rework DH API and improve PEM read/write

## Fixes

### Build Fixes
* Fix --enable-devcrypto build error for sys without u_int8_t type
* Fix casts in evp.c and build issue in ParseCRL
* Fixes for compatibility layer building with heap hint and OSSL callbacks
* fix compile error due to Werro=undef on gcc-4.8
* Fix mingw-w64 build issues on windows
* Xcode project fixes for different build settings
* Initialize variable causing failures with gcc-11 and gcc-12 with a unique wolfSSL build configuration
* Prevent WOLFSSL_NO_MALLOC from breaking RSA certificate verification
* Fixes for various tests that do not properly handle `WC_PENDING_E` with async. builds
* Fix for misc `HashObject` to be excluded for `WOLFCRYPT_ONLY`

### OCSP Fixes
* Correctly save next status with OCSP response verify
* When the OCSP responder returns an unknown exception, continue through to checking the CRL

### Math Fixes
* Fix for implicit conversion with 32-bit in SP math
* Fix for error checks when modulus is even with SP int build
* Fix for checking of err in _sp_exptmod_nct with SP int build
* ECC cofactor fix when checking scalar bits
* ARM32 ASM: don't use ldrd on user data
* SP int, fix when ECC specific size code included

### Port Fixes
* Fixes for STM32 PKA ECC (not 256-bit) and improvements for AES-GCM
* Fix for cryptocell signature verification with ECC
* Benchmark devid changes, CCM with SECO fix, set IV on AES import into SECO

### Compat. Layer Fixes
* Fix for handling DEFAULT:... cipher suite list
* Fix memory leak in wolfSSL_X509_NAME_ENTRY_get_object
* Set alt name type to V_ASN1_IA5STRING
* Update name hash functions wolfSSL_X509_subject_name_hash and wolfSSL_X509_issuer_name_hash to hash the canonical form of subject
* Fix wolfSSL_set_SSL_CTX() to be usable during handshake
* Fix X509_get1_ocsp to set num of elements in stack
* X509v3 EXT d2i: fix freeing of aia
* Fix to remove recreation of certificate with wolfSSL_PEM_write_bio_X509()
* Link newly created x509 store's certificate manager to self by default to assist with CRL verification
* Fix for compatibility `EC_KEY_new_by_curve_name` to not create a key if the curve is not found

### Misc.
* Free potential signer malloc in a fail case
* fix other name san parsing and add RID cert to test parsing
* WOLFSSL_OP_NO_TICKET fix for TLSv1.2
* fix ASN template parsing of X509 subject directory attribute
* Fix the wrong IV size with the cipher suite TLS_ECDHE_PSK_WITH_AES_128_GCM_SHA256
* Fix incorrect self signed error return when compiled with certreq and certgen.
* Fix wrong function name in debug comment with wolfSSL_X509_get_name_oneline()
* Fix for decryption after second handshake with async sniffer
* Allow session tickets to properly resume when using PQ KEMs
* Add sanity overflow check to DecodeAltNames input buffer access

For additional vulnerability information visit the vulnerability page at:
https://www.wolfssl.com/docs/security-vulnerabilities/

See INSTALL file for build instructions.
More info can be found on-line at: https://wolfssl.com/wolfSSL/Docs.html


# Resources

[wolfSSL Website](https://www.wolfssl.com/)

[wolfSSL Wiki](https://github.com/wolfSSL/wolfssl/wiki)

[FIPS 140-2/140-3 FAQ](https://wolfssl.com/license/fips)

[wolfSSL Documentation](https://wolfssl.com/wolfSSL/Docs.html)

[wolfSSL Manual](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-toc.html)

[wolfSSL API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-17-wolfssl-api-reference.html)

[wolfCrypt API Reference](https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-18-wolfcrypt-api-reference.html)

[TLS 1.3](https://www.wolfssl.com/docs/tls13/)

[wolfSSL Vulnerabilities](https://www.wolfssl.com/docs/security-vulnerabilities/)

[Additional wolfSSL Examples](https://github.com/wolfssl/wolfssl-examples)
