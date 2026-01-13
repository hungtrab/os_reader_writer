.PHONY: all version2 clean

all: version2

version2:
	@echo "Building Shared String..."
	make -C src

clean:
	@echo "Cleaning..."
	make -C src clean
