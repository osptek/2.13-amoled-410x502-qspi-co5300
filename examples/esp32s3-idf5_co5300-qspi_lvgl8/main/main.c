#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl.h"
#include "lv_demos.h"

#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst820.h"

static const char *TAG = "example";
static SemaphoreHandle_t lvgl_mux = NULL;

#define EXAMPLE_LCD_HOST    SPI2_HOST
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)

#define EXAMPLE_TOUCH_I2C_NUM  I2C_NUM_0
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400 * 1000)

#define LCD_BIT_PER_PIXEL       (16)

#define EXAMPLE_LCD_H_RES              410
#define EXAMPLE_LCD_V_RES              502
#define EXAMPLE_LCD_X_GAP              22
#define EXAMPLE_LCD_Y_GAP              0

#define EXAMPLE_PIN_NUM_LCD_CS            (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_PCLK          (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_DATA0         (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA1         (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA2         (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA3         (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_RST           (GPIO_NUM_15)
#define EXAMPLE_PIN_NUM_LCD_TE            (GPIO_NUM_16) //未使用

#define EXAMPLE_PIN_NUM_TOUCH_SCL         (GPIO_NUM_42)
#define EXAMPLE_PIN_NUM_TOUCH_SDA         (GPIO_NUM_41)
#define EXAMPLE_PIN_NUM_TOUCH_RST         (GPIO_NUM_40)
#define EXAMPLE_PIN_NUM_TOUCH_INT         (GPIO_NUM_39)

esp_lcd_touch_handle_t tp = NULL;

#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    // 将缓冲区的内容复制到显示的特定区域
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

void lvgl_port_rounder_callback(struct _lv_disp_drv_t * disp_drv, lv_area_t * area)
{
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;

    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

/* 在 LVGL 中旋转屏幕时，旋转显示和触摸。 更新驱动程序参数时调用 */
static void example_lvgl_update_cb(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t) drv->user_data;

    switch (drv->rotated) {
    case LV_DISP_ROT_NONE:
        // 旋转液晶显示屏
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, false);
        // 旋转液晶触摸
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case LV_DISP_ROT_90:
        // 旋转液晶显示屏
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, false);
        // 旋转液晶触摸
        esp_lcd_touch_set_swap_xy(tp, false);
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    case LV_DISP_ROT_180:
        // 旋转液晶显示屏
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, true);
        // 旋转液晶触摸
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, false);
        esp_lcd_touch_set_mirror_y(tp, false);
        break;
    case LV_DISP_ROT_270:
        // 旋转液晶显示屏
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, true);
        // 旋转液晶触摸
        esp_lcd_touch_set_swap_xy(tp, false);   
        esp_lcd_touch_set_mirror_x(tp, true);
        esp_lcd_touch_set_mirror_y(tp, true);
        break;
    }
}

static SemaphoreHandle_t touch_mux = NULL;

static void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *lv_data)
{
    esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)drv->user_data;
    assert(tp);

    // 创建结构体用于存储触点信息
    esp_lcd_touch_point_data_t point;
    uint8_t tp_cnt = 0;

    /* 1. 读取硬件原始数据 */
    if (xSemaphoreTake(touch_mux, 0) == pdTRUE) {
        esp_lcd_touch_read_data(tp);
        xSemaphoreGive(touch_mux); // 必须释放，防止死锁
    }

    /* 2. 获取解析后的触点数据 */
    // 这里我们只请求 1 个点 (max_point_cnt = 1)
    esp_err_t err = esp_lcd_touch_get_data(tp, &point, &tp_cnt, 1);

    /* 3. 反馈给 LVGL */
    if (err == ESP_OK && tp_cnt > 0) {
        lv_data->point.x = point.x;
        lv_data->point.y = point.y;
        lv_data->state = LV_INDEV_STATE_PRESSED;
        
        // printf("x:%d,y:%d\n", point.x, point.y);
    } else {
        lv_data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void example_touch_isr_cb(esp_lcd_touch_handle_t tp)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(touch_mux, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


static void example_increase_lvgl_tick(void *arg)
{
    /* 告诉 LVGL 已经过去了多少毫秒*/
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // 由于 LVGL API 不是线程安全的，因此锁定互斥体
        if (example_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            // 释放互斥锁
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// co5300
static const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x00}, 1, 0},
    {0x11, (uint8_t []){0x00}, 0, 120},
    //{0x35, (uint8_t []){0x00}, 1, 0},
    {0xFE, (uint8_t []){0x00}, 1, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0x53, (uint8_t []){0x20}, 1, 0},
    {0x63, (uint8_t []){0xFF}, 1, 0},
    {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xBF}, 4, 0},
    {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x6F}, 4, 0},
    {0x29, (uint8_t []){0x00}, 0, 60},
    {0x51, (uint8_t []){0xFF}, 1, 0},
    {0x58, (uint8_t []){0x07}, 1, 10},
};

