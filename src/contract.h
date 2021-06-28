#ifndef _CONTRACT_H_
#define _CONTRACT_H_

#include "cell.h"

#include <stdint.h>
#include <stdbool.h>

enum {
    SAFE_MULTISIG_WALLET = 0,
    SAFE_MULTISIG_24H_WALLET,
    SETCODE_MULTISIG_WALLET,
    SURF_WALLET,
    WALLET_V3,
};

struct ByteStream_t;
void deserialize_cells_tree(struct ByteStream_t* src);
void compute_address(const uint32_t account_number, uint8_t* address);
void get_address(const uint32_t account_number, const uint32_t contract_number, uint8_t* address);

void safe_multisig_init();
void safe_multisig_24h_init();
void setcode_multisig_init();
void surf_init();

void compute_address_wallet_v3(const uint32_t account_number, uint8_t* address);

#endif
