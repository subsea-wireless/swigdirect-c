# Include the nanopb provided Makefile rules
include nanopb/extra/nanopb.mk

# Compiler flags to enable all warnings & debug info
CFLAGS = -Wall -Werror -g -O0
CFLAGS += "-I$(NANOPB_DIR)"

# C source code files that are required
CSRC  = src/main.c                   # The main program
CSRC += src/parameters.pb.c          # The compiled protocol definition
CSRC += src/udp_stream.c            
CSRC += src/buffer.c                
CSRC += $(NANOPB_DIR)/pb_encode.c  # The nanopb encoder
CSRC += $(NANOPB_DIR)/pb_decode.c  # The nanopb decoder
CSRC += $(NANOPB_DIR)/pb_common.c  # The nanopb common parts

# Build rule for the main program
main: $(CSRC)
	$(CC) $(CFLAGS) -omain $(CSRC)

# Build rule for the protocol
parameters.pb.c: parameters.proto
	$(PROTOC) $(PROTOC_OPTS) --nanopb_out=. parameters.proto

