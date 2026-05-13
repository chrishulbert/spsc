help:
	cat Makefile

build:
	# This works on macOS using clang, likely also on Linux.
	# -O3: Full speed optimisation.
	# -march=native: Take advantage of every optimisation this CPU allows.
	# -DNDEBUG: No debug.
	# -lpthread -pthread: Allow threads.
	# -std=c23: Use latest C syntax.
	cc \
		-O3 \
		-march=native \
		-DNDEBUG \
		-lpthread \
		-pthread \
		-std=c23 \
		*.c \
		-o spsc

run: build
	./spsc
