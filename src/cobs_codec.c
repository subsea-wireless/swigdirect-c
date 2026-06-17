
#include "cobs_codec.h"

/**
 * prr 2026
 * Adapted from https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
 */

size_t cobsEncode(const uint8_t* input_buffer, size_t input_length_bytes, uint8_t* encoded_buffer)
{
    uint8_t* output = encoded_buffer; // Encoded byte pointer
	uint8_t* output_code = output++; // Output code pointer
	uint8_t code = 1; // Code value

    for (const uint8_t *input = input_buffer; input_length_bytes--; ++input)
	{
        if (*input > 0) {
		    // Byte not zero, write it
            *output++ = *input;
            code++;
        }

        // Input is zero or block completed, 
		if ((*input == 0) || (code == 0xff)) 
		{
            // Reset the code and associated output pointers
			*output_code = code;
            code = 1;
            output_code = output;
			if ((*input == 0) || (input_length_bytes > 0)) {
				++output;
            }
		}
	}

    // Write the final code value
	*output_code = code; 

	return (size_t)(output - encoded_buffer);
}

// ----------------------------------------------------------------------------

size_t cobsDecode(const uint8_t* encoded_buffer, size_t input_length_bytes, uint8_t* decoded_buffer)
{
	const uint8_t *input = encoded_buffer; // Encoded input byte pointer
	uint8_t *decode = decoded_buffer; // Decoded output byte pointer
    uint8_t code = 0xff;
    uint8_t block = 0;

	while(input < (encoded_buffer + input_length_bytes))
	{
		if (block > 0)
        { 
            *decode++ = *input++;  // Decode block byte
        }
        else
		{
			block = *input++;  // Fetch the next block length            
			if ((block > 0) && (code != 0xff)) { 
				*decode++ = 0; // Encoded zero, write it unless it's delimiter
            }
			code = block;
			if (code == 0) {
			    // Delimiter code found so terminate
                break;
            }
		}
        block--;
	}

	return (size_t)(decode - decoded_buffer);
}
