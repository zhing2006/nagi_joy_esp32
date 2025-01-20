# nagi-joy-esp32
[![License](https://img.shields.io/badge/License-GPL3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0.en.html)

[English](README.md) | [中文](README_CN.md)

## 介绍
这是基于ESP32C6开发的，需要配合[nagi-joy-pc](https://github.com/zhing2006/nagi_joy_pc)使用的游戏控制器固件。通过UDP协议与`nagi-joy-pc`通讯。

## 开发
ESP-IDF v5.3.2以上版本。

## 使用
单片机操作使用了`esp_console_repl`，可以在控制台输入`help`查看完整命令列表。

WS2812指示状态：
- 粉色：初始化中
- 蓝色：连接Wi-Fi中
- 绿色：成功连接Wi-Fi并于主机通讯中
- 红色：连接Wi-Fi失败
