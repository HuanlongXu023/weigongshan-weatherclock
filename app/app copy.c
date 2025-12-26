#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "workqueue.h"
#include "rtc.h"
#include "aht20.h"
#include "esp_at.h"
#include "weather.h"
#include "page.h"
#include "app.h"
 
/* ============================================================================
 * 时间单位转换宏定义
 * ============================================================================ */
#define MILLISECONDS(x) (x)                          // 毫秒
#define SECONDS(x)      MILLISECONDS((x) * 1000)     // 秒转毫秒
#define MINUTES(x)      SECONDS((x) * 60)            // 分钟转毫秒
#define HOURS(x)        MINUTES((x) * 60)            // 小时转毫秒
#define DAYS(x)          HOURS((x) * 24)             // 天转毫秒

/* ============================================================================
 * 定时任务间隔配置
 * ============================================================================ */
#define TIME_SYNC_INTERVAL          HOURS(1)         // 时间同步间隔：1小时同步一次网络时间
#define WIFI_UPDATE_INTERVAL        SECONDS(5)       // WiFi状态检查间隔：5秒检查一次WiFi连接状态
#define TIME_UPDATE_INTERVAL        SECONDS(1)       // 时间显示更新间隔：1秒更新一次时间显示
#define INNER_UPDATE_INTERVAL       SECONDS(3)       // 室内温湿度更新间隔：3秒读取一次AHT20传感器
#define OUTDOOR_UPDATE_INTERVAL     MINUTES(1)       // 室外天气更新间隔：1分钟获取一次天气数据

/* ============================================================================
 * 主循环事件位定义（未使用，保留用于扩展）
 * ============================================================================ */
#define MLOOP_EVT_TIME_SYNC         (1 << 0)         // 时间同步事件
#define MLOOP_EVT_WIFI_UPDATE       (1 << 1)         // WiFi更新事件
#define MLOOP_EVT_INNER_UPDATE      (1 << 2)         // 室内数据更新事件
#define MLOOP_EVT_OUTDOOR_UPDATE    (1 << 3)         // 室外数据更新事件
#define MLOOP_EVT_ALL               (MLOOP_EVT_TIME_SYNC | \
                                     MLOOP_EVT_WIFI_UPDATE | \
                                     MLOOP_EVT_INNER_UPDATE | \
                                     MLOOP_EVT_OUTDOOR_UPDATE) // 所有事件

/* ============================================================================
 * FreeRTOS定时器句柄
 * ============================================================================ */
static TimerHandle_t time_sync_timer;       // 时间同步定时器（1小时）
static TimerHandle_t wifi_update_timer;     // WiFi状态监控定时器（5秒）
static TimerHandle_t time_update_timer;     // 时间显示更新定时器（1秒）
static TimerHandle_t inner_update_timer;    // 室内温湿度更新定时器（3秒）
static TimerHandle_t outdoor_update_timer;  // 室外天气更新定时器（1分钟）

/* ============================================================================
 * 函数：time_sync
 * 功能：通过SNTP协议从网络同步时间到RTC
 * 说明：定时器回调函数，周期性执行
 * ============================================================================ */
