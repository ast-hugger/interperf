CC = clang
CFLAGS = -g -Wall -O3
BUILD_DIR = build

all: wordcode wordcode2 wordcode3 wordcode4 \
	handlercode handlercode2 \
	directthreaded directthreaded2 directthreaded3 \
	directthreaded3const directthreaded3primtweak directthreaded4 \
	comboinstructions comboinstructions2

%: %.c $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $(BUILD_DIR)/$@ $< $(LFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf *.o *.dSYM $(BUILD_DIR)
