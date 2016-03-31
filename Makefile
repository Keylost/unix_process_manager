include_dir = ./includes
src_dir = ./src
bin_dir = ./bin
CFLAGS = -std=c99 -D_XOPEN_SOURCE=700 -O0

all: lab
main.o:
	gcc $(CFLAGS) -c -I$(include_dir) $(src_dir)/main.c -o $(bin_dir)/main.o
common.o:
	gcc $(CFLAGS) -c -I$(include_dir) $(src_dir)/common.c -o $(bin_dir)/common.o
lab: main.o common.o
	gcc $(CFLAGS) -o $(bin_dir)/lab $(bin_dir)/main.o $(bin_dir)/common.o
clean:
	rm -rf $(bin_dir)/*.o
