export INCDIR=$(DEMIKERNEL_LIB)/include
export LIBS=$(DEMIKERNEL_LIB)/lib/libdemikernel.so
export CC := gcc
export CFLAGS := -Werror -Wall -Wextra -O3 -I $(INCDIR) -std=c99
export SRC_C := $(wildcard *.c)
export OBJ := $(SRC_C:.c=.o)
export BINDIR=bin

multi-connect: mkdir multi-connect.o
	$(CC) $(CFLAGS) $@.o -o $(BINDIR)/$@ $(LIBS)

mkdir:
	mkdir -p $(BINDIR)

clean:
	@rm -rf $(OBJ)
	@rm -rf $(BINDIR)/multi-connect
