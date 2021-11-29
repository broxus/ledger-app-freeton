#include "os.h"
#include "cx.h"
#include "utils.h"
#include "menu.h"

#include <stdlib.h>

#define AMOUNT_MAX_SIZE 21

static const char hexAlphabet[] = "0123456789ABCDEF";

static cx_ecfp_public_key_t publicKey;
void get_public_key(uint32_t account_number, uint8_t* publicKeyArray) {
    cx_ecfp_private_key_t privateKey;

    cx_ecfp_init_public_key(CX_CURVE_Ed25519, NULL, 0, &publicKey);
    BEGIN_TRY {
        TRY {
            get_private_key(account_number, &privateKey);
            cx_ecfp_generate_pair(CX_CURVE_Ed25519, &publicKey, &privateKey, 1);
            io_seproxyhal_io_heartbeat();
        } FINALLY {
            os_memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    for (int i = 0; i < 32; i++) {
        publicKeyArray[i] = publicKey.W[64 - i];
    }
    if ((publicKey.W[32] & 1) != 0) {
        publicKeyArray[31] |= 0x80;
    }
}

uint32_t readUint32BE(uint8_t *buffer) {
  return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3]);
}

uint64_t readUint64BE(uint8_t *buffer) {
    uint32_t i1 = buffer[3] + (buffer[2] << 8u) + (buffer[1] << 16u) + (buffer[0] << 24u);
    uint32_t i2 = buffer[7] + (buffer[6] << 8u) + (buffer[5] << 16u) + (buffer[4] << 24u);
    return i2 | ((uint64_t) i1 << 32u);
}

static const uint32_t HARDENED_OFFSET = 0x80000000;

void get_private_key(uint32_t account_number, cx_ecfp_private_key_t *privateKey) {
    const uint32_t derivePath[BIP32_PATH] = {
            44 | HARDENED_OFFSET,
            396 | HARDENED_OFFSET,
            account_number | HARDENED_OFFSET,
            0 | HARDENED_OFFSET,
            0 | HARDENED_OFFSET
    };

    uint8_t privateKeyData[32];
    BEGIN_TRY {
        TRY {
            io_seproxyhal_io_heartbeat();
            os_perso_derive_node_bip32_seed_key(HDW_ED25519_SLIP10, CX_CURVE_Ed25519, derivePath, BIP32_PATH, privateKeyData, NULL, NULL, 0);
            cx_ecfp_init_private_key(CX_CURVE_Ed25519, privateKeyData, 32, privateKey);
            io_seproxyhal_io_heartbeat();
        } FINALLY {
            os_memset(privateKeyData, 0, sizeof(privateKeyData));
        }
    }
    END_TRY;
}

void send_response(uint8_t tx, bool approve) {
    G_io_apdu_buffer[tx++] = approve? 0x90 : 0x69;
    G_io_apdu_buffer[tx++] = approve? 0x00 : 0x85;
    reset_app_context();
    // Send back the response, do not restart the event loop
    io_exchange(CHANNEL_APDU | IO_RETURN_AFTER_TX, tx);
    // Display back the original UX
    ui_idle();
}

unsigned int ui_prepro(const bagl_element_t *element) {
    unsigned int display = 1;
    if (element->component.userid > 0) {
        display = (ux_step == element->component.userid - 1);
        if (display) {
            if (element->component.userid == 1) {
                UX_CALLBACK_SET_INTERVAL(2000);
            }
            else {
                UX_CALLBACK_SET_INTERVAL(MAX(3000, 1000 + bagl_label_roundtrip_duration_ms(element, 7)));
            }
        }
    }
    return display;
}

uint8_t leading_zeros(uint16_t value) {
    uint8_t lz = 0;
    uint16_t msb = 0x8000;
    for (uint8_t i = 0; i < 16; ++i) {
        if ((value << i) & msb) {
            break;
        }
        ++lz;
    }

    return lz;
}

