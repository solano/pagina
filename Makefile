.POSIX:
.SUFFIXES:

CC	= cc
CFLAGS	= -Wall -Wextra -g
mkbuilddir = @mkdir -p build; mkdir -p build/obj

all: build/pagina

build/pagina: build/libpagina.a src/pagina.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o build/pagina src/pagina.c $(LDLIBS) -Lbuild -lpagina

build/libpagina.a: src/pagina.h build/obj/parse.o build/obj/view.o build/obj/write.o build/obj/types.o build/obj/filter.o build/obj/document.o
	cp src/pagina.h build/pagina.h
	ar rcs build/libpagina.a build/obj/*

build/obj/parse.o: src/parse.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/parse.o -c src/parse.c

build/obj/view.o: src/view.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/view.o -c src/view.c

build/obj/write.o: src/write.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/write.o -c src/write.c

build/obj/filter.o: src/filter.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/filter.o -c src/filter.c

build/obj/types.o: src/types.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/types.o -c src/types.c

build/obj/document.o: src/document.c
	$(mkbuilddir)
	$(CC) $(CFLAGS) -o build/obj/document.o -c src/document.c

clean:
	rm -r build
