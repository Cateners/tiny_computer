#ifndef _WS_DECODE_H_
#define _WS_DECODE_H_

#include <stdint.h>
#include <rfb/rfb.h>

#if defined(__APPLE__)

#include <libkern/OSByteOrder.h>
#define WS_NTOH64(n) OSSwapBigToHostInt64(n)
#define WS_NTOH32(n) OSSwapBigToHostInt32(n)
#define WS_NTOH16(n) OSSwapBigToHostInt16(n)
#define WS_HTON64(n) OSSwapHostToBigInt64(n)
#define WS_HTON16(n) OSSwapHostToBigInt16(n)

#else

#ifdef LIBVNCSERVER_HAVE_ENDIAN_H
#include <endian.h>
#elif LIBVNCSERVER_HAVE_SYS_ENDIAN_H
#include <sys/endian.h>
#endif

#define WS_NTOH64(n) htobe64(n)
#define WS_NTOH32(n) htobe32(n)
#define WS_NTOH16(n) htobe16(n)
#define WS_HTON64(n) htobe64(n)
#define WS_HTON16(n) htobe16(n)

#endif

#define B64LEN(__x) (((__x + 2) / 3) * 12 / 3)
#define WSHLENMAX 14LL  /* 2 + sizeof(uint64_t) + sizeof(uint32_t) */
#define WS_HYBI_MASK_LEN 4

#define ARRAYSIZE(a) ((sizeof(a) / sizeof((a[0]))) / (size_t)(!(sizeof(a) % sizeof((a[0])))))

struct ws_ctx_s;
typedef struct ws_ctx_s ws_ctx_t;

typedef int (*wsEncodeFunc)(rfbClientPtr cl, const char *src, int len, char **dst);
typedef int (*wsDecodeFunc)(ws_ctx_t *wsctx, char *dst, int len);

typedef int (*wsReadFunc)(void *ctx, char *dst, size_t len);

typedef struct ctxInfo_s{
  void *ctxPtr;
  wsReadFunc readFunc;
} ctxInfo_t;

enum {
  /* header not yet received completely */
  WS_HYBI_STATE_HEADER_PENDING,
  /* data available */
  WS_HYBI_STATE_DATA_AVAILABLE,
  WS_HYBI_STATE_DATA_NEEDED,
  /* received a complete frame */
  WS_HYBI_STATE_FRAME_COMPLETE,
  /* received part of a 'close' frame */
  WS_HYBI_STATE_CLOSE_REASON_PENDING,
  /* */
  WS_HYBI_STATE_ERR
};

typedef union ws_mask_s {
  char c[4];
  uint32_t u;
} ws_mask_t;

/* XXX: The union and the structs do not need to be named.
 *      We are working around a bug present in GCC < 4.6 which prevented
 *      it from recognizing anonymous structs and unions.
 *      See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=4784
 */
typedef struct 
#if __GNUC__
__attribute__ ((__packed__)) 
#endif
ws_header_s {
  unsigned char b0;
  unsigned char b1;
  union {
    struct 
#if __GNUC__
    __attribute__ ((__packed__)) 
#endif
           {
      uint16_t l16;
      ws_mask_t m16;
    } s16;
    struct
#if __GNUC__
__attribute__ ((__packed__)) 
#endif
           {
      uint64_t l64;
      ws_mask_t m64;
    } s64;
    ws_mask_t m;
  } u;
} ws_header_t;

typedef struct ws_header_data_s {
  ws_header_t *data;
  /** bytes read */
  int nRead;
  /** mask value */
  ws_mask_t mask;
  /** length of frame header including payload len, but without mask */
  int headerLen;
  /** length of the payload data */
  uint64_t payloadLen;
  /** opcode */
  unsigned char opcode;
  /** fin bit */
  unsigned char fin;
} ws_header_data_t;

struct ws_ctx_s {
    char codeBufDecode[2048 + WSHLENMAX]; /* base64 + maximum frame header length */
    char codeBufEncode[B64LEN(UPDATE_BUF_SIZE) + WSHLENMAX]; /* base64 + maximum frame header length */
    char *writePos;
    unsigned char *readPos;
    int readlen;
    int hybiDecodeState;
    char carryBuf[3];                      /* For base64 carry-over */
    int carrylen;
    int base64;
    ws_header_data_t header;
    uint64_t nReadPayload;
    unsigned char continuation_opcode;
    wsEncodeFunc encode;
    wsDecodeFunc decode;
    ctxInfo_t ctxInfo;
};

enum
{
    WS_OPCODE_CONTINUATION = 0x00,
    WS_OPCODE_TEXT_FRAME = 0x01,
    WS_OPCODE_BINARY_FRAME = 0x02,
    WS_OPCODE_CLOSE = 0x08,
    WS_OPCODE_PING = 0x09,
    WS_OPCODE_PONG = 0x0A,
    WS_OPCODE_INVALID = 0xFF
};

int webSocketsDecodeHybi(ws_ctx_t *wsctx, char *dst, int len);

void hybiDecodeCleanupComplete(ws_ctx_t *wsctx);
#endif
