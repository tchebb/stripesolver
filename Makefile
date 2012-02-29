OBJECTS = main.o
CFLAGS = -Wall

solver : $(OBJECTS)
	cc $(CFLAGS) -o stripesolver $(OBJECTS)

.PHONY: clean
clean :
	-rm stripesolver $(OBJECTS)

