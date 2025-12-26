/**
 * @file main_page.c
 * @brief 主页面显示模块 - 负责显示温湿度、时间、日期、WiFi状态和天气信息
 * @details 本文件实现了主页面的UI布局和各个信息区域的更新功能
 */

#include <stdint.h>
#include <stdio.h>
#include "ui.h"
#include "rtc.h"
#include "page.h"

// ======================== 颜色定义 ========================
// RGB565格式颜色值定义
#define COLOR_BACKGROUND    mkcolor(0, 0, 0)        // 背景色：黑色
#define COLOR_TEXT_WHITE    mkcolor(255, 255, 255)  // 文字色：白色
#define COLOR_TEXT_YELLOW   mkcolor(255, 255, 0)    // 强调色：黄色
#define COLOR_TEXT_CYAN     mkcolor(0, 255, 255)    // 信息色：青色

// ======================== 布局定义 ========================
// 定义屏幕上各个信息区域的位置坐标

// WiFi SSID 显示区域
#define WIFI_SSID_X         10      // WiFi名称显示的X坐标
#define WIFI_SSID_Y         10      // WiFi名称显示的Y坐标

// 时间显示区域
#define TIME_X              20      // 时间显示的X坐标
#define TIME_Y              50      // 时间显示的Y坐标

// 日期显示区域
#define DATE_X              20      // 日期显示的X坐标
#define DATE_Y              90      // 日期显示的Y坐标

// 室内温湿度显示区域
#define INNER_TEMP_X        20      // 室内温度显示的X坐标
#define INNER_TEMP_Y        130     // 室内温度显示的Y坐标
#define INNER_HUMI_X        20      // 室内湿度显示的X坐标
#define INNER_HUMI_Y        170     // 室内湿度显示的Y坐标

// 室外温度和天气图标显示区域
#define OUTDOOR_TEMP_X      20      // 室外温度显示的X坐标
#define OUTDOOR_TEMP_Y      210     // 室外温度显示的Y坐标
#define WEATHER_ICON_X      160     // 天气图标显示的X坐标
#define WEATHER_ICON_Y      200     // 天气图标显示的Y坐标

/**
 * @brief 显示主页面初始布局
 * @details 清空屏幕并绘制主页面的静态标签和初始内容
 *          包括: WiFi、时间、日期、室内外温湿度等信息的标签
 */
void main_page_display(void)
{
    // 清空整个屏幕为黑色背景
    ui_fill_color(0, 0, UI_WIDTH - 1, UI_HEIGHT - 1, COLOR_BACKGROUND);
    
    // 显示静态标签文字
    ui_write_string(WIFI_SSID_X, WIFI_SSID_Y, "WiFi: ", 
                    COLOR_TEXT_YELLOW, COLOR_BACKGROUND, &font16_maple_bold);
    
    ui_write_string(10, TIME_Y - 20, "Time:", 
                    COLOR_TEXT_CYAN, COLOR_BACKGROUND, &font16_maple_bold);
    
    ui_write_string(10, DATE_Y - 20, "Date:", 
                    COLOR_TEXT_CYAN, COLOR_BACKGROUND, &font16_maple_bold);
    
    ui_write_string(10, INNER_TEMP_Y - 20, "Indoor:", 
                    COLOR_TEXT_CYAN, COLOR_BACKGROUND, &font16_maple_bold);
    
    ui_write_string(10, OUTDOOR_TEMP_Y - 20, "Outdoor:", 
                    COLOR_TEXT_CYAN, COLOR_BACKGROUND, &font16_maple_bold);
}
 
/**
 * @brief 刷新WiFi SSID显示
 * @param ssid WiFi网络名称字符串指针
 * @details 在屏幕上更新显示当前连接的WiFi名称
 *          先用背景色清除原来的内容，再显示新的SSID
 */
