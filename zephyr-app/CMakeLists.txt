# Copyright (c) 2021 Nicholas Winans
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.13.1)
find_package(Zephyr)
project(raw-hid)

target_sources(app PRIVATE src/main.c)
