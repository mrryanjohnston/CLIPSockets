all:
	make -C src/
clean:
	make -C src/ clean
	rm ./clips
