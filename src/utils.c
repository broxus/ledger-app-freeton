#include "os.h"
#include "cx.h"
#include "menu.h"
#include "utils.h"

#include <stdlib.h>

// max amount is max uint64 scaled down: "18446744073.709551615"
#define AMOUNT_MAX_SIZE 22

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
            memset(&privateKey, 0, sizeof(privateKey));
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
            memset(privateKeyData, 0, sizeof(privateKeyData));
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

void print_public_key(const uint8_t *in, char *out, uint8_t len) {
    out[0] = '0';
    out[1] = 'x';
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
    uint8_t i, j;
    for (i = 0, j = 2; i < len; i += 1, j += 2) {
        out[j] = hexAlphabet[in[i] / 16];
        out[j + 1] = hexAlphabet[in[i] % 16];
    }
    out[j] = '\0';
}

void print_address_short(int8_t dst_workchain_id, const uint8_t *in, char *out, uint8_t len) {
    uint8_t offset = 0;

    uint8_t workchain_id = (uint8_t) dst_workchain_id;
    if (dst_workchain_id < 0) {
        out[offset++] = '-';
        workchain_id = (workchain_id ^ 0xffffffffffffffff) + 1;
    }

    {
        uint8_t i = offset;
        uint8_t j = offset;

        uint8_t dVal = workchain_id;
        do {
            if (dVal > 0) {
                out[i] = (dVal % 10) + '0';
                dVal /= 10;
            } else {
                out[i] = '0';
            }
            i++;
            offset++;
        } while (dVal > 0);

        out[i--] = ':';

        for (; j < i; j++, i--) {
            int tmp = out[j];
            out[j] = out[i];
            out[i] = tmp;
        }
    }

    uint8_t i, j;
    for (i = 0, j = offset + 1; i < 3; i += 1, j += 2) {
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
}

int print_token_amount(
        uint64_t amount,
        const char *asset,
        uint8_t decimals,
        char *out,
        size_t out_length
) {
    BAIL_IF(out_length > INT_MAX);
    uint64_t dVal = amount;
    int outlen  = (int)out_length;
    int i = 0;
    int min_chars = decimals + 1;

    if (i < (outlen - 1)) {
        do {
            if (i == decimals) {
                out[i] = '.';
                i += 1;
            }
            out[i] = (dVal % 10) + '0';
            dVal /= 10;
            i += 1;
        } while ((dVal > 0 || i < min_chars) && i < outlen);
    }
    BAIL_IF(i >= outlen);
    // Reverse order
    int j, k;
    for (j = 0, k = i - 1; j < k; j++, k--) {
        char tmp = out[j];
        out[j] = out[k];
        out[k] = tmp;
    }
    // Strip trailing 0s
    for (i -= 1; i > 0; i--) {
        if (out[i] != '0') break;
    }
    i += 1;

    // Strip trailing .
    if (out[i-1] == '.') i -= 1;

    if (asset) {
        int asset_length = strlen(asset);
        // Check buffer has space
        BAIL_IF((i + 1 + asset_length + 1) > outlen);
        // Qualify amount
        out[i++] = ' ';
        strcpy(out + i, asset);
    } else {
        out[i] = '\0';
    }

    return 0;
}

#define EVER_DECIMALS 9
int print_amount(
        uint64_t amount,
        char *out,
        size_t out_length
) {
    return print_token_amount(amount, "EVER", EVER_DECIMALS, out, out_length);
}
