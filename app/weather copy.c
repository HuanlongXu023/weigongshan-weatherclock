#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "weather.h"

/* ============================================================================
 * 函数：parse_seniverse_response
 * 功能：解析心知天气API返回的JSON格式天气数据
 * 参数：
 *   response - HTTP响应字符串（JSON格式）
 *   info     - 输出参数，存储解析后的天气信息
 * 返回值：
 *   true  - 解析成功
 *   false - 解析失败
 * 
 * JSON数据格式示例：
 * {
 *   "results": [{
 *     "location": {
 *       "name": "Singapore",
 *       "path": "Singapore,Singapore,Singapore"
 *     },
 *     "now": {
 *       "text": "Sunny",
 *       "code": "0",
 *       "temperature": "28"
 *     }
 *   }]
 * }
 * ============================================================================ */
bool parse_seniverse_response(const char *response, weather_info_t *info)
{
	/* ------------------------------------------------------------------------
	 * 第一步：定位到JSON的"results"字段
	 * strstr查找子字符串，如果找不到返回NULL
	 * ------------------------------------------------------------------------ */
	response = strstr(response, "\"results\":");
	if (response == NULL)
		return false;  // 没有找到results字段，JSON格式错误
	
	/* ------------------------------------------------------------------------
	 * 第二步：解析location字段（地理位置信息）
	 * ------------------------------------------------------------------------ */
	const char *location_response = strstr(response, "\"location\":");
	if (location_response == NULL)
		return false;  // 没有找到location字段，JSON格式错误
	
	/* ------------------------------------------------------------------------
	 * 2.1 解析城市名称（name字段）
	 * sscanf格式：读取引号内最多31个非引号字符到info->city
	 * 示例："name": "Singapore" -> info->city = "Singapore"
	 * ------------------------------------------------------------------------ */
	const char *loaction_name_response = strstr(location_response, "\"name\":");
	if (loaction_name_response)
	{
		sscanf(loaction_name_response, "\"name\": \"%31[^\"]\"", info->city);
	}
	
	/* ------------------------------------------------------------------------
	 * 2.2 解析完整路径（path字段）
	 * sscanf格式：读取引号内最多128个非引号字符到info->loaction
	 * 示例："path": "Singapore,Singapore,Singapore" 
	 *       -> info->loaction = "Singapore,Singapore,Singapore"
	 * ------------------------------------------------------------------------ */
	const char *loaction_path_response = strstr(location_response, "\"path\":");
	if (loaction_path_response)
	{
		sscanf(loaction_path_response, "\"path\": \"%128[^\"]\"", info->loaction);
	}
	
	/* ------------------------------------------------------------------------
	 * 第三步：解析now字段（当前天气信息）
	 * ------------------------------------------------------------------------ */
	const char *now_response = strstr(response, "\"now\":");
	if (now_response == NULL)
		return false;  // 没有找到now字段，JSON格式错误
	
	/* ------------------------------------------------------------------------
	 * 3.1 解析天气描述（text字段）
	 * sscanf格式：读取引号内最多15个非引号字符到info->weather
	 * 示例："text": "Sunny" -> info->weather = "Sunny"
	 * 可能的值：Sunny(晴), Cloudy(多云), Rainy(雨), etc.
	 * ------------------------------------------------------------------------ */
	const char *now_text_response = strstr(now_response, "\"text\":");
	if (now_text_response)
	{
		sscanf(now_text_response, "\"text\": \"%15[^\"]\"", info->weather);
	}
	
	/* ------------------------------------------------------------------------
	 * 3.2 解析天气代码（code字段）
	 * sscanf格式：读取整数到info->weather_code
	 * 示例："code": "0" -> info->weather_code = 0
	 * 天气代码用于显示对应的天气图标
	 * 常见代码：0=晴天, 4=多云, 9=阴天, 10=阵雨, 13=小雨, etc.
	 * ------------------------------------------------------------------------ */
	const char *now_code_response = strstr(now_response, "\"code\":");
	if (now_code_response)
	{
		sscanf(now_code_response, "\"code\": \"%d\"", &info->weather_code);
	}
	
	/* ------------------------------------------------------------------------
	 * 3.3 解析温度（temperature字段）
	 * 注意：温度在JSON中是字符串格式，需要先读取到字符串再转换为浮点数
	 * 示例："temperature": "28" -> temperature_str = "28" -> info->temperature = 28.0
	 * ------------------------------------------------------------------------ */
	char temperature_str[16] = { 0 };  // 临时存储温度字符串（最多15个字符+结束符）
	const char *now_temperature_response = strstr(now_response, "\"temperature\":");
	if (now_temperature_response)
	{
		// 先用sscanf读取字符串格式的温度值
		if (sscanf(now_temperature_response, "\"temperature\": \"%15[^\"]\"", temperature_str) == 1)
			// 使用atof（ASCII to Float）将字符串转换为浮点数
			info->temperature = atof(temperature_str);
	}
	
	/* ------------------------------------------------------------------------
	 * 解析成功，返回true
	 * 此时info结构体中已包含：
	 *   - city: 城市名称
	 *   - loaction: 完整地理路径
	 *   - weather: 天气描述文字
	 *   - weather_code: 天气代码（用于图标显示）
	 *   - temperature: 温度值（摄氏度）
	 * ------------------------------------------------------------------------ */
	return true;
}

