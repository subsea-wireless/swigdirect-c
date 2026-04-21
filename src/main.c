
#include <stdbool.h>
#include <stdio.h>
#include <pb_encode.h>
#include <pb_decode.h>
#include "parameters.pb.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "udp_stream.h"

#define DELAYVAL 500

#define MSG_SIZE 50
// calculate COBS overhead for max. buffer size
#define COBS_SIZE (MSG_SIZE+MSG_SIZE/254+1)

uint8_t sMsg[MSG_SIZE];

//PacketSerial cobs; 
udp_connection_t udp_out, udp_in;
buffer_t  udp_in_buffer;
byte_stream_t udp_out_stream, udp_in_stream;


char udp_packet_buffer[255];
unsigned int udp_port = 55502;


// nanopb callback for sending parameter payloads
bool send_parameter(pb_ostream_t *stream, const pb_field_iter_t *field, void * const *arg) {
  if (!pb_encode_tag_for_field(stream, field))
    return false;
  subseawireless_Parameter parameter = subseawireless_Parameter_init_default;
  parameter.integer = 42; //integer
  parameter.id = subseawireless_Parameter_identifier_swig_version_major; //SWiG version
  return pb_encode_submessage(stream, subseawireless_Parameter_fields, &parameter);
}

// convenience function to send a swig response message
void encode_swig_response(pb_ostream_t *pbbuffer) {
  subseawireless_Message message = subseawireless_Message_init_default;
  message.source = 2;
  message.target = 1;
  //message.requests.funcs.encode = &send_request; 
  //message.parameters.funcs.encode = &send_parameter; 
  message.responses.funcs.encode = &send_parameter; 

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

  //IPAddress remote = udp.remoteIP();
  //printf("Received UDP packet from %s,  %i bytes\n", remote.toString(), size);
  // read UDP data
  //int len = udp_receive_packet(udp_socket, cobs_buffer, COBS_SIZE);
  int len = udp_in_stream.read2buf((stream_data_t*)&udp_in, cobs_buffer, COBS_SIZE);
  if (len>0) {
    printf("received data, %i bytes\n", len);
    success = true;
    // decode packet
    //uint8_t in_buffer[MSG_SIZE];
    //size_t msg_size = cobs_coder.decode(cobs_buffer, len, in_buffer);
    //printf("received COBS message: raw size %i, decoded size %i\n", len, msg_size);
    //pb_istream_t pbbuffer_in = pb_istream_from_buffer(in_buffer, msg_size);
    pb_istream_t pbbuffer_in = pb_istream_from_buffer(cobs_buffer, len);
    // decode SWiG
    message.requests.funcs.decode = &decode_parameter;
    message.requests.arg = &parameter;
    message.parameters.funcs.decode = &decode_parameter;
    message.parameters.arg = &parameter;
    message.responses.funcs.decode = &decode_parameter;
    message.responses.arg = &parameter;
    pb_decode(&pbbuffer_in, subseawireless_Message_fields, &message);

    //success = success && decode_swig_message(&pbbuffer_in, &message);
    printf("SWiG message from %i to %i\n", message.source, message.target);
    
    //udp.flush();

    if (!success) return;
    
    // send response back to sender
    pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
    encode_swig_response(&pbbuffer);
    //size_t cobs_size = cobs_coder.encode(sMsg, pbbuffer.bytes_written, cobs_buffer);
    // add 0 as delimiter
    //cobs_size++;
    //send_to_stream(&udp_out_stream, cobs_buffer, cobs_size);
    //printf("Sent response, %i bytes\n", cobs_size);
    send_to_stream(&udp_out_stream, sMsg, pbbuffer.bytes_written);
    printf("Sent response, %i bytes\n", (int)pbbuffer.bytes_written);
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
  pb_ostream_t pbbuffer = pb_ostream_from_buffer(sMsg, sizeof(sMsg));
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


int main (void) {
  setup();
  while (true) loop();
}