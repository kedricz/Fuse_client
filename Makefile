CC=gcc
FUSE = sludge-fs
SLUDGE = sludge

build: $(FILES)
	$(CC) -g `pkg-config fuse --cflags --libs` $(FUSE).c -o $(FUSE)
	$(CC) -o $(SLUDGE) $(SLUDGE).c
	$(CC) -g `pkg-config fuse --cflags --libs` fuse-ec1.c -o ec1
	$(CC) -g `pkg-config fuse --cflags --libs` tgz-fuse.c -o tgz-fuse


clean:
	$(RM) $(FUSE) $(SLUDGE)
