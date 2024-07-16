CC = gcc
CCFLAGS = -Wall -march=native -mavx2 -O2
EXEC_FILE = chompailer
SRC = $(wildcard *.c)
OBJ = $(SRC:.c=.o)

all: $(EXEC_FILE)

debug: CCFLAGS += -ggdb -g -D DEBUG
debug: $(OBJ) 
	$(CC) $^ -o $(EXEC_FILE)
	
%.o: %.c %.h
	$(CC) -c $(CCFLAGS) $< -o $@

$(EXEC_FILE): $(OBJ)
	$(CC) $^ -o $@

clean:
	rm -f $(OBJ)
