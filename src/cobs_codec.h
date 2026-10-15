#ifndef COBS_CODEC_H
#define COBS_CODEC_H

#include <stdint.h>
#include <stddef.h>

/** 
 * @brief COBS encode data to buffer
 * @param input_buffer The input data to encode
 * @param input_length_bytes The number of bytes to encode
 * @param encoded_buffer The encoded output buffer
 * @return Encoded buffer length in bytes
 * @note Does not output delimiter byte
*/
size_t cobsEncode(const uint8_t* input_buffer, size_t input_length_bytes, uint8_t* encoded_buffer);

/** 
 * @brief COBS decode data from buffer
 * @param encoded_buffer The input (encoded) data
 * @param input_length_bytes The number of bytes to decode
 * @param decoded_buffer The decoded output buffer
 * @return Number of bytes successfully decoded
 * @note Stops decoding if delimiter byte is found
*/
size_t cobsDecode(const uint8_t* encoded_buffer, size_t input_length_bytes, uint8_t* decoded_buffer);

#endif // COBS_CODEC_H
