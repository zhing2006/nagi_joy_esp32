#ifndef __TASKS_H__
#define __TASKS_H__

typedef struct joystick_info joystick_info_t;

/// @brief joystick.
extern joystick_info_t g_joystick;

/// @brief Main loop task.
/// @param pvParameters Task parameters.
void main_loop_task(void* pvParameters);

#endif  // __TASKS_H__