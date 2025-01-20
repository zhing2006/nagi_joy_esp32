idf_component_register(
  SRCS "peripherals/encoder.c" "peripherals/button.c" "peripherals/axis.c" "tasks.c" "modules/udp.c" "global.c" "main.c" "commands.c" "peripherals/led_ws2812.c" "modules/wifi.c"
  INCLUDE_DIRS "." "./peripherals" "./modules"
)