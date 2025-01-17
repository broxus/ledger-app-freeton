#include <stdint.h>

#include "contract.h"
#include "globals.h"

const uint8_t safe_multisig_code_hash[] = { 0x80, 0xd6, 0xc4, 0x7c, 0x4a, 0x25, 0x54, 0x3c,
                                            0x9b, 0x39, 0x7b, 0x71, 0x71, 0x6f, 0x3f, 0xae,
                                            0x1e, 0x2c, 0x5d, 0x24, 0x71, 0x74, 0xc5, 0x2e,
                                            0x2c, 0x19, 0xbd, 0x89, 0x64, 0x42, 0xb1, 0x05 };

const uint8_t safe_multisig_wallet[] = { 0xB5, 0xEE, 0x9C, 0x72, 0x01, 0x02, 0x49, 0x01, 0x00, 0x10, 0xF4, 0x00,
                                         0x02, 0x01, 0x34, 0x06, 0x01, 0x01, 0x01, 0xC0, 0x02, 0x02, 0x03, 0xCF,
                                         0x20, 0x05, 0x03, 0x01, 0x01, 0xDE, 0x04, 0x00, 0x03, 0xD0, 0x20, 0x00,
                                         0x41, 0xD8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04 };

const uint8_t safe_multisig_wallet_cells_count = 6;
const uint8_t safe_multisig_wallet_code_child_depth = 0x0c;
const uint8_t safe_multisig_wallet_data_child_depth = 0x03;

void safe_multisig_init() {
    contract_context.code_hash = safe_multisig_code_hash;
    contract_context.wallet = safe_multisig_wallet;
    contract_context.wallet_size = sizeof(safe_multisig_wallet);
    contract_context.wallet_cells_count = safe_multisig_wallet_cells_count;
    contract_context.wallet_code_child_depth = safe_multisig_wallet_code_child_depth;
    contract_context.wallet_data_child_depth = safe_multisig_wallet_data_child_depth;
}
