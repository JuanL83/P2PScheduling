CC = gcc
CCOPTS = -Wall -c -g -ggdb
LINKOPTS = -Wall -g -ggdb -lpthread

EXEC=scheduler
OBJECTS=scheduler.o schedulerSimulation.o list.o

all: $(EXEC)

$(EXEC): $(OBJECTS)
	$(CC) $(LINKOPTS) -o $@ $^

%.o:%.c
	$(CC) $(CCOPTS) -o $@ $^

test: scheduler
	- make clean
	- make
	- ./scheduler -fifo -test
	- ./scheduler -rr -test

clean:
	- $(RM) $(EXEC)
	- $(RM) $(OBJECTS)
	- $(RM) *~
