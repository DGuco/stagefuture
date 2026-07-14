/*****************************************************************
* FileName:log.h
* Summary :
* Date	  :2024-2-27
* Author  :DGuco(1139140929@qq.com)
******************************************************************/
#ifndef __LOG_H__
#define __LOG_H__

#include "singleton.h"
#define  CONSOLE_LOG_NAME "console"
#define  MAX_SPDLOG_QUEUE_SIZE (102400)
#define  MAX_SPDLOG_THREAD_POOL (4)

enum enDiskLog
{
	ASSERT_DISK = 0,
	DEBUG_DISK = 1,
	ERROR_DISK = 2,
	DIS_LOG_MAX,
};

enum enCacheLog
{
	DEBUG_CACHE = 0,
	ERROR_CACHE = 1,
	THREAD_ERROR = 2,
	CACHE_LOG_MAX,
};

static std::pair<int, std::string> g_DisLogFile[] =
{
	{enDiskLog::ASSERT_DISK,"assert"},
	{enDiskLog::DEBUG_DISK,"debug_disk"},
	{enDiskLog::ERROR_DISK,"error_disk"},
};

static std::pair<int, std::string> g_CacheLogFile[] =
{
	{enCacheLog::DEBUG_CACHE,"debug"},
	{enCacheLog::ERROR_CACHE,"error"},
	{enCacheLog::THREAD_ERROR,"thread_error"},
};

namespace detail_log {

	// 判断是否为字符串类型（用于分派格式符与参数转换）
	template<typename T> struct is_string_type : std::false_type {};
	template<> struct is_string_type<std::string> : std::true_type {};
	template<> struct is_string_type<const std::string&> : std::true_type {};
	template<> struct is_string_type<const char*> : std::true_type {};
	template<> struct is_string_type<char*> : std::true_type {};
	template<size_t N> struct is_string_type<char[N]> : std::true_type {};
	template<size_t N> struct is_string_type<const char[N]> : std::true_type {};

	// 参数转换：字符串 -> const char*；其他数字 -> long（与 %s / %ld 对应）
	inline const char* convert_arg(const std::string& val) { return val.c_str(); }
	inline const char* convert_arg(const char* val)        { return val; }
	inline const char* convert_arg(char* val)              { return val; }
	template<size_t N>
	inline const char* convert_arg(const char(&val)[N])     { return val; }
	template<size_t N>
	inline const char* convert_arg(char(&val)[N])           { return val; }

	template<typename T>
	typename std::enable_if<
		!is_string_type<typename std::decay<T>::type>::value &&
		!std::is_same<typename std::decay<T>::type, const char*>::value &&
		!std::is_same<typename std::decay<T>::type, char*>::value &&
		!std::is_pointer<typename std::decay<T>::type>::value,
		long>::type
	convert_arg(const T& val) { return static_cast<long>(val); }

	// 针对指针（非字符指针）的兜底：转为 long（避免指针被当作字符串处理）
	template<typename T>
	typename std::enable_if<
		std::is_pointer<typename std::decay<T>::type>::value &&
		!std::is_same<typename std::decay<T>::type, const char*>::value &&
		!std::is_same<typename std::decay<T>::type, char*>::value,
		long>::type
	convert_arg(const T& val) { return reinterpret_cast<long>(val); }

	// 格式符：字符串 -> %s；其他 -> %ld
	inline const char* fmt_spec(const std::string&) { return "%s"; }
	inline const char* fmt_spec(const char*)        { return "%s"; }
	inline const char* fmt_spec(char*)              { return "%s"; }
	template<size_t N>
	inline const char* fmt_spec(const char(&)[N])    { return "%s"; }
	template<size_t N>
	inline const char* fmt_spec(char(&)[N])          { return "%s"; }
	template<typename T>
	typename std::enable_if<
		!is_string_type<typename std::decay<T>::type>::value &&
		!std::is_same<typename std::decay<T>::type, const char*>::value &&
		!std::is_same<typename std::decay<T>::type, char*>::value,
		const char*>::type
	fmt_spec(const T&) { return "%ld"; }

	// 修改后（C++11 递归）
	template<typename T>
	void collect_specs(std::string& out, const T& val) {
		out += fmt_spec(val);
	}

	template<typename T, typename... Rest>
	void collect_specs(std::string& out, const T& val, const Rest&... rest) {
		out += fmt_spec(val);
		collect_specs(out, rest...);
	}

	// 空参数包的终止函数
	inline void collect_specs(std::string&) {}
}
	
// 将 vFmt 中每个 "{}" 按顺序替换为 args 对应类型的 %ld 或 %s
template<typename... Args>
static std::string build_fmt_string(const char* vFmt, const Args&... args)
{
	const size_t n = sizeof...(Args);
	std::string specs;
	specs.reserve(n * 3);
	detail_log::collect_specs(specs, args...);

	std::string result;
	result.reserve(strlen(vFmt) + specs.size());
	size_t specIdx = 0;
	for (size_t i = 0; vFmt && vFmt[i] != '\0'; ++i)
	{
		if (vFmt[i] == '{' && vFmt[i + 1] == '}')
		{
			if (specIdx < specs.size())
			{
				if (specs[specIdx] == '%' && specs[specIdx + 1] == 'l' && specs[specIdx + 2] == 'd')
				{
					result += "%ld";
					specIdx += 3;
				}
				else if (specs[specIdx] == '%' && specs[specIdx + 1] == 's')
				{
					result += "%s";
					specIdx += 2;
				}
				else
				{
					result += specs[specIdx++];
				}
			}
			else
			{
				result += "{}";
			}
			++i;
		}
		else
		{
			result += vFmt[i];
		}
	}
	return result;
}

class CLog : public CSingleton<CLog>
{
public:
	CLog() {};
	~CLog() {};
	template<typename... Args>
	int DiskLog(int log_type, const char* vFmt, const Args &... args);
	template<typename... Args>
	int CacheLog(int log_type, const char* vFmt, const Args &... args);
};

template<typename... Args>
int CLog::DiskLog(int log_type, const char* vFmt, const Args &... args)
{
	std::string fmt = ::build_fmt_string(vFmt, args...);
	printf("[%s] ", g_DisLogFile[log_type].second.c_str());
	printf(fmt.c_str(), detail_log::convert_arg(args)...);
	printf(" \n");
	return 0;
}

template<typename... Args>
int CLog::CacheLog(int log_type, const char* vFmt, const Args &... args)
{
	std::string fmt = ::build_fmt_string(vFmt, args...);
	printf("[%s] ", g_CacheLogFile[log_type].second.c_str());
	printf(fmt.c_str(), detail_log::convert_arg(args)...);
	printf(" \n");
	return 0;
}

#define DISK_LOG  CLog::GetSingletonPtr()->DiskLog
#define CACHE_LOG  CLog::GetSingletonPtr()->CacheLog

#endif //__LOG_H__