#define SCRATCH_SIZE 37
uint8_t convert_hex_amount_to_displayable(uint8_t* amount, uint8_t amount_length, char* out) {
    uint8_t LOOP1 = 28;
    uint8_t LOOP2 = 9;
    uint16_t scratch[SCRATCH_SIZE];
    uint8_t offset = 0;
    uint8_t nonZero = 0;
    uint8_t i;
    uint8_t targetOffset = 0;
    uint8_t workOffset;
    uint8_t j;
    uint8_t nscratch = SCRATCH_SIZE;
    uint8_t smin = nscratch - 2;
    uint8_t comma = 0;

    for (i = 0; i < SCRATCH_SIZE; i++) {
        scratch[i] = 0;
    }
    for (i = 0; i < amount_length; i++) {
        for (j = 0; j < 8; j++) {
            uint8_t k;
            uint16_t shifted_in =
                (((amount[i] & 0xff) & ((1 << (7 - j)))) != 0) ? (short)1
                                                               : (short)0;
            for (k = smin; k < nscratch; k++) {
                scratch[k] += ((scratch[k] >= 5) ? 3 : 0);
            }
            if (scratch[smin] >= 8) {
                smin -= 1;
            }
            for (k = smin; k < nscratch - 1; k++) {
                scratch[k] =
                    ((scratch[k] << 1) & 0xF) | ((scratch[k + 1] >= 8) ? 1 : 0);
            }
            scratch[nscratch - 1] = ((scratch[nscratch - 1] << 1) & 0x0F) |
                                    (shifted_in == 1 ? 1 : 0);
        }
    }

    for (i = 0; i < LOOP1; i++) {
        if (!nonZero && (scratch[offset] == 0)) {
            offset++;
        } else {
            nonZero = 1;
            out[targetOffset++] = scratch[offset++] + '0';
        }
    }
    if (targetOffset == 0) {
        out[targetOffset++] = '0';
    }
    workOffset = offset;
    for (i = 0; i < LOOP2; i++) {
        unsigned char allZero = 1;
        unsigned char j;
        for (j = i; j < LOOP2; j++) {
            if (scratch[workOffset + j] != 0) {
                allZero = 0;
                break;
            }
        }
        if (allZero) {
            break;
        }
        if (!comma) {
            out[targetOffset++] = '.';
            comma = 1;
        }
        out[targetOffset++] = scratch[offset++] + '0';
    }
    return targetOffset;
}

void print_binary(const uint8_t *in, char *out, uint8_t len) {
    out[0] = '0';
    out[1] = ':';
    uint8_t i, j;
    for (i = 0, j = 2; i < len; i += 1, j += 2) {
        out[j] = hexAlphabet[in[i] / 16];
        out[j + 1] = hexAlphabet[in[i] % 16];
    }
    out[j] = '\0';
}

void print_address(const uint8_t *in, char *out, uint8_t len) {
    out[0] = '0';
    out[1] = ':';
    if (2 + len * 2 > 18) {
        uint8_t i, j;
        for (i = 0, j = 2; i < 3; i += 1, j += 2) {
            out[j] = hexAlphabet[in[i] / 16];
            out[j + 1] = hexAlphabet[in[i] % 16];
        }
        out[j++] = '.';
        out[j++] = '.';
        for (i = len - 3; i < len; i += 1, j += 2) {
            out[j] = hexAlphabet[in[i] / 16];
            out[j + 1] = hexAlphabet[in[i] % 16];
        }
        out[j] = '\0';
    } else {
        print_binary(in, out, len);
        return;
    }
}

int print_amount(uint64_t amount, char *out, size_t out_len) {
    char buffer[AMOUNT_MAX_SIZE] = {0};
    uint64_t dVal = amount;
    int i;

    for (i = 0; dVal > 0 || i < 11; i++) {
        if (dVal > 0) {
            buffer[i] = (dVal % 10) + '0';
            dVal /= 10;
        } else {
            buffer[i] = '0';
        }
        if (i == 8) {
            i += 1;
            buffer[i] = '.';
        }
        if (i >= AMOUNT_MAX_SIZE) {
            return -1;
        }
    }

    // reverse order
    for (int j = 0; j < i / 2; j++) {
        char c = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = c;
    }

    // strip trailing 0s
    i -= 1;
    while (buffer[i] == '0') {
        buffer[i] = 0;
        i -= 1;
    }
    // strip trailing .
    if (buffer[i] == '.') buffer[i] = 0;
    strlcpy(out, buffer, out_len);

    strlcpy(buffer, "TON", AMOUNT_MAX_SIZE);
    strlcat(out, " ", out_len);
    strlcat(out, buffer, out_len);

    return 0;
}
