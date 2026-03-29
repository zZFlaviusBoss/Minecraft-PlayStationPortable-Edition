# Minecraft PSP Port — Makefile (CMake wrapper)

BUILD_DIR = build
JOBS     ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: build clean help

build:
	mkdir -p $(BUILD_DIR)
	cmake -DCMAKE_TOOLCHAIN_FILE=.cmake/psp.cmake -B $(BUILD_DIR) -S .
	cmake --build $(BUILD_DIR) -j$(JOBS)
	cp -r res $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)

help:
	@echo "Targets:"
	@echo "  make build   → create build/, configure with CMake and compile the project"
	@echo "  make clean   → remove the build/ directory"
	@echo "  make help    → display this message"