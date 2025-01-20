#include <stdint.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "axis.h"

#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#if NAGI_AXIS_USE_ADC_CONTINUOUS
#include "esp_adc/adc_continuous.h"
#include "esp_adc/adc_filter.h"
#else
#include "esp_adc/adc_oneshot.h"
#endif
#include "esp_log.h"

static const char* TAG = "axis";

// @brief The ADC calibration handle for ADC1 channel 1.
static adc_cali_handle_t g_adc1_cali_chan1_handle = NULL;
#if NAGI_AXIS_USE_ADC_CONTINUOUS
// @brief The ADC continuous handle for ADC1.
static adc_continuous_handle_t g_adc1_cont_handle = NULL;
// @brief The ADC IIR filter handle for ADC1.
static adc_iir_filter_handle_t g_adc1_iir_filter_handle = NULL;

static uint8_t g_conv_results[8 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NAGI_MAX_NUM_OF_AXES];
#else
// @brief The ADC oneshot handle for ADC1.
static adc_oneshot_unit_handle_t g_adc1_oneshot_handle = NULL;

#define FILTER_WINDOW_SIZE 4
#define FILTER_ALPHA_SHIFT (FILTER_WINDOW_SIZE - 1)
static int g_adc1_raw[NAGI_MAX_NUM_OF_AXES];
static int g_adc1_voltage[NAGI_MAX_NUM_OF_AXES][FILTER_WINDOW_SIZE];
static int g_exp_weights_filter_index = 0;
#endif

// @brief The axis data.
uint16_t g_axes_data[NAGI_MAX_NUM_OF_AXES];

// @brief Initialize the axis module.
void initialize_axis(void) {
  // Initialize the ADC calibration.
  adc_unit_t unit = ADC_UNIT_1;
  adc_channel_t channel = ADC_CHANNEL_1;
  adc_atten_t atten = ADC_ATTEN_DB_12;

  adc_cali_curve_fitting_config_t cali_config = {
    .unit_id = unit,
    .chan = channel,
    .atten = atten,
    .bitwidth = ADC_BITWIDTH_DEFAULT,
  };

  ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &g_adc1_cali_chan1_handle));

#if NAGI_AXIS_USE_ADC_CONTINUOUS
  // Initialize the ADC continuous.
  adc_continuous_handle_cfg_t adc_config = {
    .max_store_buf_size = 8 * 2 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NAGI_MAX_NUM_OF_AXES,
    .conv_frame_size = 8 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV * NAGI_MAX_NUM_OF_AXES,
  };

  ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &g_adc1_cont_handle));

  adc_continuous_config_t adc_cont_config = {
    .sample_freq_hz = 4 * 1000,
    .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
  };

  adc_digi_pattern_config_t adc_pattern = {
    .atten = ADC_ATTEN_DB_12,
    .bit_width = ADC_BITWIDTH_12,
    .channel = ADC_CHANNEL_1,
    .unit = ADC_UNIT_1,
  };

  adc_cont_config.pattern_num = 1;
  adc_cont_config.adc_pattern = &adc_pattern;

  ESP_ERROR_CHECK(adc_continuous_config(g_adc1_cont_handle, &adc_cont_config));

  adc_continuous_iir_filter_config_t adc_iir_filter_config = {
    .unit = ADC_UNIT_1,
    .channel = ADC_CHANNEL_1,
    .coeff = ADC_DIGI_IIR_FILTER_COEFF_8,
  };

  ESP_ERROR_CHECK(adc_new_continuous_iir_filter(g_adc1_cont_handle, &adc_iir_filter_config, &g_adc1_iir_filter_handle));
  ESP_ERROR_CHECK(adc_continuous_iir_filter_enable(g_adc1_iir_filter_handle));

  // Fill the conversion results with 0x0.
  memset(g_conv_results, 0x0, sizeof(g_conv_results));
#else
  // Initialize the ADC oneshot.
  adc_oneshot_unit_init_cfg_t adc_oneshot_config = {
    .unit_id = ADC_UNIT_1,
  };

  ESP_ERROR_CHECK(adc_oneshot_new_unit(&adc_oneshot_config, &g_adc1_oneshot_handle));

  adc_oneshot_chan_cfg_t adc_oneshot_chan_config = {
    .bitwidth = ADC_BITWIDTH_12,
    .atten = ADC_ATTEN_DB_12,
  };

  ESP_ERROR_CHECK(adc_oneshot_config_channel(g_adc1_oneshot_handle, ADC_CHANNEL_1, &adc_oneshot_chan_config));

  // Fill the axis data with 0xFFFF.
  memset(g_axes_data, 0xFF, sizeof(uint16_t) * NAGI_MAX_NUM_OF_AXES);
#endif
}

// @brief Deinitialize the axis module.
void deinitialize_axis(void) {
#if NAGI_AXIS_USE_ADC_CONTINUOUS
  // Deinitialize the ADC IIR filter.
  if (g_adc1_iir_filter_handle != NULL) {
    adc_continuous_iir_filter_disable(g_adc1_iir_filter_handle);
    adc_del_continuous_iir_filter(g_adc1_iir_filter_handle);
    g_adc1_iir_filter_handle = NULL;
  }
  // Deinitialize the ADC continuous.
  if (g_adc1_cont_handle != NULL) {
    adc_continuous_deinit(g_adc1_cont_handle);
    g_adc1_cont_handle = NULL;
  }
#else
  // Deinitialize the ADC oneshot.
  if (g_adc1_oneshot_handle != NULL) {
    adc_oneshot_del_unit(g_adc1_oneshot_handle);
    g_adc1_oneshot_handle = NULL;
  }
#endif

  // Deinitialize the ADC calibration.
  if (g_adc1_cali_chan1_handle != NULL) {
    adc_cali_delete_scheme_curve_fitting(g_adc1_cali_chan1_handle);
    g_adc1_cali_chan1_handle = NULL;
  }
}

