objects = core.o main.o
flags = `pkg-config --cflags gtk+-2.0`
libs = `pkg-config --libs gtk+-2.0`

tourniquet: $(objects)
	gcc -g $(flags) -Wall $(objects) -o tourniquet -ggdb $(libs)

main.o: main.c core.o
	gcc -g $(flags) -Wall -c main.c -ggdb $(libs)
core.o: core.c core.h
	gcc -g 	$(flags) -Wall -c core.c core.h -ggdb $(libs)

.PHONY: clean
clean:
	rm tourniquet $(objects)