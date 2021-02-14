# C Compiler of choice
CC         := gcc
# C Compiler flags
CFLAGS     := -Wall -Werror -Wextra
# Get all source files in current directory with .c extension
SRC        := $(wildcard *.c)
# For all source files compile a .o file for the them
OBJ        := $(SRC:.c=.o)
# Final executable to create
EXECUTABLE := simple_http_client.out

# Rule to build everything with phony command
.PHONY: all
all: $(EXECUTABLE)

# $@ refers to the target $(EXECUTABLE)
# $< means the all the rules seperated by spaces.
$(EXECUTABLE): $(OBJ)
	$(CC) $^ $(CFLAGS) -o $@

# Create a .o rule for all .c files which compiles them all without linking
%.o: %.c
	$(CC) $< $(CFLAGS) -c -o $@

# Rule to clean up all object files
.PHONY: clean
clean:
	rm -f $(OBJ)
