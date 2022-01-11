CC=gcc
AR=ar
WARNS=-Wall 
BUILD=-std=c99 -pedantic
OPT=-O3
INCS=

CFLAGS=$(INCS) $(BUILD) $(OPT) $(WARNS)

COMMON=lib_common
TARGET1=lib_exprs
TARGET2=lib_btree
TARGET3=lib_hashtbl

.SILENT:

default: $(COMMON).a

clean:
	rm -rf $(TARGET1).o $(TARGET2).o $(TARGET3).o $(COMMON).a Debug/ Release/
	
$(COMMON).a: $(TARGET1).o $(TARGET2).o $(TARGET3).o
	$(AR) -r $@ $^
	
$(TARGET1).o: $(TARGET1).c $(TARGET1).h
$(TARGET2).o: $(TARGET2).c $(TARGET2).h
$(TARGET3).o: $(TARGET3).c $(TARGET3).h

