/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief BSP LCD
 *
 * This file offers API for basic LCD control.
 * It is useful for users who want to use the LCD without the default Graphical Library LVGL.
 *
 * For standard LCD initialization with LVGL graphical library, you can call all-in-one function bsp_display_start().
 */

#pragma once
#include "esp_lcd_types.h"
#include "driver/gpio.h"

/* LCD color formats */
#define ESP_LCD_COLOR_FORMAT_RGB565    (1)
#define ESP_LCD_COLOR_FORMAT_RGB888    (2)

/* LCD display color format */
#define BSP_LCD_COLOR_FORMAT        (ESP_LCD_COLOR_FORMAT_RGB565)
/* LCD display color bits */
#define BSP_LCD_BITS_PER_PIXEL      (16)
/* LCD definition */

#define EXAMPLE_LCD_QSPI_H_RES      (410)
#define EXAMPLE_LCD_QSPI_V_RES      (502)

#define EXAMPLE_LCD_X_GAP              22
#define EXAMPLE_LCD_Y_GAP              0

/**
 * @brief Tear configuration structure
 *
 */
#define BSP_SYNC_TASK_CONFIG(te_io, intr_type)  \
    {                                           \
        .task_priority = 4,                     \
        .task_stack = 2048,                     \
        .task_affinity = -1,                    \
        .time_Tvdl = 13,                        \
        .time_Tvdh = 3,                         \
        .te_gpio_num = te_io,                   \
        .tear_intr_type = intr_type,            \
    }

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BSP display configuration structure
 *
 */
typedef struct {
    int max_transfer_sz;    /*!< Maximum transfer size, in bytes. */
    struct {
        int task_priority;          /*!< Tear task priority */
        int task_stack;             /*!< Tear task stack size */
        int task_affinity;          /*!< Tear task pinned to core (-1 is no affinity) */
        uint32_t time_Tvdl;         /*!< The display panel is updated from the Frame Memory, Reference specifications */
        uint32_t time_Tvdh;         /*!< The display panel is not updated from the Frame Memory, Reference specifications */
        int te_gpio_num;            /*!< Tear gpio num */
        gpio_int_type_t tear_intr_type;  /*!< Tear intr type */
    } tear_cfg;
} bsp_display_config_t;

/**
 * @brief Create new display panel
 *
 * For maximum flexibility, this function performs only reset and initialization of the display.
 *
 * If you want to free resources allocated by this function, you can use esp_lcd API, ie.:
 *
 * \code{.c}
 * esp_lcd_panel_del(panel);
 * esp_lcd_panel_io_del(io);
 * spi_bus_free(spi_num_from_configuration);
 * \endcode
 *
 * @param[in]  config    display configuration
 * @param[out] ret_panel esp_lcd panel handle
 * @param[out] ret_io    esp_lcd IO handle
 * @return
 *      - ESP_OK         On success
 *      - Else           esp_lcd failure
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel, esp_lcd_panel_io_handle_t *ret_io);

#ifdef __cplusplus
}
#endif
