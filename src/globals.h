#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "os.h"
#include "ux.h"
#include "os_io_seproxyhal.h"
#include "errors.h"
#include "cell.h"

#define P1_CONFIRM 0x01
#define P1_NON_CONFIRM 0x00
#define P2_CONFIRM 0x01
#define P2_NON_CONFIRM 0x00
#define P1_FIRST 0x00
#define P1_MORE 0x80
#define P1_LAST 0x81

#define PUBLIC_KEY_LENGTH 32
#define ADDRESS_LENGTH 32
#define BIP32_PATH 5
#define TO_SIGN_LENGTH 32
#define SIGN_LENGTH 64

extern ux_state_t ux;
// display stepped screens
extern unsigned int ux_step;
extern unsigned int ux_step_count;

typedef enum {
    APP_STATE_IDLE,
    APP_STATE_GETTING_ADDRESS
} AppState_t;

extern AppState_t app_state;

#ifdef CUSTOM_TVC_BUFFER_SIZE
#define TVC_BUFFER_SIZE CUSTOM_TVC_BUFFER_SIZE
#else
#define TVC_BUFFER_SIZE 10048 // 64 byte aligned
#endif

extern unsigned char N_buffer[TVC_BUFFER_SIZE];
#define N_tvcStorage ((volatile unsigned char*) PIC(N_buffer))

#define MAX_CELLS_COUNT 128
#define HASHES_BUFFER_SIZE (MAX_CELLS_COUNT * 32)

extern unsigned char N_hashes[HASHES_BUFFER_SIZE];
#define N_hashesStorage ((volatile unsigned char*) PIC(N_hashes))

void reset_app_context(void);

#define MAX_PUBLIC_KEY_CELL_DATA_SIZE 36 // label(3) + public key(32) + tag(1)

typedef struct ContractContext_t {
    Cell_t cells[MAX_CELLS_COUNT];
    uint8_t cell_depth[MAX_CELLS_COUNT];
    uint8_t public_key_cell_data[MAX_PUBLIC_KEY_CELL_DATA_SIZE];
    uint16_t offset;
    uint16_t tvc_length;
    uint8_t cells_count;
    uint8_t public_key_cell_index;
    uint8_t public_key_label_size_bits;
} ContractContext_t;

typedef struct AddressContext_t {
    uint8_t address[ADDRESS_LENGTH];
    char address_str[65];
} AddressContext_t;

typedef struct PublicKeyContext_t {
    uint8_t public_key[PUBLIC_KEY_LENGTH];
    char public_key_str[65];
} PublicKeyContext_t;

typedef struct SignContext_t {
    uint8_t to_sign[TO_SIGN_LENGTH];
    uint8_t hash[SIGN_LENGTH];
    uint32_t account_number;
    char to_sign_str[65];
} SignContext_t;

typedef union {
    PublicKeyContext_t pk_context;
    AddressContext_t addr_context;
    SignContext_t sign_context;
} DataContext_t;

extern ContractContext_t contract_context;
extern DataContext_t data_context;

#endif
