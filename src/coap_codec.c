#include "coap_codec.h"
#include <string.h>

/**
 * @brief The current CoAP message version that can be encoded/decoded
 */
const uint8_t kCoapSupportedVersion = 0x1; 

/**
 * @brief Unpacks (reads) the given bits from a byte
 * @param byte The byte to read the desired bits from
 * @param bit_offset The bit index of the lsb to start extracting from
 * @param num_bits The number of bits to extract
 * @return The extracted bits
 */
uint8_t Unpack(uint8_t byte, uint8_t bit_offset, uint8_t num_bits) {
    uint8_t mask = 0xFF;
    if (num_bits < 7) {
        mask = (0x1 << num_bits) - 1;
    }
    return (byte >> bit_offset) & mask;
}

/**
 * @brief Packs (writes) the given bits in to a byte
 * @param dest The byte to write the bits to
 * @param bit_offset The bit index of the lsb to start write to
 * @param num_bits The number of bits to write
 */
void Pack(uint8_t* dest, uint8_t value, uint8_t bit_offset, uint8_t num_bits) {
    uint8_t base_bits  = 0;
    if(bit_offset > 0) {
        base_bits = 8 - (bit_offset & 0x7);
    }
    uint8_t mask = (1 << num_bits) - 1;
    mask <<= base_bits - num_bits;
    *dest &= ~mask;
    *dest |= value << bit_offset;
}

// ----------------------------------------------------------------------------

size_t coapEncode(struct CoapFrame* coap_frame, uint8_t* message) {
    Pack(message, coap_frame->version, 6, 2);
    Pack(message, coap_frame->type, 4, 2);
    Pack(message, coap_frame->token.length, 0, 4);
    Pack(&message[1], coap_frame->code.code_class, 5, 3);
    Pack(&message[1], coap_frame->code.code_detail, 0, 5);
    message[2] = coap_frame->message_id >> 8;
    message[3] = coap_frame->message_id & 0xff;
    if(coap_frame->token.length > 0) {
       memcpy(&message[4], coap_frame->token.data, coap_frame->token.length);
    }
    message[4 + coap_frame->token.length] = 0xff;
    size_t offset = 4 + coap_frame->token.length + 1; 
    memcpy(&message[offset], coap_frame->payload.data, coap_frame->payload.length);
    return coap_frame->payload.length + offset;
}

// ----------------------------------------------------------------------------

bool coapDecode(const uint8_t* encoded_buffer, size_t input_length_bytes, struct CoapFrame* coap_frame) {
    // initialise the frame to zero
    memset(coap_frame, 0, sizeof(struct CoapFrame));
    // check the frame version is supported
    uint8_t version = Unpack(encoded_buffer[0], 6, 2);
    if(version == kCoapSupportedVersion) {
        coap_frame->version = version;
        coap_frame->type = (enum CoapMessageType)Unpack(encoded_buffer[0], 4, 2);
        // unpack the message token details
        uint8_t token_length = Unpack(encoded_buffer[0], 0, 4);
        if(token_length > 8) {
            token_length = 8;
        }
        coap_frame->token.length = token_length;
        coap_frame->token.data = &encoded_buffer[4];
        // unpack the message class and code
        coap_frame->code.code_class = Unpack(encoded_buffer[1], 5, 3);
        coap_frame->code.code_detail = Unpack(encoded_buffer[1], 0, 5);
        // unpack the message id
        coap_frame->message_id = (uint16_t)(encoded_buffer[2]) << 8;
        coap_frame->message_id |= encoded_buffer[3];
        // unpack the payload related components
        coap_frame->payload.data = NULL;
        if(encoded_buffer[3 + token_length] == 0xFF) {
            coap_frame->payload.data = &encoded_buffer[3 + token_length + 1];
        } else if(encoded_buffer[4 + token_length] == 0xFF) {
            coap_frame->payload.data = &encoded_buffer[4 + token_length + 1];
        }
         // calculate the payload length
        if(coap_frame->payload.data != NULL) {
            coap_frame->payload.length = (encoded_buffer + input_length_bytes) - coap_frame->payload.data;
        } else {
            coap_frame->payload.length = 0;
        }
    }
    return false;
}