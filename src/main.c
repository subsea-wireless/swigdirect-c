
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "parameters.pb.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "udp_stream.h"
#include "cobs_codec.h"
#include "coap_codec.h"
#include "crc16.h"

#define DELAYVAL 500

/**
 * @brief The maximum protobuf message buffer length
 */
#define MSG_SIZE 50

/**
 * @brief The maximum CoAP message size (bytes)
 * @note The maximum CoAP overhead is the fixed 4 byte header with the largest token size (8 bytes) and an end marker (1 byte)
 */
#define COAP_SIZE (MSG_SIZE+13)

/**
 * @brief The maximum COBS message size (bytes)
 * @note This is the maximum CoAP message size, plus a 2-byte CRC, plus the overhead for the byte stuffing and with a nul start and end byte
 */
#define COBS_SIZE (2+(COAP_SIZE+2)+(COAP_SIZE+2)/254+1)

uint8_t message_buffer[MSG_SIZE];
uint8_t coap_buffer[COAP_SIZE];

//PacketSerial cobs; 
udp_connection_t udp_out, udp_in;
buffer_t udp_in_buffer;
byte_stream_t udp_out_stream, udp_in_stream;


char udp_packet_buffer[255];
unsigned int udp_port = 55502;

/**
 * @brief The source ID used in SWiG messages (message.source)
 */
int message_source = 2;

/**
 * @brief The target ID used in SWiG messages (message.target)
 */
int message_target = 1;


// nanopb callback for sending parameter payloads
bool send_parameter(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

/******************************************************************************
Function:   encode_string
******************************************************************************/
bool encode_string(pb_ostream_t *stream, const pb_field_t *field, void *const *arg) {
    char *str = (char *)*arg;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, (uint8_t *)str, strlen(str));
}

const char* kVendorName = "Hydromea";

// nanopb callback for sending parameter payloads
bool send_parameters(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.string.arg = (void*)kVendorName; 
  parameter.string.funcs.encode = encode_string;
  parameter.id = subseawireless_Parameter_identifier_manufacturer_name; // Manufacturer name
  pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);

  memset(&parameter, 0, sizeof(subseawireless_Parameter));
  pb_encode_tag_for_field(stream, field);
  parameter.integer = 1; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);

  pb_encode_tag_for_field(stream, field);
  parameter.integer = 2; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_minor; //SWiG version
  pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);

  pb_encode_tag_for_field(stream, field);
  parameter.integer = -99; //integer
  parameter.id = subseawireless_Parameter_identifier_background_noise; // Background noise
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

// convenience function to send a swig response message
void encode_swig_response(pb_ostream_t *pbbuffer) {
  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = message_source;
  message.target = message_target;
  //message.requests.funcs.encode = &send_request; 
  //message.parameters.funcs.encode = &send_parameter; 
  message.responses.funcs.encode = &send_parameters; 

  pb_encode(pbbuffer, subseawireless_Message_fields , &message);
}

bool decode_parameter(pb_istream_t *stream, const pb_field_iter_t *field, void **arg) {
    // 1. Handle "requests" (repeated int32 - packed)
    if (field->tag == subseawireless_Message_requests_tag) {
        while (stream->bytes_left > 0) {
            uint64_t value;
            if (!pb_decode_varint(stream, &value)) {
                return false;
            }
            printf("Requested ID: %lu\n", value);
        }
        return true;
    }

    // 2. Handle "parameters" or "responses" (repeated Message)  
    subseawireless_Parameter param = subseawireless_Parameter_init_default;
    
    // Use standard pb_decode. pb_decode_ex with NULLTERMINATED is for 
    // top-level streams without a known length.
    if (!pb_decode(stream, subseawireless_Parameter_fields, &param)) {
        const char * error = PB_GET_ERROR(stream);
        printf("pb_decode error: %s\n", error);
        return false;
    }

    printf("Received parameter! id=%i, value=%i\n", param.id, param.integer);
    return true;
}


bool decode_swig_message(pb_istream_t *pbbuffer_in, subseawireless_Message *message){
  //message->responses.funcs.decode = &decode_parameter;
  return pb_decode(pbbuffer_in, subseawireless_Message_fields, message);

}

void udp_init() {
    buffer_make_buffered_stream(&(udp_in_buffer), &(udp_in_stream));
		register_read_stream_udp( &udp_in_stream, &udp_in, udp_port);
		register_write_stream_udp(&udp_out_stream, &udp_out, "127.0.0.1",55501);
		udp_out.sock=udp_in.sock;

}

void send_to_stream(byte_stream_t *stream, uint8_t buf[], int size) {
  for (int i=0; i<size; i++) {
    stream->put(stream->data, buf[i]);
  }
  stream->flush(stream->data);
}

