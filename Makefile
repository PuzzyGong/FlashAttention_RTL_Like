CXX ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -I.
D ?= 4
N ?= 4
override CPPFLAGS += -DHEAD_DIMENTION=$(D) -DN=$(N)
TARGET := flash_attention.exe
SRCS := main.cpp
PYTHON := ./.venv/Scripts/python.exe

.PHONY: all run trace view clean FORCE

all: $(TARGET)

$(TARGET): FORCE $(SRCS) rtl_like_attention/define.hpp rtl_like_attention/flash_array.hpp rtl_like_attention/cell_array.hpp rtl_like_attention/cell.hpp rtl_like_attention/cell_types.hpp mini_torch/torch.h
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SRCS) -o $(TARGET)

FORCE:

run: $(TARGET)
	./$(TARGET)

trace: $(TARGET)
	powershell -NoProfile -ExecutionPolicy Bypass -Command "& .\\$(TARGET) --trace trace.json; if (Test-Path trace.json) { exit 0 } else { exit 1 }"

view: trace
	$(PYTHON) visualize_trace.py trace.json

clean:
	$(RM) $(TARGET)
