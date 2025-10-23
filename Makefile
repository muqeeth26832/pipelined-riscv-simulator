
rebuild:
	rm -rf build && \
	mkdir build && \
	cd build && \
	cmake .. && \
	make


refresh:
	cd build && cmake .. && make