/** handler for received UDP packets*/
void udp_update() {
  
  //COBS cobs_coder;
  subseawireless_Message message = subseawireless_Message_init_default;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  bool success = false;
  uint8_t cobs_buffer[COBS_SIZE];
  struct CoapFrame coap_frame;

  //IPAddress remote = udp.remoteIP();
  //printf("Received UDP packet from %s,  %i bytes\n", remote.toString(), size);
  // read UDP data
  //int len = udp_receive_packet(udp_socket, cobs_buffer, COBS_SIZE);
  int len = udp_in_stream.read2buf((stream_data_t*)&udp_in, cobs_buffer, COBS_SIZE);
  if (len > 0) {
    printf("received data, %i bytes\n", len);
    // decode packet as COBS
    uint8_t* cobs_frame = cobs_buffer;
    while((*cobs_frame == '\0') && (len > 0)) {
      cobs_frame++;
      len--;
    }
    size_t msg_size = cobsDecode(cobs_frame, (size_t)len, cobs_buffer);
    printf("received COBS message: decoded size %lu\n", (long unsigned)msg_size);
    
    if(msg_size > 2) {
      // calculate the CRC
      uint16_t calculated_crc = Crc16CalculateChecksum(cobs_buffer, msg_size - 2);
      uint16_t decoded_crc = (uint16_t)(cobs_buffer[msg_size - 2]) << 8;
      decoded_crc |= cobs_buffer[msg_size - 1];
      printf("CRC: received 0x%02x, expected 0x%02x\n", decoded_crc, calculated_crc);
      // validate the CRC
      if(decoded_crc == calculated_crc) {
        // decode CoAP frame
        coapDecode(cobs_buffer, msg_size - 2, &coap_frame);

        printf("Version: %d\n", coap_frame.version);
        printf("Type: %d\n", coap_frame.type);
        printf("Message id: %d\n", coap_frame.message_id);
        printf("Code class: %d\n", coap_frame.code.code_class);
        printf("Code detail: %d\n", coap_frame.code.code_detail);
        printf("Payload length: %d\n", (int)coap_frame.payload.length);

        // decode SWiG protobuf
        pb_istream_t pbbuffer_in = pb_istream_from_buffer(coap_frame.payload.data, coap_frame.payload.length);
        
        message.requests.funcs.decode = &decode_parameter;
        message.requests.arg = &parameter;
        message.parameters.funcs.decode = &decode_parameter;
        message.parameters.arg = &parameter;
        message.responses.funcs.decode = &decode_parameter;
        message.responses.arg = &parameter;
        success = pb_decode(&pbbuffer_in, subseawireless_Message_fields, &message);

        printf("SWiG message from %i to %i [%s]\n", message.source, message.target, success ? "ok" : "failed");
      }
    }

    //udp.flush();

    if (!success) return;
    
    // send response back to sender
    pb_ostream_t pbbuffer = pb_ostream_from_buffer(message_buffer, sizeof(message_buffer));
    encode_swig_response(&pbbuffer);

    // add CoAP header
    coap_frame.payload.data = message_buffer;
    coap_frame.payload.length = pbbuffer.bytes_written;
    size_t coap_msg_length = coapEncode(&coap_frame, coap_buffer);

    // add crc
    uint16_t crc = Crc16CalculateChecksum(coap_buffer, coap_msg_length);
    coap_buffer[coap_msg_length] = crc >> 8;
    coap_buffer[coap_msg_length+1] = crc & 0xFF;

    // COBs encode and add 0 as a delimiter at the start and end
    cobs_buffer[0] = 0;
    size_t cobs_size = 1 + cobsEncode(coap_buffer, coap_msg_length+2, &cobs_buffer[1]);
    cobs_buffer[cobs_size++] = 0;
    
    send_to_stream(&udp_out_stream, cobs_buffer, cobs_size);
    printf("Sent COBS response: raw size %lu, encoded size %lu bytes\n", (long unsigned)pbbuffer.bytes_written, (long unsigned)cobs_size);
  }
}


/** handler for received serial packets (COBS callback)*/
void onPacketReceived(const uint8_t* buffer, size_t size)
{
  subseawireless_Message message = subseawireless_Message_init_default;
  bool success = false;
  pb_istream_t pbbuffer_in = pb_istream_from_buffer(buffer, size);
  // decode SWiG
  success = success && decode_swig_message(&pbbuffer_in, &message);
  if (!success) return;
  // send response
  pb_ostream_t pbbuffer = pb_ostream_from_buffer(message_buffer, sizeof(message_buffer));
  encode_swig_response(&pbbuffer);
  //cobs.send(sMsg, pbbuffer.bytes_written); 

}

void setup() {
    // Use serial port
  //begin(115200);
  // set up COBS
  //cobs.begin(&Serial);
  //cobs.setPacketHandler(&onPacketReceived);

  //setup_wifi();

  udp_init();
  
  //pb_ostream_t sizestream = {0};
  
}

bool send_request(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode(stream, subseawireless_Parameter_fields, &parameter);
}


int counter = 0;
void loop() {
  //cobs.update();
  udp_update();
  counter+=4;
  //delay(10);
}


void usage(const char *prog) {
  fprintf(stderr,
    "Usage: %s [--port <udp-port>] [--source <source-id>] [--target <target-id>]\n"
    "  --port    UDP port to listen on (default: 55502)\n"
    "  --source  Source ID for SWiG messages (default: 2)\n"
    "  --target  Target ID for SWiG messages (default: 1)\n", prog);
}

/**
 * @brief Parse command line arguments, falling back to defaults
 */
void parse_args(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      udp_port = (unsigned int)atoi(argv[++i]);
    } else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) {
      message_source = atoi(argv[++i]);
    } else if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
      message_target = atoi(argv[++i]);
    } else {
      fprintf(stderr, "Invalid argument: %s\n", argv[i]);
      usage(argv[0]);
      exit(EXIT_FAILURE);
    }
  }
}

int main (int argc, char **argv) {
  parse_args(argc, argv);
  setup();
  while (true) loop();
}