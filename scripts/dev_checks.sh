#!/usr/bin/env bash

echo "Running clang-format..."
clang-format -i $(find include src -name "*.hpp" -o -name "*.cpp")

echo "Running clang-tidy..."
clang-tidy -p build $(find src -name "*.cpp")