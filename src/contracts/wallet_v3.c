#include <stdint.h>

#include "contract.h"
#include "globals.h"
#include "utils.h"

const uint8_t wallet_v3_code_hash[] = { 0x84, 0xDA, 0xFA, 0x44, 0x9F, 0x98, 0xA6, 0x98,
                                        0x77, 0x89, 0xBA, 0x23, 0x23, 0x58, 0x07, 0x2B,
                                        0xC0, 0xF7, 0x6D, 0xC4, 0x52, 0x40, 0x02, 0xA5,
                                        0xD0, 0x91, 0x8B, 0x9A, 0x75, 0xD2, 0xD5, 0x99 };

const uint8_t wallet_v3_id[] = { 0x4B, 0xA9, 0x2D, 0x8A };

void compute_address_wallet_v3(const uint32_t account_number, uint8_t* address) {
    uint8_t data_hash[HASH_SIZE];

    // compute data hash
    {
        uint8_t hash_buffer[42]; // d1(1) + d2(1) + data(8) + pubkey(32)

        uint16_t hash_buffer_offset = 0;
        hash_buffer[0] = 0x00; // d1(1)
        hash_buffer[1] = 0x50; // d2(1)
        hash_buffer_offset += 2;

        // data
        hash_buffer[2] = 0x00;
        hash_buffer[3] = 0x00;
        hash_buffer[4] = 0x00;
        hash_buffer[5] = 0x00;
        hash_buffer_offset += 4;

        // data
        memcpy(hash_buffer + hash_buffer_offset, wallet_v3_id, sizeof(wallet_v3_id));
        hash_buffer_offset += sizeof(wallet_v3_id);

        uint8_t public_key[PUBLIC_KEY_LENGTH];
        get_public_key(account_number, public_key);

        memcpy(hash_buffer + hash_buffer_offset, public_key, PUBLIC_KEY_LENGTH);
        hash_buffer_offset += PUBLIC_KEY_LENGTH;

        int result = cx_hash_sha256(hash_buffer, hash_buffer_offset, data_hash, HASH_SIZE);
        VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
    }

    // compute address
    {
        uint8_t hash_buffer[71]; // d1(1) + d2(1) + data(5) + code_hash(32) + data_hash(32)

        uint16_t hash_buffer_offset = 0;
        hash_buffer[0] = 0x02; // d1(1)
        hash_buffer[1] = 0x01; // d2(1)
        hash_buffer_offset += 2;

        hash_buffer[2] = 0x34; // data
        hash_buffer_offset += 1;

        hash_buffer[3] = 0x00;
        hash_buffer[4] = 0x00; // code hash child depth
        hash_buffer[5] = 0x00;
        hash_buffer[6] = 0x00; // data hash child depth
        hash_buffer_offset += 4;

        // append code hash
        memcpy(hash_buffer + hash_buffer_offset, wallet_v3_code_hash, HASH_SIZE);
        hash_buffer_offset += HASH_SIZE;

        // append code hash
        memcpy(hash_buffer + hash_buffer_offset, data_hash, HASH_SIZE);
        hash_buffer_offset += HASH_SIZE;

        int result = cx_hash_sha256(hash_buffer, hash_buffer_offset, address, HASH_SIZE);
        VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
    }
}
