CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude $(shell pkg-config --cflags gtk+-3.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0) -lm

SRC_DIR = src
GUI_DIR = src/gui
BACKEND_DIR = src/backend
OBJ_DIR = build

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c) \
       $(wildcard $(GUI_DIR)/*.c) \
       $(wildcard $(BACKEND_DIR)/*.c)

# Object files (mapped to build directory)
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRCS))

# Executable name
TARGET = sorter.exe

# Default rule
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

# Compile
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean
