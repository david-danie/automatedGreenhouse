# SmartPlant — firmware ESP-IDF (migración desde Arduino-ESP32).
# Proyecto IDF puro. La lógica de negocio se porta desde ESP32_controller/ (Arduino),
# que se conserva en el repo como especificación de referencia durante la migración.
#
# Build:  idf.py set-target esp32c3 && idf.py build
# Flash:  idf.py -p <puerto> flash monitor   (primer flasheo: idf.py erase-flash antes)
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(smartplant)
