#!/bin/bash
# Copyright 2025 Sandia National Laboratories
find -iname '*.h' -or -iname '*.c' -or -iname '*.hpp' -or -iname '*.cpp' | xargs clang-format -i
