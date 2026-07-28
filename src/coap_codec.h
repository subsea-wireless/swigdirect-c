#ifndef COAP_CODEC_H
#define COAP_CODEC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Standard CoAP message types
 */
enum CoapMessageType {
    kRequestConfirmable = 0,
    kRequestNonConfirmable = 1,
    kResponseAcknowledge = 2,
    kResponseReset = 3
};

/**
 * @brief Standard CoAP method codes
 */
enum CoapMethodCode {
    kCoapUndefinedCode = 0,
    kCoapGetCode = 1,
    kCoapPostCode = 2,
    kCoapPutCode = 3,
    kCoapDeleteCode = 4,
    kCoapFetchCode = 5,
    kCoapPatchCode = 6,
    kCoapIPatchCode = 7 
};

/**
 * @brief Combines a CoAP code class and method 
 */
struct CoapCode {
    uint8_t code_class;
    uint8_t code_detail;
};

/**
 * @brief Combines payload and payload length information 
 */
struct CoapSizedData {
    size_t length;
    const uint8_t* data;
};

/**
 * @brief Holds parameters relating to a CoAP frame
 */
struct CoapFrame {
    uint8_t version;                // the CoAP version number
    enum CoapMessageType type;      // the CoAP message type
    struct CoapCode code;           // the CoAP message class/code
    uint16_t message_id;            // the CoAP message id
    struct CoapSizedData token;     // the CoAP token details
    struct CoapSizedData payload;   // the CoAP payload details
};

/**
 * @brief Encodes a CoAP frame to a message buffer
 * @param coap_frame The CoAP frame to encode
 * @param message_buffer The destination message buffer
 */
size_t coapEncode(struct CoapFrame* coap_frame, uint8_t* message_buffer);

/** 
 * @brief Decodes a CoAP frame from a message buffer
 * @param encoded_buffer The input (encoded) data
 * @param input_length_bytes The number of bytes to decode
 * @param coap_frame The decoded CoAP frame
 * @return true if a valid CoAP frame was decoded, false otherwise
*/
bool coapDecode(const uint8_t* encoded_buffer, size_t input_length_bytes, struct CoapFrame* coap_frame);

#endif // COAP_CODEC_H