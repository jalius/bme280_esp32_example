/*
 *   Permission to use, copy, modify, and/or distribute this software for
 *   any purpose with or without fee is hereby granted.
 *
 *   THE SOFTWARE IS PROVIDED “AS IS” AND THE AUTHOR DISCLAIMS ALL
 *   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 *   OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE
 *   FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY
 *   DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 *   AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 *   OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "rom/ets_sys.h"
#include "driver/i2c.h"
#include "common.h"

static const char* TAG = "bme280_esp32_example";
const gpio_num_t PIN_SDA = 5;
const gpio_num_t PIN_SCL = 6;
const uint32_t SELECTED_I2C_FREQUENCY = 100000;
const i2c_port_t SELECTED_I2C_PORT = I2C_NUM_0;

static void bme280_setup(struct bme280_dev* dev, uint32_t* meas_time_us);
static double C_to_F(double C);
static bool get_measurements(uint32_t meas_time_us, struct bme280_dev *dev, struct bme280_data *out, bool Fahrenheit);

static void bme280_setup(struct bme280_dev* dev, uint32_t* meas_time_us)
{
        if (dev == NULL){
                return;
        }	
        // set up i2c config object, pins, frequency, etc

        bme280_i2c_conf conf = { 
                .addr= BME280_I2C_ADDR_PRIM, //bme280_defines.h
                .i2c_port = SELECTED_I2C_PORT,
                .i2c_conf = {
                        .mode = I2C_MODE_MASTER,
                        .sda_io_num = PIN_SDA,
                        .sda_pullup_en = GPIO_PULLUP_ENABLE,
                        .scl_io_num = PIN_SCL,
                        .scl_pullup_en = GPIO_PULLUP_ENABLE,
                        .master.clk_speed = SELECTED_I2C_FREQUENCY
                                // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
                }	
        };

        bme280_init_i2c_dev(&conf, dev);

        bme280_init(dev);

        struct bme280_settings settings;
        /* Always read the current settings before writing, especially when all the configuration is not modified */
        bme280_get_sensor_settings(&settings, dev);

        /* Configuring the over-sampling rate, filter coefficient and standby time */
        /* Overwrite the desired settings */
        settings.filter = BME280_FILTER_COEFF_2;

        /* Over-sampling rate for humidity, temperature and pressure */
        settings.osr_h = BME280_OVERSAMPLING_1X;
        settings.osr_p = BME280_OVERSAMPLING_1X;
        settings.osr_t = BME280_OVERSAMPLING_1X;

        /* Setting the standby time */
        settings.standby_time = BME280_STANDBY_TIME_0_5_MS;

        bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, dev);

        /* Calculate measurement time in microseconds */
        bme280_cal_meas_delay(meas_time_us, &settings);

        // Normal Mode will continuously take measurements
        // Forced Mode will take 1 measurement and then go to sleep
        // 1 measurement period is meas_time_us(<9.3ms) plus standby time minimum (0.5ms)

        printf("\nHumidity calculation (Data displayed are compensated values)\n");
        printf("Measurement time : %lu us\n\n", (long unsigned int)*meas_time_us);
}
static double C_to_F(double C)
{
        return C * (9/5) + 32;
}
static bool get_measurements(uint32_t meas_time_us, struct bme280_dev *dev, struct bme280_data *out, bool Fahrenheit)
{
        // start the measurement in FORCED mode
        bme280_set_sensor_mode(BME280_POWERMODE_FORCED, dev);
        uint8_t status_reg;
        bme280_get_regs(BME280_REG_STATUS, &status_reg, 1, dev);
        // Check measurement status -> 1: running. 0: done
        if (status_reg & BME280_STATUS_MEAS_DONE){ 
                // wait for measurement to complete
                dev->delay_us(meas_time_us, dev->intf_ptr);
        }
        /* Read compensated data */
        bme280_get_sensor_data(BME280_ALL, out, dev);
        if (Fahrenheit)
        {
                out->temperature = C_to_F(out->temperature);
        }
        return true;
}
void app_main(void)
{
        ESP_LOGI(TAG, "sleep for 2ms, wait for BME280 init");
        ets_delay_us(2000);

        ESP_LOGI(TAG, "i2c_driver_install %d", I2C_NUM_0);
        ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

        static struct bme280_dev dev;
        static uint32_t meas_time_us = 10000; //10ms default, actual value calculated by bme280 API
        bme280_setup(&dev, &meas_time_us);
        while(1)
        {
                static struct bme280_data reading;

                // blocks for meas_time then gets data
                get_measurements(meas_time_us, &dev, &reading, true);
				printf("BME280 Readout -> Humidity %lf (%%RH), Temp %lf (F), Pressure %lf (bar)\n",
					reading.humidity, reading.temperature, reading.pressure);

        }
}
