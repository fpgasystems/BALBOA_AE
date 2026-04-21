CC = gcc
CFLAGS = -g
LDFLAGS = -g -libverbs -lrdmacm -lssl -lcrypto -lpthread

BIN_DIR = bin

%.o: %.c $(DEPS)
	@mkdir -p $(BIN_DIR)
	$(CC) -c -o $(BIN_DIR)/$@ $< $(CFLAGS)

server: server.o common.o
	@mkdir -p $(BIN_DIR)
	cd $(BIN_DIR) && $(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

client: client.o common.o
	@mkdir -p $(BIN_DIR)
	cd $(BIN_DIR) && $(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -rf $(BIN_DIR)

.PHONY: clean