static void time_sync(void)
{
    uint32_t restart_sync_delay = TIME_SYNC_INTERVAL;  // 默认下次同步延迟1小时
    rtc_date_time_t rtc_date = { 0 };                  // RTC时间结构体

    esp_date_time_t esp_date = { 0 };                  // ESP获取的网络时间结构体
    
    // 尝试从SNTP服务器获取时间
    if (!esp_at_sntp_get_time(&esp_date))
    {
        printf("[SNTP] get time failed\n");
        restart_sync_delay = SECONDS(1);  // 获取失败，1秒后重试
        goto err;
    }
    
    // 校验获取的时间是否有效（年份应大于2000）
    if (esp_date.year < 2000)
    {
        printf("[SNTP] invalid date formate\n");
        restart_sync_delay = SECONDS(1);  // 时间格式无效，1秒后重试
        goto err;
    }
    
    // 打印同步到的网络时间
    printf("[SNTP] sync time: %04u-%02u-%02u %02u:%02u:%02u (%d)\n",
        esp_date.year, esp_date.month, esp_date.day,
        esp_date.hour, esp_date.minute, esp_date.second, esp_date.weekday);
    
    // 将ESP时间结构转换为RTC时间结构
    rtc_date.year = esp_date.year;
    rtc_date.month = esp_date.month;
    rtc_date.day = esp_date.day;
    rtc_date.hour = esp_date.hour;
    rtc_date.minute = esp_date.minute;
    rtc_date.second = esp_date.second;
    rtc_date.weekday = esp_date.weekday;
    
    // 将时间写入RTC芯片
    rtc_set_time(&rtc_date);
    
err:
    // 修改定时器周期（成功则1小时后重新同步，失败则1秒后重试）
    xTimerChangePeriod(time_sync_timer, pdMS_TO_TICKS(restart_sync_delay), 0);
}

/* ============================================================================
 * 函数：wifi_update
 * 功能：监控WiFi连接状态，检测连接/断开事件并更新显示
 * 说明：定时器回调函数，每5秒执行一次
 * ============================================================================ */
static void wifi_update(void)
{
    static esp_wifi_info_t last_info = { 0 };  // 保存上次的WiFi信息，用于比较变化

    esp_wifi_info_t info = { 0 };               // 当前WiFi信息
    
    // 获取当前WiFi状态信息
    if (!esp_at_get_wifi_info(&info))
    {
        printf("[AT] wifi info get failed\n");
        return;
    }
    
    // 如果WiFi信息完全相同，无需处理
    if (memcmp(&info, &last_info, sizeof(esp_wifi_info_t)) == 0)
    {
        return;
    }
    
    // 如果连接状态没有变化，无需处理
    if (last_info.connected == info.connected)
    {
        return;
    }
    
    // 检测到WiFi连接状态变化
    if (info.connected)
    {
        // WiFi已连接
        printf("[WIFI] connected to %s\n", info.ssid);
        printf("[WIFI] SSID: %s, BSSID: %s, Channel: %d, RSSI: %d\n",
                info.ssid, info.bssid, info.channel, info.rssi);
        
        // 更新屏幕显示：显示连接的WiFi名称
        main_page_redraw_wifi_ssid(info.ssid);
    }
    else
    {
        // WiFi已断开
        printf("[WIFI] disconnected from %s\n", last_info.ssid);
        
        // 更新屏幕显示：显示"wifi lost"
        main_page_redraw_wifi_ssid("wifi lost");
    }
    
    // 保存当前信息，用于下次比较
    memcpy(&last_info, &info, sizeof(esp_wifi_info_t));
}

/* ============================================================================
 * 函数：time_update
 * 功能：从RTC读取当前时间并更新屏幕显示
 * 说明：定时器回调函数，每1秒执行一次
 * ============================================================================ */
static void time_update(void)
{
    static rtc_date_time_t last_date = { 0 };  // 保存上次的时间，用于比较变化
    
    rtc_date_time_t date;                       // 当前时间
    rtc_get_time(&date);                        // 从RTC读取当前时间
    
    // 如果RTC时间无效（年份小于2020），不更新显示
    if (date.year < 2020)
    {
        return;
    }
    
    // 如果时间没有变化，不更新显示（避免不必要的屏幕刷新）
    if (memcmp(&date, &last_date, sizeof(rtc_date_time_t)) == 0)
    {
        return;
    }
    
    // 保存当前时间
    memcpy(&last_date, &date, sizeof(rtc_date_time_t));
    
    // 更新屏幕显示的时间和日期
    main_page_redraw_time(&date);
    main_page_redraw_date(&date);
}

