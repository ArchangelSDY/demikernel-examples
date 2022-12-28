export CC := gcc
export CFLAGS := -Werror -Wall -Wextra -O3 -I $(INCDIR) -std=c99
export SRC_C := $(wildcard *.c)
export OBJ := $(SRC_C:.c=.o)
export BINDIR=bin

http-server: mkdir http-server.o
	$(CC) $(CFLAGS) $@.o -o $(BINDIR)/$@ $(LIBS)

mkdir:
	mkdir -p $(BINDIR)

clean:
	@rm -rf $(OBJ)
	@rm -rf $(BINDIR)/http-server
