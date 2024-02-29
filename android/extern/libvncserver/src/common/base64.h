#ifndef _BASE64_H
#define _BASE64_H

extern int __b64_ntop(u_char const *src, size_t srclength, char *target, size_t targsize);
extern int __b64_pton(char const *src, u_char *target, size_t targsize);

#define rfbBase64NtoP __b64_ntop
#define rfbBase64PtoN __b64_pton

#endif /* _BASE64_H */
