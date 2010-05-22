CC = gcc
INCLUDE = -I.
CFLAGS = -g $(INCLUDE) ${OPTIONS}
LIBS = -lpcap -lz -lpthread

TARGET1 = apsdsniff 
OBJ1 = wl.o mgmt.o parseMgmtFixed.o parseMgmtIelems.o wlcommon.o data.o

all:	$(TARGET1)

clean:
	rm -f *.o *~ *.bak $(TARGET1)

$(TARGET1):	$(OBJ1)
	$(CC) -o $(TARGET1) $(OBJ1) $(LIBS)
