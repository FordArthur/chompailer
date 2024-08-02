CC := gcc
CCFLAGS := -Wall
EXEC_FILE := chompailer

SRC_LOCATION := ./src
BUILD_LOCATION := ./build
TEST_LOCATION := ./test

SRCS := $(wildcard $(SRC_LOCATION)/*.c)
OBJS := $(patsubst $(SRC_LOCATION)/%.c,$(BUILD_LOCATION)/%.o,$(SRCS))

build: $(OBJS)
	$(CC) $(OBJS) -o $(EXEC_FILE)

release: CCFLAGS += -O2 -mavx2 -march=native
release: build

debug: CCFLAGS += -g -ggdb2 -fsanitize=address
debug: build

$(BUILD_LOCATION)/%.o: $(SRC_LOCATION)/%.c
	$(CC) $(CCFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(BUILD_LOCATION)/*