// @brief Start the axis module.
void start_axis(void) {
#if NAGI_AXIS_USE_ADC_CONTINUOUS
  // Start the ADC continuous.
  ESP_ERROR_CHECK(adc_continuous_start(g_adc1_cont_handle));
#endif
}

// @brief Stop the axis module.
void stop_axis(void) {
#if NAGI_AXIS_USE_ADC_CONTINUOUS
  // Stop the ADC continuous.
  ESP_ERROR_CHECK(adc_continuous_stop(g_adc1_cont_handle));
#endif
}

#if !NAGI_AXIS_USE_ADC_CONTINUOUS
void oneshot_read_axis(uint32_t chan_num);
#endif
// @brief Read the axis data.
void read_axis(void) {
#if NAGI_AXIS_USE_ADC_CONTINUOUS
  // Read the ADC continuous data.
  uint32_t ret_num = 0;
  esp_err_t ret = adc_continuous_read(g_adc1_cont_handle, g_conv_results, sizeof(g_conv_results), &ret_num, 0);
  if (ret == ESP_OK) {
    static int chan_data[NAGI_MAX_NUM_OF_AXES];
    static int chan_count[NAGI_MAX_NUM_OF_AXES];
    memset(chan_data, 0, sizeof(chan_data));
    memset(chan_count, 0, sizeof(chan_count));
    for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
      adc_digi_output_data_t *p = (adc_digi_output_data_t*)&g_conv_results[i];
      uint32_t chan_num = p->type2.channel;
      uint32_t data = p->type2.data;
      if (chan_num < SOC_ADC_CHANNEL_NUM(ADC_UNIT_1)) {
        // The first axis is the ADC1 channel 1, we don't use channel 0.
        if (chan_num - ADC_CHANNEL_1 < NAGI_MAX_NUM_OF_AXES) {
          // Convert the raw data to the calibrated data.
          int voltage;
          ret = adc_cali_raw_to_voltage(g_adc1_cali_chan1_handle, data, &voltage);
          if (ret == ESP_OK) {
            chan_data[chan_num - ADC_CHANNEL_1] += voltage;
            chan_count[chan_num - ADC_CHANNEL_1]++;
          } else {
            ESP_LOGE(TAG, "Error occurred during converting the raw data to voltage. Error %s", esp_err_to_name(ret));
          }
        } else {
          ESP_LOGW(TAG, "Invalid ADC channel number %lu", chan_num);
        }
      } else {
        ESP_LOGW(TAG, "Invalid ADC channel number %lu", chan_num);
      }
    }
    for (int i = 0; i < NAGI_MAX_NUM_OF_AXES; i++) {
      if (chan_count[i] > 0) {
        int voltage = chan_data[i] / chan_count[i];
        if (abs(voltage - (int)g_axes_data[i]) > NAGI_AXIS_JITTER_THRESHOLD) {
          g_axes_data[i] = voltage;
        }
      }
    }
  } else if (ret == ESP_ERR_TIMEOUT) {
    // Timeout means no data is available.
  } else {
    ESP_LOGE(TAG, "Error occurred during reading the ADC continuous. Error %s", esp_err_to_name(ret));
  }
}
#else
  oneshot_read_axis(ADC_CHANNEL_1);
}

void oneshot_read_axis(uint32_t chan_num) {
  // Read the ADC oneshot data.
  esp_err_t ret = adc_oneshot_read(g_adc1_oneshot_handle, chan_num, &g_adc1_raw[chan_num - ADC_CHANNEL_1]);
  if (ret == ESP_OK) {
    ret = adc_cali_raw_to_voltage(g_adc1_cali_chan1_handle, g_adc1_raw[chan_num - ADC_CHANNEL_1], &g_adc1_voltage[chan_num - ADC_CHANNEL_1][g_exp_weights_filter_index]);
    if (ret == ESP_OK) {
      g_exp_weights_filter_index = (g_exp_weights_filter_index + 1) % FILTER_WINDOW_SIZE;

      int sum = 0;
      int weight = 1 << FILTER_ALPHA_SHIFT;
      int total_weight = 0;
      for (uint8_t i = 0; i < FILTER_WINDOW_SIZE; i++) {
        int idx = (g_exp_weights_filter_index - i - 1 + FILTER_WINDOW_SIZE) % FILTER_WINDOW_SIZE;
        sum += g_adc1_voltage[chan_num - ADC_CHANNEL_1][idx] * weight;
        total_weight += weight;
        weight >>= 1;
      }

      int voltage = sum / total_weight;
      if (abs(voltage - (int)g_axes_data[chan_num - ADC_CHANNEL_1]) > NAGI_AXIS_JITTER_THRESHOLD) {
        g_axes_data[chan_num - ADC_CHANNEL_1] = voltage;
      }
    } else {
      ESP_LOGE(TAG, "Error occurred during converting the raw data to voltage. Error %s", esp_err_to_name(ret));
    }
  } else {
    ESP_LOGE(TAG, "Error occurred during reading the ADC oneshot. Error %s", esp_err_to_name(ret));
  }
}
#endif