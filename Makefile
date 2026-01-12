.PHONY: all clean version1 version2 version3

all: version1 version2 version3

version1:
	@echo "Building Version 1: Prime Counter..."
	$(MAKE) -C version1_prime

version2:
	@echo "Building Version 2: Shared String..."
	$(MAKE) -C version2_string

version3:
	@echo "Building Version 3: File Simulation..."
	$(MAKE) -C version3_file

clean:
	$(MAKE) -C version1_prime clean
	$(MAKE) -C version2_string clean
	$(MAKE) -C version3_file clean
	@echo "All cleaned!"
