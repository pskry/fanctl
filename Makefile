build:
	pio run

upload:
	pio run -t upload

monitor: upload
	pio device monitor

fmt format:
	./format.sh

init:
	pio project init --ide vim

clean:
	pio run -t clean
