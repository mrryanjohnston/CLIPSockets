all:
	make -C src/
NO_IMAGE_MAGICK:
	make -C src/ NO_IMAGE_MAGICK
clean:
	make -C src/ clean
	rm ./clips
