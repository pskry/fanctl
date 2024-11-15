build:
	pio run

upload:
	pio run -t upload

monitor:
	pio device monitor

init:
	pio project init --ide vim

clean:
	pio run -t clean
