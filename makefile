# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g

# Libraries
LIBS = -lnetfilter_queue

# Source files
SRCS = fire_wall.c utils.c bucket.c

# Object files (replace .c with .o)
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = fire_wall

# Default target
all: $(TARGET)

# Link object files into executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

# Compile .c files to .o files
%.o: %.c fire_wall.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files
clean:
	rm -f $(OBJS) $(TARGET)

# Phony targets (not actual files)
.PHONY: all clean
