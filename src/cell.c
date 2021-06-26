#include "cell.h"
#include "utils.h"
#include "errors.h"

#include <stdio.h>

void Cell_init(struct Cell_t* self, uint8_t* cell_begin) {
    VALIDATE(self && cell_begin, ERR_CELL_IS_EMPTY);
    self->cell_begin = cell_begin;
}

uint8_t Cell_get_d1(const struct Cell_t* self) {
    VALIDATE(self && self->cell_begin, ERR_CELL_IS_EMPTY);
    return self->cell_begin[0];
}

uint8_t Cell_get_d2(const struct Cell_t* self) {
    VALIDATE(self && self->cell_begin, ERR_CELL_IS_EMPTY);
    return self->cell_begin[1];
}

uint8_t Cell_get_data_size(const struct Cell_t* self) {
    uint8_t d2 = Cell_get_d2(self);
    return (d2 >> 1) + (((d2 & 1) != 0) ? 1 : 0);
}

uint8_t* Cell_get_data(const struct Cell_t* self) {
    VALIDATE(self && self->cell_begin, ERR_CELL_IS_EMPTY);
    return self->cell_begin + CELL_DATA_OFFSET;
}

uint8_t* Cell_get_refs(const struct Cell_t* self, uint8_t* refs_count) {
    uint8_t d1 = Cell_get_d1(self);
    *refs_count = d1 & 7;
    uint8_t data_size = Cell_get_data_size(self);
    return self->cell_begin + CELL_DATA_OFFSET + data_size;
}

uint16_t deserialize_cell(struct Cell_t* cell, const uint8_t cell_index, const uint8_t cells_count) {
    uint8_t d1 = Cell_get_d1(cell);
    uint8_t l = d1 >> 5; // level
    uint8_t h = (d1 & 16) == 16; // with hashes
    uint8_t s = (d1 & 8) == 8; // exotic
    uint8_t r = d1 & 7;	// refs count
    uint8_t absent = r == 7 && h;
    UNUSED(l);
    UNUSED(absent);

    VALIDATE(!h, ERR_INVALID_DATA);
    VALIDATE(!s, ERR_INVALID_DATA); // only ordinary cells are valid
    VALIDATE(r <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);

    uint8_t data_size = Cell_get_data_size(cell);
    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(cell, &refs_count);
    for (uint8_t i = 0; i < refs_count; ++i) {
        uint8_t ref = refs[i];
        VALIDATE(ref <= cells_count && ref > cell_index, ERR_INVALID_DATA);
    }

    return CELL_DATA_OFFSET + data_size + refs_count; // cell size
}

void calc_cell_hash(Cell_t* cell, const uint8_t cell_index) {
    uint8_t hash_buffer[262]; // d1(1) + d2(1) + data(128) + 4 * (depth(1) + hash(32))

    uint16_t hash_buffer_offset = 0;
    hash_buffer[0] = Cell_get_d1(cell);
    hash_buffer[1] = Cell_get_d2(cell);
    hash_buffer_offset += 2;
    uint8_t data_size = Cell_get_data_size(cell);
    BocContext_t* bc = &boc_context;
    if (bc->public_key_cell_index && cell_index == bc->public_key_cell_index) {
        os_memcpy(hash_buffer + hash_buffer_offset, bc->public_key_cell_data, data_size);
    } else {
        os_memcpy(hash_buffer + hash_buffer_offset, Cell_get_data(cell), data_size);
    }
    hash_buffer_offset += data_size;

    uint8_t refs_count = 0;
    uint8_t* refs = Cell_get_refs(cell, &refs_count);
    VALIDATE(refs_count >= 0 && refs_count <= MAX_REFERENCES_COUNT, ERR_INVALID_DATA);
    for (uint8_t child = 0; child < refs_count; ++child) {
        uint8_t* depth = &bc->cell_depth[cell_index];
        uint8_t child_depth = bc->cell_depth[refs[child]];
        *depth = (*depth > child_depth + 1) ? *depth : (child_depth + 1);
        uint8_t buf[2];
        buf[0] = 0;
        buf[1] = child_depth;
        os_memcpy(hash_buffer + hash_buffer_offset, buf, sizeof(buf));
        hash_buffer_offset += sizeof(buf);
    }
    
    for (uint8_t child = 0; child < refs_count; ++child) {
        uint8_t* cell_hash = bc->hashes + refs[child] * HASH_SIZE;
        os_memcpy(hash_buffer + hash_buffer_offset, cell_hash, HASH_SIZE);
        hash_buffer_offset += HASH_SIZE;
    }
    
    int result = cx_hash_sha256(hash_buffer, hash_buffer_offset, bc->hashes + cell_index * HASH_SIZE, HASH_SIZE);
    VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
}

void calc_root_cell_hash(Cell_t* cell) {
    BocContext_t* bc = &boc_context;
    ContractContext_t* cc = &contract_context;

    uint8_t hash_buffer[262]; // d1(1) + d2(1) + data(128) + 4 * (depth(1) + hash(32))

    uint16_t hash_buffer_offset = 0;
    hash_buffer[0] = Cell_get_d1(cell);
    hash_buffer[1] = Cell_get_d2(cell);
    hash_buffer_offset += 2;

    uint8_t data_size = Cell_get_data_size(cell);
    os_memcpy(hash_buffer + hash_buffer_offset, Cell_get_data(cell), data_size);
    hash_buffer_offset += data_size;

    // code hash child
    hash_buffer[hash_buffer_offset] = 0x00;
    hash_buffer[hash_buffer_offset + 1] = cc->wallet_code_child_depth;
    hash_buffer_offset += 2;

    // data hash child
    hash_buffer[hash_buffer_offset] = 0x00;
    hash_buffer[hash_buffer_offset + 1] = cc->wallet_data_child_depth;
    hash_buffer_offset += 2;

    // append code hash
    os_memcpy(hash_buffer + hash_buffer_offset, cc->code_hash, HASH_SIZE);
    hash_buffer_offset += HASH_SIZE;

    // append data hash
    os_memcpy(hash_buffer + hash_buffer_offset, bc->hashes + HASH_SIZE, HASH_SIZE);
    hash_buffer_offset += HASH_SIZE;

    int result = cx_hash_sha256(hash_buffer, hash_buffer_offset, bc->hashes, HASH_SIZE);
    VALIDATE(result == HASH_SIZE, ERR_INVALID_HASH);
}
