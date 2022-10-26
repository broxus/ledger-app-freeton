#include "apdu_constants.h"
#include "utils.h"
#include "errors.h"
#include "byte_stream.h"
#include "message.h"
#include "contract.h"

static uint8_t set_result_sign_transaction() {
    cx_ecfp_private_key_t privateKey;
    SignTransactionContext_t* context = &data_context.sign_tr_context;

    BEGIN_TRY {
        TRY {
            get_private_key(context->account_number, &privateKey);
            cx_eddsa_sign(&privateKey, CX_LAST, CX_SHA512, context->to_sign, TO_SIGN_LENGTH, NULL, 0, context->signature, SIGNATURE_LENGTH, NULL);
        } FINALLY {
            memset(&privateKey, 0, sizeof(privateKey));
        }
    }
    END_TRY;

    uint8_t tx = 0;
    G_io_apdu_buffer[tx++] = SIGNATURE_LENGTH;
    memmove(G_io_apdu_buffer + tx, context->signature, SIGNATURE_LENGTH);
    tx += SIGNATURE_LENGTH;
    return tx;
}

UX_STEP_NOCB(
    ux_sign_transaction_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });
UX_STEP_NOCB(
    ux_sign_transaction_flow_2_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = data_context.sign_tr_context.amount_str,
    });
UX_STEP_NOCB(
    ux_sign_transaction_flow_3_step,
    bnnn_paging,
    {
      .title = "Address",
      .text = data_context.sign_tr_context.address_str,
    });
UX_STEP_NOCB(
    ux_sign_transaction_flow_4_step,
    bnnn_paging,
    {
      .title = "Hash",
      .text = data_context.sign_tr_context.to_sign_str,
    });
UX_STEP_CB(
    ux_sign_transaction_flow_5_step,
    pbb,
    send_response(set_result_sign_transaction(), true),
    {
      &C_icon_validate_14,
      "Accept",
      "and send",
    });
UX_STEP_CB(
    ux_sign_transaction_flow_6_step,
    pb,
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Reject",
    });

UX_FLOW(ux_sign_transaction_flow,
    &ux_sign_transaction_flow_1_step,
    &ux_sign_transaction_flow_2_step,
    &ux_sign_transaction_flow_3_step,
    &ux_sign_transaction_flow_4_step,
    &ux_sign_transaction_flow_5_step,
    &ux_sign_transaction_flow_6_step
);

void handleSignTransaction(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    UNUSED(tx);

    VALIDATE(p1 == 0 && p2 == 0, ERR_INVALID_REQUEST);
    SignTransactionContext_t* context = &data_context.sign_tr_context;

    size_t offset = 0;

    context->account_number = readUint32BE(dataBuffer + offset);
    offset += sizeof(context->account_number);

    context->wallet_type = readUint32BE(dataBuffer + offset);
    offset += sizeof(context->wallet_type);

    uint8_t* msg_begin = dataBuffer + offset;
    uint8_t msg_length = dataLength - offset;

    ByteStream_t src;
    ByteStream_init(&src, msg_begin, msg_length);

    prepare_to_sign(&src);

    // TODO: remove later
    snprintf(context->to_sign_str, sizeof(context->to_sign_str), "%.*H", sizeof(context->to_sign), context->to_sign);

    ux_flow_init(0, ux_sign_transaction_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
