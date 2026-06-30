CC      = gcc
CFLAGS  = -std=c11 -O2 -Iinclude
SRCS    = src/value.c src/chunk.c src/vm.c src/lexer.c \
          src/compiler.c src/stdlib.c src/http.c src/db.c src/main.c
OUT     = alzc

all: $(OUT)

$(OUT):
	$(CC) $(CFLAGS) $(SRCS) -o $(OUT) -lm
	@echo "Built: ./$(OUT)"

debug:
	$(CC) $(CFLAGS) -g -fsanitize=address $(SRCS) -o $(OUT)_debug -lm

clean:
	rm -f $(OUT) $(OUT)_debug *.o

install: $(OUT)
	cp $(OUT) /usr/local/bin/$(OUT)

.PHONY: all debug clean install
