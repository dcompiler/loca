# Get the current OS and architecture
OS ?= $(shell uname -s)
CPU ?= $(shell uname -m)
PLATFORM ?= $(OS).$(CPU)
TARGET_PLATFORM ?= $(PLATFORM)

# Set the default compilers
CC = gcc
CXX = g++
CLINK = gcc

# Compile flag options
CFLAGS ?= -O0 -g 
CXXFLAGS ?= $(CFLAGS) -std=c++11

# Don't require any libraries by default
LIBS ?= -lm 

# Set the default include directories
INCLUDE_DIRS ?= -I. 

# Build by default
all: simulator

SRCS ?= $(wildcard *.c)
OBJS ?= $(patsubst %.c, %.o, $(SRCS))
HEADERS ?= $(wildcard *.h)

simulator: $(OBJS)
	$(CLINK) $< -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_DIRS) $< -c -o $@

clean:
	rm -rf $(OBJS) simulator;
