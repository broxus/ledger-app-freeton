#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include <stdint.h>

struct ByteStream_t;
void prepare_to_sign(struct ByteStream_t* src, uint32_t wallet_type);

#endif
