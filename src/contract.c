#include "contract.h"
#include "slice_data.h"
#include "byte_stream.h"
#include "cell.h"
#include "utils.h"
#include "errors.h"
#include "hashmap_label.h"

void deserialize_cells_tree(struct ByteStream_t* src) {
    {
        uint64_t magic = ByteStream_read_u32(src);
        VALIDATE(BOC_GENERIC_TAG == magic, ERR_INVALID_DATA);
    }

    uint8_t first_byte = ByteStream_read_byte(src);
    {
        bool index_included = (first_byte & 0x80) != 0;
        bool has_crc = (first_byte & 0x40) != 0;
        bool has_cache_bits = (first_byte & 0x20) != 0;
        uint8_t flags = (first_byte & 0x18) >> 3;
        UNUSED(flags);
        VALIDATE(!index_included && !has_crc && !has_cache_bits, ERR_INVALID_DATA);
    }

    uint8_t ref_size = first_byte & 0x7; // size in bytes
    VALIDATE(ref_size == 1, ERR_INVALID_DATA);

    uint8_t offset_size = ByteStream_read_byte(src);
    VALIDATE(offset_size != 0 && offset_size <= 8, ERR_INVALID_DATA);

    ByteStream_read_byte(src);   // move cursor
    uint8_t cells_count = contract_context.wallet_cells_count; // use only N first of cells

    uint8_t roots_count = ByteStream_read_byte(src);
    VALIDATE(roots_count == MAX_ROOTS_COUNT, ERR_INVALID_DATA);
    VALIDATE(cells_count <= MAX_CONTRACT_CELLS_COUNT, ERR_INVALID_DATA);
    boc_context.cells_count = cells_count;

    {
        uint8_t absent_count = ByteStream_read_byte(src);
        UNUSED(absent_count);
        uint8_t* total_cells_size = ByteStream_read_data(src, offset_size);
        UNUSED(total_cells_size);
        uint8_t* buf = ByteStream_read_data(src, roots_count * ref_size);
        UNUSED(buf);
    }

    Cell_t cell;
    for (uint8_t i = 0; i < cells_count; ++i) {
        uint8_t* cell_begin = ByteStream_get_cursor(src);
        Cell_init(&cell, cell_begin);
        uint16_t offset = deserialize_cell(&cell, i, cells_count);
        boc_context.cells[i] = cell;
        ByteStream_read_data(src, offset);
    }
}

void find_public_key_cell() {
    BocContext_t* bc = &boc_context;
    VALIDATE(Cell_get_data(&bc->cells[0])[0] & 0x20, ERR_INVALID_DATA); // has data branch

    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(&bc->cells[0], &refs_count);
    VALIDATE(refs_count > 0 && refs_count <= 2, ERR_INVALID_DATA);

    uint8_t data_root = refs[refs_count - 1];
    VALIDATE(data_root != 0 && data_root <= MAX_CONTRACT_CELLS_COUNT, ERR_INVALID_DATA);
    refs = Cell_get_refs(&bc->cells[data_root], &refs_count);
    VALIDATE(refs_count != 0 && refs_count <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);

    uint8_t key_buffer[8];
    SliceData_t key;
    memset(key_buffer, 0, sizeof(key_buffer));
    SliceData_init(&key, key_buffer, sizeof(key_buffer));

    uint16_t bit_len = SliceData_remaining_bits(&key);
    put_to_node(refs[0], bit_len, &key);
}

void get_address(const uint32_t account_number, const uint32_t contract_number, uint8_t* address) {
    switch (contract_number) {
        case SAFE_MULTISIG_WALLET:
            safe_multisig_init();
            compute_address(account_number, address);
            break;
        case SAFE_MULTISIG_24H_WALLET:
            safe_multisig_24h_init();
            compute_address(account_number, address);
            break;
        case SETCODE_MULTISIG_WALLET:
            setcode_multisig_init();
            compute_address(account_number, address);
            break;
        case SURF_WALLET:
            surf_init();
            compute_address(account_number, address);
            break;
        case WALLET_V3:
            compute_address_wallet_v3(account_number, address);
            break;
        default:
            THROW(ERR_INVALID_CONTRACT);
    }
}

void compute_address(const uint32_t account_number, uint8_t* address) {
    BocContext_t* bc = &boc_context;
    ContractContext_t* cc = &contract_context;

    {
        ByteStream_t src;
        ByteStream_init(&src, (uint8_t*)cc->wallet, cc->wallet_size);
        deserialize_cells_tree(&src);
    }

    VALIDATE(bc->cells_count != 0, ERR_INVALID_DATA);
    find_public_key_cell(); // sets public key cell index to boc_context

    VALIDATE(bc->public_key_cell_index && bc->public_key_label_size_bits, ERR_CELL_IS_EMPTY);
    Cell_t* cell = &bc->cells[bc->public_key_cell_index];
    uint8_t cell_data_size = Cell_get_data_size(cell);
    VALIDATE(cell_data_size != 0 && cell_data_size <= MAX_PUBLIC_KEY_CELL_DATA_SIZE, ERR_INVALID_DATA);
    uint8_t* cell_data = Cell_get_data(cell);

    memcpy(bc->public_key_cell_data, cell_data, cell_data_size);
    uint8_t* public_key = data_context.pk_context.public_key;
    get_public_key(account_number, public_key);

    uint8_t* data = bc->public_key_cell_data;
    SliceData_t slice;
    SliceData_init(&slice, data, sizeof(bc->public_key_cell_data));
    SliceData_move_by(&slice, bc->public_key_label_size_bits);
    SliceData_append(&slice, public_key, PUBKEY_LENGTH * 8, true);

    for (int16_t i = bc->cells_count - 1; i >= 1; --i) {
        Cell_t* cell = &bc->cells[i];
        calc_cell_hash(cell, i);
    }

    calc_root_cell_hash(&bc->cells[0]);

    memcpy(address, bc->hashes, HASH_LENGTH);
}
