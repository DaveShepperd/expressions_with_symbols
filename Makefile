TARGET=main
DEPEND_LIB=libs/lib_common.a

CC=gcc
AR=ar
LD=gcc
STDLIBS=-lm
WARNS=-Wall -pedantic
BUILD=-std=c99
INCS=-Ilibs
OPT=-O0 -g
#OPT=-O3
CFLAGS=$(INCS) $(OPT) $(WARNS) $(BUILD)

default: $(DEPEND) $(TARGET)

OBJS=exprs_test.o exprs_test_bt.o exprs_test_ht.o exprs_test_nos.o exprs_test_walk.o

$(DEPEND_LIB):
	cd libs;make
	
$(TARGET): $(TARGET).o $(OBJS) $(DEPEND_LIB)
	$(LD) -o $@ $^ $(STDLIBS)

clean:
	rm -rf *.o $(TARGET) Debug Release
	cd libs;make clean

$(TARGET).o: $(TARGET).c

exprs_test.o: exprs_test.c exprs_test.h
exprs_test_bt.o: exprs_test_bt.c exprs_test_bt.h
exprs_test_ht.o: exprs_test_ht.c exprs_test_ht.h
exprs_test_nos.o: exprs_test_nos.c exprs_test_nos.h
exprs_test_walk.o: exprs_test_walk.c exprs_test_walk.h