/* ============================================================================
 * 函数：inner_update
 * 功能：读取AHT20温湿度传感器数据并更新屏幕显示
 * 说明：定时器回调函数，每3秒执行一次
 * ============================================================================ */
static void inner_update(void)
{
    static float last_temperature, last_humidity;  // 保存上次的温湿度，用于比较变化
    
    // 启动AHT20温湿度测量
    if (!aht20_start_measurement())
    {
        printf("[AHT20] start measurement failed\n");
        return;
    }
    
    // 等待AHT20测量完成
    if (!aht20_wait_for_measurement())
    {
        printf("[AHT20] wait for measurement failed\n");
        return;
    }
    
    float temperature = 0.0f, humidity = 0.0f;  // 温度和湿度变量
    
    // 读取测量结果
    if (!aht20_read_measurement(&temperature, &humidity))
    {
        printf("[AHT20] read measurement failed\n");
        return;
    }
    
    // 如果温湿度没有变化，不更新显示
    if (temperature == last_temperature && humidity == last_humidity)
    {
        return;
    }
    
    // 保存当前值
    last_temperature = temperature;
    last_humidity = humidity;
    
    // 打印温湿度数据
    printf("[AHT20] Temperature: %.1f, Humidity: %.1f\n", temperature, humidity);
    
    // 更新屏幕显示的室内温度和湿度
    main_page_redraw_inner_temperature(temperature);
    main_page_redraw_inner_humidity(humidity);
}

/* ============================================================================
 * 函数：outdoor_update
 * 功能：通过HTTP请求获取室外天气数据并更新屏幕显示
 * 说明：定时器回调函数，每1分钟执行一次
 * ============================================================================ */
static void outdoor_update(void)
{
    static weather_info_t last_weather = { 0 };  // 保存上次的天气信息，用于比较变化
    
    weather_info_t weather = { 0 };               // 当前天气信息
    
    // 心知天气API URL（包含API key和城市位置编码）
    // weather_url指针存储的是这个字符串在内存当中的首地址
    const char *weather_url = "https://api.seniverse.com/v3/weather/now.json?key=SfRic8Wmp-Qh3OeFk&location=WTEMH46Z5N09&language=en&unit=c";
    
    // 发送HTTP GET请求获取天气数据
    const char *weather_http_response = esp_at_http_get(weather_url);
    if (weather_http_response == NULL)
    {
        printf("[WEATHER] http error\n");
        return;
    }
    
    // 解析JSON格式的天气数据
    if (!parse_seniverse_response(weather_http_response, &weather))
    {
        printf("[WEATHER] parse failed\n");
        return;
    }
    
    // 如果天气信息没有变化，不更新显示
    if (memcmp(&last_weather, &weather, sizeof(weather_info_t)) == 0)
    {
        return;
    }
    
    // 保存当前天气信息
    memcpy(&last_weather, &weather, sizeof(weather_info_t));
    
    // 打印天气信息
    printf("[WEATHER] %s, %s, %.1f\n", weather.city, weather.weather, weather.temperature);
    
    // 更新屏幕显示的室外温度和天气图标
    main_page_redraw_outdoor_temperature(weather.temperature);
    main_page_redraw_outdoor_weather_icon(weather.weather_code);
}

/* ============================================================================
 * 任务函数类型定义
 * ============================================================================ */
typedef void (*app_job_t)(void);  // 应用任务函数指针类型

/* ============================================================================
 * 函数：app_work
 * 功能：工作队列的任务包装函数
 * 参数：param - 实际要执行的任务函数指针
 * 说明：将任务函数提交到工作队列中执行
 * ============================================================================ */
static void app_work(void *param)
{
    app_job_t job = (app_job_t)param;  // 将参数转换为任务函数指针
    job();                              // 执行任务
}

/* ============================================================================
 * 函数：work_timer_cb
 * 功能：定时器回调函数（工作队列模式）
 * 参数：timer - 定时器句柄
 * 说明：将任务提交到工作队列中异步执行（适用于耗时任务）
 * ============================================================================ */
