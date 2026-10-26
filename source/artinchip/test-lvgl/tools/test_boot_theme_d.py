#!/usr/bin/env python3
"""Run real LVGL tests for original A boot and self-check."""
import sys
sys.dont_write_bytecode = True
from boot_native_test_support import main

if __name__ == "__main__":
    main(1)