void app_main(void)
{
    static lv_disp_draw_buf_t disp_buf; // 包含称为绘制缓冲区的内部图形缓冲区
    static lv_disp_drv_t disp_drv;      //包含回调函数

    ESP_LOGI(TAG, "Initialize SPI bus");
    const spi_bus_config_t buscfg = CO5300_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                 EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                 EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);

    ESP_ERROR_CHECK(spi_bus_initialize(EXAMPLE_LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = {                                     
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,                                      
        .dc_gpio_num = -1,                                      
        .spi_mode = 0,                                          
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ, //CO5300_MAX=50MHZ                           
        .trans_queue_depth = 10,                                
        .on_color_trans_done = example_notify_lvgl_flush_ready,                              
        .user_ctx = &disp_drv,                                     
        .lcd_cmd_bits = 32,                                     
        .lcd_param_bits = 8,                                    
        .flags = {                                              
            .quad_mode = true,                                  
        },                                                      
    };

    co5300_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds, // 如果使用自定义初始化命令，请取消注释这些行
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(co5300_lcd_init_cmd_t),
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    // 将 LCD 连接到 SPI 总线
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EXAMPLE_LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_handle_t panel_handle = NULL;
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Install CO5300 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, EXAMPLE_LCD_X_GAP, EXAMPLE_LCD_Y_GAP));
    // 在打开屏幕或背光之前，用户可以将预定义的图案刷新到屏幕上
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_co5300_set_brightness(panel_handle, 80));

    ESP_LOGI(TAG, "Initialize I2C bus");
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = EXAMPLE_TOUCH_I2C_NUM,
        .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
        .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
    tp_io_config.scl_speed_hz = EXAMPLE_TOUCH_I2C_CLK_HZ;

    // Attach the TOUCH to the I2C bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle));

    touch_mux = xSemaphoreCreateBinary();
    assert(touch_mux);

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES,
        .y_max = EXAMPLE_LCD_V_RES,
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
        .interrupt_callback = example_touch_isr_cb,
    };

    ESP_LOGI(TAG, "Initialize touch controller CST820");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(tp_io_handle, &tp_cfg, &tp));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // 分配 LVGL 使用的绘制缓冲区
    // 建议选择绘制缓冲区的大小至少为屏幕大小的 1/10
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LCD_H_RES * (EXAMPLE_LCD_V_RES/10) * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LCD_H_RES * (EXAMPLE_LCD_V_RES/10) * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // 初始化 LVGL 绘制缓冲区
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LCD_H_RES * (EXAMPLE_LCD_V_RES/10));

    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb;
    disp_drv.rounder_cb = lvgl_port_rounder_callback;
    disp_drv.drv_update_cb = example_lvgl_update_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // 设置背景颜色为黑色
    lv_obj_t *default_scr = lv_disp_get_scr_act(disp);     // 获取当前活动屏幕
    if (default_scr) {
        lv_obj_remove_style_all(default_scr);               // 清除默认样式
        lv_obj_set_style_bg_color(default_scr, lv_color_black(), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(default_scr, LV_OPA_COVER, LV_STATE_DEFAULT);
        lv_obj_set_size(default_scr, LV_PCT(100), LV_PCT(100));
    }

    ESP_LOGI(TAG, "Install LVGL tick timer");
    // LVGL 的 Tick 接口（使用 esp_timer 生成 2ms 周期性事件）
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    static lv_indev_drv_t indev_drv;    // 输入设备驱动程序（触摸）
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    indev_drv.user_data = tp;
    
    lv_indev_drv_register(&indev_drv);

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Display LVGL demos");
    //由于 LVGL API 不是线程安全的，因此锁定互斥体
    if (example_lvgl_lock(-1)) {

       // lv_disp_set_rotation(NULL, LV_DISP_ROT_180);

        lv_demo_widgets();      /* 小部件示例 */
        //lv_demo_music();        /* 类似智能手机的现代音乐播放器演示 */
        // lv_demo_stress();       /* LVGL 压力测试 */
        // lv_demo_benchmark();    /* 用于测量 LVGL 性能或比较不同设置的演示 */

        // 释放互斥锁
        example_lvgl_unlock();
    }
}