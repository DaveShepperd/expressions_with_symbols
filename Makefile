TARGET=main
DEPEND_LIB=libs/lib_common.a

CC=gcc
AR=ar
LD=gcc
STDLIBS=-lm
WARNS=-Wall -pedantic
BUILD=-std=c99
INCS=-Ilibs
#OPT=-O0 -g
OPT=-O3
CFLAGS=$(INCS) $(OPT) $(WARNS) $(BUILD)

default: $(DEPEND) $(TARGET)

OBJS=btree_test.o hashtbl_test.o exprs_test_bt.o exprs_test_ht.o exprs_test_nos.o

$(DEPEND_LIB):
	cd libs;make
	
$(TARGET): $(TARGET).o $(OBJS) $(DEPEND_LIB)
	$(LD) -o $@ $^ $(STDLIBS)

clean:
	rm -f *.o $(TARGET)
	cd libs;make clean

$(TARGET).o: $(TARGET).c

btree_test.o: btree_test.c btree_test.h
exprs_test_bt.o: exprs_test_bt.c exprs_test_bt.h
exprs_test_ht.o: exprs_test_ht.c exprs_test_ht.h
exprs_test_nos.o: exprs_test_nos.c exprs_test_nos.h
hashtbl_test.o: hashtbl_test.c hashtbl_test.h
lib_btree.o: lib_btree.c lib_btree.h
lib_hashtbl.o: lib_hashtbl.c lib_hashtbl.h
lib_exprs.o: lib_exprs.c lib_exprs.h

