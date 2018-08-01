EXEC     = raid5_soft
CC       = gcc

CFLAGS   = -std=gnu11 -O3 -Wall -Wextra -Wstrict-aliasing
LDFLAGS  += -pthread

SRC      = $(wildcard *.c)
OBJ      = $(SRC:.c=.o)

all: $(EXEC)

${EXEC}: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

.PHONY: clean mrproper

clean:
	@rm -rf *.o

mrproper: clean
	@rm -rf $(EXEC)