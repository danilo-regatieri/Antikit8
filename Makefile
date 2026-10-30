CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
LDLIBS = -lm

TARGET = antikit8
SRC = main.c mmu.c wsclock.c auditoria.c hardware_specs.c
OBJ = $(SRC:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDLIBS)

%.o: %.c simulator.h hardware_specs.h
	$(CC) $(CFLAGS) -c $< -o $@

debug: CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
debug: LDLIBS = -lm -fsanitize=address,undefined
debug: clean $(TARGET)

clean:
	rm -f $(TARGET) $(OBJ) metrics.csv resultados/*.csv resultados/*.log

.PHONY: all debug clean
