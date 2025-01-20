#ifndef __AXIS_H__
#define __AXIS_H__

// @brief The axis data.
extern uint16_t g_axes_data[NAGI_MAX_NUM_OF_AXES];

// @brief Initialize the axis module.
void initialize_axis(void);

// @brief Deinitialize the axis module.
void deinitialize_axis(void);

// @brief Start the axis module.
void start_axis(void);

// @brief Stop the axis module.
void stop_axis(void);

// @brief Read the axis data.
void read_axis(void);

#endif // __AXIS_H__