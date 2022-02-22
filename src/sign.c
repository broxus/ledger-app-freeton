#include "apdu_constants.h"
#include "utils.h"
#include "errors.h"

static uint8_t set_result_sign() {
    cx_ecfp_private_key_t privateKey;
    SignContext_t* context = &data_context.sign_context;

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
    ux_sign_flow_1_step,
    pnn,
    {
      &C_icon_eye,
      "Review",
      "transaction",
    });
UX_STEP_NOCB(
    ux_sign_flow_2_step,
    bnnn_paging,
    {
      .title = "Amount",
      .text = data_context.sign_context.amount_str,
    });
UX_STEP_NOCB(
    ux_sign_flow_3_step,
    bnnn_paging,
    {
      .title = "Address",
      .text = data_context.sign_context.dst_address_str,
    });
UX_STEP_CB(
    ux_sign_flow_4_step,
    pnn,
    send_response(set_result_sign(), true),
    {
      &C_icon_validate_14,
      "Finalize",
      "Transaction",
    });
UX_STEP_CB(
    ux_sign_flow_5_step,
    pb,
    send_response(0, false),
    {
      &C_icon_crossmark,
      "Cancel",
    });

UX_FLOW(ux_sign_flow,
    &ux_sign_flow_1_step,
    &ux_sign_flow_2_step,
    &ux_sign_flow_3_step,
    &ux_sign_flow_4_step,
    &ux_sign_flow_5_step
);

void handleSign(uint8_t p1, uint8_t p2, uint8_t *dataBuffer, uint16_t dataLength, volatile unsigned int *flags, volatile unsigned int *tx) {
    VALIDATE(p1 == 0 && p2 == 0, ERR_INVALID_REQUEST);
    SignContext_t* context = &data_context.sign_context;
    VALIDATE(dataLength == (sizeof(context->account_number) + sizeof(context->amount) + sizeof(context->dst_account_id) + sizeof(context->to_sign)), ERR_INVALID_REQUEST);

    context->account_number = readUint32BE(dataBuffer);
    context->amount = readUint64BE(dataBuffer + sizeof(context->account_number));
    memcpy(context->dst_account_id, dataBuffer + sizeof(context->account_number) + sizeof(context->amount), ADDRESS_LENGTH);
    memcpy(context->to_sign, dataBuffer + sizeof(context->account_number) + sizeof(context->amount) + sizeof(context->dst_account_id), TO_SIGN_LENGTH);

    print_amount(context->amount, context->amount_str, sizeof(context->amount_str));
    print_address_short(context->dst_account_id, context->dst_address_str, sizeof(context->dst_address_str));

    ux_flow_init(0, ux_sign_flow, NULL);
    *flags |= IO_ASYNCH_REPLY;
}
