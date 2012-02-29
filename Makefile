OBJECTS = solver.o
CFLAGS = -Wall

solver : $(OBJECTS)
	cc $(CFLAGS) -o solver $(OBJECTS)

.PHONY: clean
clean :
	-rm solver $(OBJECTS)

