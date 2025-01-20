# nagi-joy-esp32
[![License](https://img.shields.io/badge/License-GPL3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.en.html)

[English](README.md) | [中文](README_CN.md)

## Introduction
This is a game controller firmware based on ESP32C6, which needs to be used with [nagi-joy-pc](https://github.com/zhing2006/nagi_joy_pc). It communicates with `nagi-joy-pc` via the UDP protocol.

## Development
ESP-IDF version 5.3.2 or above.

## Usage
The microcontroller operation uses `esp_console_repl`, and you can enter `help` in the console to view the complete list of commands.

WS2812 status indicators:
- Pink: Initializing
- Blue: Connecting to Wi-Fi
- Green: Successfully connected to Wi-Fi and communicating with the host
- Red: Failed to connect to Wi-Fi