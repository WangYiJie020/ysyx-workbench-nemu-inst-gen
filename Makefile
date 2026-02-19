VENV := .
PYTHON := $(VENV)/bin/python

BUILD_DIR = ./build

$(shell mkdir -p $(BUILD_DIR))

OUT_CC = $(BUILD_DIR)/out.cc

# TESTCXXSRCS = ./IDLHlper.cc
TESTCXXSRCS = $(shell find $(BUILD_DIR) -name '*.cc')
TESTCXXSRCS += ./foomain.cc


default: geninc

testcompile: IDLHlper.hpp $(OUT_CC)
	clang++ -I$(BUILD_DIR) $(TESTCXXSRCS) -o $(BUILD_DIR)/testcompile
	rm $(BUILD_DIR)/testcompile

$(OUT_CC): gen.py
	$(PYTHON) gen.py > $(OUT_CC)

geninc: $(OUT_CC)
	cp ./encoding.out.h $(BUILD_DIR)/
	cp ./IDLHlper.cc $(BUILD_DIR)/
	cp ./IDLHlper.hpp $(BUILD_DIR)/

clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: clean testcompile geninc
