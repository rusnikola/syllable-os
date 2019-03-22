CC   = gcc
LL   = gcc

OBJS = ftreg.o registrar.o
EXE  = ftreg
COPTS = -c -Wall -O2

all: $(OBJS)
	$(LL) $(OBJS) -lsyllable -lstdc++ -o $(EXE)
	strip --strip-all $(EXE)

clean:
	rm -f $(OBJS)
	rm -f $(EXE)

%.o: %.cpp
	$(CC) $(COPTS) $< -o $@

ftreg.o: ftreg.cpp
registrar.o: registrar.cpp
