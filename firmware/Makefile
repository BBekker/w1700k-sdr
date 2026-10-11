# SPDX-License-Identifier: MIT
# Our MT7996 firmware set -> build/ (see README.md).
PYTHON ?= python3
BUILD ?= build
export PYTHONDONTWRITEBYTECODE := 1

.PHONY: all clean

all:
	$(PYTHON) build.py --out $(BUILD)

clean:
	rm -rf $(BUILD)
