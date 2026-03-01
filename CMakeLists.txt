# Adafruit FT6206 Library
# MIT License

cmake_minimum_required(VERSION 3.5)

idf_component_register(SRCS "Adafruit_FT6206.cpp" INCLUDE_DIRS "." REQUIRES arduino-esp32 Adafruit_BusIO)

project(Adafruit_FT6206)