static void work_timer_cb(TimerHandle_t timer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);  // 从定时器ID获取任务函数
    workqueue_run(app_work, job);                          // 提交到工作队列执行
}

/* ============================================================================
 * 函数：app_timer_cb
 * 功能：定时器回调函数（直接执行模式）
 * 参数：timer - 定时器句柄
 * 说明：在定时器中断上下文中直接执行任务（适用于快速任务）
 * ============================================================================ */
static void app_timer_cb(TimerHandle_t timer)
{
    app_job_t job = (app_job_t)pvTimerGetTimerID(timer);  // 从定时器ID获取任务函数
    job();                                                 // 直接执行任务
}

/* ============================================================================
 * 函数：app_init
 * 功能：应用初始化，创建并启动所有定时器和任务
 * 说明：由main.c在系统启动后调用
 * ============================================================================ */
void app_init(void)
{
    /* ------------------------------------------------------------------------
     * 创建5个定时器
     * 参数说明：
     *   1. 定时器名称（用于调试）
     *   2. 定时周期（ticks）
     *   3. 是否自动重载（pdTRUE=周期执行，pdFALSE=单次执行）
     *   4. 定时器ID（存储任务函数指针）
     *   5. 定时器回调函数
     * ------------------------------------------------------------------------ */
    
    // 时间显示更新定时器：1秒周期，直接在定时器回调中执行（快速任务）
    time_update_timer = xTimerCreate("time update", pdMS_TO_TICKS(TIME_UPDATE_INTERVAL), pdTRUE, time_update, app_timer_cb);
    
    // 时间同步定时器：初始200ms后执行第一次，之后根据成功/失败动态调整周期
    time_sync_timer = xTimerCreate("time sync", pdMS_TO_TICKS(200), pdFALSE, time_sync, work_timer_cb);
    
    // WiFi状态监控定时器：5秒周期，通过工作队列执行
    wifi_update_timer = xTimerCreate("wifi update", pdMS_TO_TICKS(WIFI_UPDATE_INTERVAL), pdTRUE, wifi_update, work_timer_cb);
    
    // 室内温湿度更新定时器：3秒周期，通过工作队列执行
    inner_update_timer = xTimerCreate("inner upadte", pdMS_TO_TICKS(INNER_UPDATE_INTERVAL), pdTRUE, inner_update, work_timer_cb);
    
    // 室外天气更新定时器：1分钟周期，通过工作队列执行（网络请求耗时）
    outdoor_update_timer = xTimerCreate("outdoor update", pdMS_TO_TICKS(OUTDOOR_UPDATE_INTERVAL), pdTRUE, outdoor_update, work_timer_cb);

    /* ------------------------------------------------------------------------
     * 立即执行一次所有任务（不等定时器触发）
     * 确保系统启动后立即显示数据，提升用户体验
     * ------------------------------------------------------------------------ */
    workqueue_run(app_work, time_sync);      // 立即同步网络时间
    workqueue_run(app_work, wifi_update);    // 立即检查WiFi状态
    workqueue_run(app_work, inner_update);   // 立即读取室内温湿度
    workqueue_run(app_work, outdoor_update); // 立即获取室外天气
    
    /* ------------------------------------------------------------------------
     * 启动所有定时器
     * 参数0表示立即启动，不等待
     * ------------------------------------------------------------------------ */
    xTimerStart(time_update_timer, 0);    // 启动时间显示更新定时器
    xTimerStart(time_sync_timer, 0);      // 启动时间同步定时器
    xTimerStart(wifi_update_timer, 0);    // 启动WiFi监控定时器
    xTimerStart(inner_update_timer, 0);   // 启动室内数据更新定时器
    xTimerStart(outdoor_update_timer, 0); // 启动室外天气更新定时器
}