void main_page_redraw_wifi_ssid(const char *ssid)
{
    // 清除旧的SSID显示区域（用背景色覆盖）
    ui_fill_color(WIFI_SSID_X + 60, WIFI_SSID_Y, 
                  UI_WIDTH - 1, WIFI_SSID_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的WiFi名称
    ui_write_string(WIFI_SSID_X + 60, WIFI_SSID_Y, ssid, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font16_maple_bold);
}

/**
 * @brief 刷新时间显示
 * @param time RTC时间结构体指针，包含时、分、秒信息
 * @details 以 HH:MM:SS 格式显示当前时间
 *          使用大号字体以便清晰显示
 */
void main_page_redraw_time(rtc_date_time_t *time)
{
    char time_str[16];  // 时间字符串缓冲区
    
    // 格式化时间字符串为 "HH:MM:SS" 格式
    // %02u 表示至少2位数字，不足时前面补0
    sprintf(time_str, "%02u:%02u:%02u", 
            time->hour, time->minute, time->second);
    
    // 清除旧的时间显示区域
    ui_fill_color(TIME_X, TIME_Y, 
                  UI_WIDTH - 1, TIME_Y + 30, 
                  COLOR_BACKGROUND);
    
    // 显示新的时间（使用大号字体）
    ui_write_string(TIME_X, TIME_Y, time_str, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font24_maple_bold);
}

/**
 * @brief 刷新日期显示
 * @param date RTC日期结构体指针，包含年、月、日和星期信息
 * @details 以 YYYY-MM-DD Week:N 格式显示当前日期
 *          星期用数字1-7表示（1=周一，7=周日）
 */
void main_page_redraw_date(rtc_date_time_t *date)
{
    char date_str[32];  // 日期字符串缓冲区
    
    // 格式化日期字符串为 "YYYY-MM-DD Week:N" 格式
    sprintf(date_str, "%04u-%02u-%02u Week:%u", 
            date->year, date->month, date->day, date->weekday);
    
    // 清除旧的日期显示区域
    ui_fill_color(DATE_X, DATE_Y, 
                  UI_WIDTH - 1, DATE_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的日期
    ui_write_string(DATE_X, DATE_Y, date_str, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font16_maple_bold);
}

/**
 * @brief 刷新室内温度显示
 * @param temperature 温度值（摄氏度），浮点数
 * @details 显示格式为 "Temp: XX.X°C"
 *          保留一位小数以提供足够的精度
 */
void main_page_redraw_inner_temperature(float temperature)
{
    char temp_str[32];  // 温度字符串缓冲区
    
    // 格式化温度字符串
    // %.1f 表示保留1位小数
    sprintf(temp_str, "Temp: %.1f C", temperature);
    
    // 清除旧的温度显示区域
    ui_fill_color(INNER_TEMP_X, INNER_TEMP_Y, 
                  UI_WIDTH - 1, INNER_TEMP_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的温度值
    ui_write_string(INNER_TEMP_X, INNER_TEMP_Y, temp_str, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font16_maple_bold);
}

/**
 * @brief 刷新室内湿度显示
 * @param humidity 相对湿度值（百分比），浮点数
 * @details 显示格式为 "Humi: XX.X%"
 *          湿度范围通常为 0-100%
 */
void main_page_redraw_inner_humidity(float humidity)
{
    char humi_str[32];  // 湿度字符串缓冲区
    
    // 格式化湿度字符串
    sprintf(humi_str, "Humi: %.1f%%", humidity);
    
    // 清除旧的湿度显示区域
    ui_fill_color(INNER_HUMI_X, INNER_HUMI_Y, 
                  UI_WIDTH - 1, INNER_HUMI_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的湿度值
    ui_write_string(INNER_HUMI_X, INNER_HUMI_Y, humi_str, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font16_maple_bold);
}


/**
 * @brief 刷新室外温度显示
 * @param temperature 室外温度值（摄氏度），浮点数
 * @details 显示从天气API获取的室外温度
 *          格式为 "Temp: XX.X°C"
 */
void main_page_redraw_outdoor_temperature(float temperature)
{
    char temp_str[32];  // 温度字符串缓冲区
    
    // 格式化室外温度字符串
    sprintf(temp_str, "Temp: %.1f C", temperature);
    
    // 清除旧的室外温度显示区域
    ui_fill_color(OUTDOOR_TEMP_X, OUTDOOR_TEMP_Y, 
                  UI_WIDTH - 1, OUTDOOR_TEMP_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的室外温度
    ui_write_string(OUTDOOR_TEMP_X, OUTDOOR_TEMP_Y, temp_str, 
                    COLOR_TEXT_WHITE, COLOR_BACKGROUND, &font16_maple_bold);
}

/**
 * @brief 刷新天气图标显示
 * @param code 天气代码（来自天气API）
 * @details 根据天气代码选择对应的图标进行显示
 *          支持的天气类型：晴天、多云、雨天、雪天等
 * 
 * 常见天气代码映射：
 * - 0-3:   晴天系列
 * - 4-8:   多云系列
 * - 9-13:  雨天系列
 * - 14-17: 雪天系列
 * 
 * @note 需要预先在图像资源中定义各种天气图标
 */
void main_page_redraw_outdoor_weather_icon(const int code)
{
    const image_t *weather_icon = NULL;  // 天气图标指针
    
    // 根据天气代码选择对应的图标
    // 这里需要根据实际的天气API代码定义进行映射
    switch (code)
    {
        case 0:  // 晴天
        case 1:
            weather_icon = &img_weather_sunny;
            break;
            
        case 4:  // 多云
        case 5:
        case 6:
        case 7:
        case 8:
            weather_icon = &img_weather_cloudy;
            break;
            
        case 9:   // 雨天
        case 10:
        case 11:
        case 12:
        case 13:
            weather_icon = &img_weather_rainy;
            break;
            
        case 14:  // 雪天
        case 15:
        case 16:
        case 17:
            weather_icon = &img_weather_snowy;
            break;
            
        default:  // 未知天气，显示默认图标
            weather_icon = &img_weather_unknown;
            break;
    }
    
    // 如果找到了对应的图标，则显示
    if (weather_icon != NULL)
    {
        // 先清除旧的图标区域
        ui_fill_color(WEATHER_ICON_X, WEATHER_ICON_Y, 
                      WEATHER_ICON_X + 64, WEATHER_ICON_Y + 64, 
                      COLOR_BACKGROUND);
        
        // 绘制新的天气图标
        ui_draw_image(WEATHER_ICON_X, WEATHER_ICON_Y, weather_icon);
    }
}

/**
 * @brief 刷新城市名称显示（可选功能）
 * @param city 城市名称字符串指针
 * @details 显示从天气API获取的城市信息
 *          可用于确认天气数据来源
 */
void main_page_redraw_outdoor_city(const char *city)
{
    // 定义城市显示位置
    const uint16_t CITY_X = 20;
    const uint16_t CITY_Y = 250;
    
    // 清除旧的城市名称
    ui_fill_color(CITY_X, CITY_Y, 
                  UI_WIDTH - 1, CITY_Y + 20, 
                  COLOR_BACKGROUND);
    
    // 显示新的城市名称
    ui_write_string(CITY_X, CITY_Y, city, 
                    COLOR_TEXT_CYAN, COLOR_BACKGROUND, &font16_maple_bold);
}

