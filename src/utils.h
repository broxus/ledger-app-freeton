#ifndef _UTILS_H_
#define _UTILS_H_

#include "os.h"
#include "cx.h"
#include "globals.h"

#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

uint32_t readUint32BE(uint8_t *buffer);
uint64_t readUint64BE(uint8_t *buffer);

void get_public_key(uint32_t accountNumber, uint8_t* publicKeyArray);
void get_private_key(uint32_t accountNumber, cx_ecfp_private_key_t *privateKey);

int  print_amount(uint64_t amount, char *out, size_t out_length);
int print_token_amount(
        uint64_t amount,
        const char *asset,
        uint8_t decimals,
        char *out,
        size_t out_length
);

void print_public_key(const uint8_t *in, char *out, uint8_t len);

void print_address(const uint8_t *in, char *out, uint8_t len);
void print_address_short(int8_t dst_workchain_id, const uint8_t *in, char *out, uint8_t len);

uint8_t leading_zeros(uint16_t value);
void send_response(uint8_t tx, bool approve);
unsigned int ui_prepro(const bagl_element_t *element);

#define BAIL_IF(x) {int err = x; if (err) return err;}

#define VALIDATE(cond, error) \
    do {\
        if (!(cond)) { \
            PRINTF("Validation Error in %s: %d\n", __FILE__, __LINE__); \
            THROW(error); \
        } \
    } while(0)

#define PRINT_LINE() PRINTF("Print line %s: %d\n", __FILE__, __LINE__)

#endif
