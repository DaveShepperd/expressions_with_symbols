CC=gcc
AR=ar
WARNS=-Wall 
BUILD=-std=c99 -pedantic
DEFS=-D_GNU_SOURCE
OPT=-O0 -g
#OPT=-O3
INCS=

CFLAGS=$(INCS) $(BUILD) $(OPT) $(WARNS)

COMMON=lib_common
TARGET1=lib_exprs
TARGET2=lib_btree
TARGET3=lib_hashtbl

.SILENT:

default: $(COMMON).a

clean:
	echo "    Cleaning ..."
	rm -rf $(TARGET1).o $(TARGET2).o $(TARGET3).o $(COMMON).a mk_operstuff mk_operstuff.o Debug/ Release/

$(COMMON).a: $(TARGET1).o $(TARGET2).o $(TARGET3).o
	echo "    Making $@ ..."
	$(AR) -r $@ $^

lib_operstuff.h : mk_operstuff lib_operstuff.dat Makefile
	echo "    Making $@ ..."
	./mk_operstuff > $@

mk_operstuff: mk_operstuff.c Makefile
	echo "    Making $@ ..."
	$(CC) -o $@ $(CFLAGS) $<

%.o: %.c
	echo "    Making $@ ..."
	$(CC) -c $(CFLAGS) $<

$(TARGET1).o: $(TARGET1).c lib_operstuff.h $(TARGET1).h
$(TARGET2).o: $(TARGET2).c lib_operstuff.h $(TARGET1).h $(TARGET2).h
$(TARGET3).o: $(TARGET3).c lib_operstuff.h $(TARGET1).h $(TARGET3).h

