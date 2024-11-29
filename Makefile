# Makefile for lite-shell project

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -I./include -Wall -Wextra

# Build directory
BUILDDIR = build

# Source and object files
SRCDIR = src
OBJDIR = $(BUILDDIR)/obj
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = $(BUILDDIR)/lite-shell

# Default target
all: $(TARGET)

# Rule to link object files to create the executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

# Rule to compile source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove object files and executable
clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
