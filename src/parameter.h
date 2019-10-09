/*!
 * @file parameter.h 使用XML文件格式管理配置参数
 */

#ifndef PARAMETER_H_
#define PARAMETER_H_

#include <string>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/foreach.hpp>
#include <boost/smart_ptr.hpp>
#include "AstroDeviceDef.h"

using std::string;

struct Parameter {// 软件配置参数
	int	portClient;		//< 客户端网络服务端口
	int portDome;		//< 圆顶网络服务端口

	bool ntpEnable;		//< NTP启用标志
	string ntpHost;		//< NTP服务器IP地址
	int ntpMaxDiff;		//< 采用自动校正时钟策略时, 本机时钟与NTP时钟所允许的最大偏差, 量纲: 毫秒

	string sitename;	//< 测站名称
	double siteLon;		//< 测站地理经度, 量纲: 角度. 东经为正
	double siteLat;		//< 测站地理纬度, 量纲: 角度. 北纬为正
	double siteAlt;		//< 海拔高度, 量纲: 米
	int timezone;		//< 本地时时区, 量纲: 小时

	string pathWeather;	//< 气象环境参数文件路径

	/*
	 * open_, 打开天窗的控制参数
	 */
	double openSunAlt;	//< 最大太阳高度角, 量纲: 角度
	int openWindOpt;	//< 采用风速的选项索引. 1: 瞬时; 2: 2min平均; 3: 10min平均
	double openWindSpd;	//< 最大安全风速, 量纲: 米/秒
	int openContNum;	//< 风速低于阈值的连续次数

	/*
	 * clo_, 关闭天窗的控制参数
	 */
	double cloSunAlt;			//< 最低太阳高度角, 量纲: 角度
	int cloWindOpt;				//< 采用风速的选项索引. 1: 瞬时; 2: 2min平均; 3: 10min平均
	double cloWindSpdEmergency;	//< 危险风速, 量纲: 米/秒. 风速大于该值时, 立即关闭天窗
	double cloWindSpd;			//< 最大安全风速, 量纲: 米/秒
	int cloContNum;				//< 风速大于阈值的连续次数

public:
	/*!
	 * @brief 初始化文件filepath, 存储缺省配置参数
	 * @param filepath 文件路径
	 */
	void InitFile(const std::string& filepath) {
		using namespace boost::posix_time;
		using boost::property_tree::ptree;

		ptree pt;

		pt.add("version", "0.1");
		pt.add("date", to_iso_string(second_clock::universal_time()));

		ptree& node1 = pt.add("NetworkServer", "");
		node1.add("Client.<xmlattr>.Port", 4020);
		node1.add("Dome.<xmlattr>.Port",   4021);

		ptree& node2 = pt.add("NTP", "");
		node2.add("<xmlattr>.Enable",        false);
		node2.add("<xmlattr>.IPv4",          "172.28.1.3");
		node2.add("MaxDiff.<xmlattr>.Value", 100);
		node2.add("<xmlcomment>", "Difference is in millisec");

		ptree& node3 = pt.add("ObservationSite", "");
		node3.add("<xmlattr>.Name", "Lenghu");
		node3.add("Longitude.<xmlattr>.Value",  100.0);
		node3.add("Latitude.<xmlattr>.Value",    30.0);
		node3.add("Altitude.<xmlattr>.Value",  4500.0);
		node3.add("Timezone.<xmlattr>.Value",       8);

		pt.add("Weather.<xmlattr>.Path", "/Volumes/Fast_SSD/data/weather/realtime_weather.txt");

		ptree& node4 = pt.add("SlitOpen", "");
		node4.add("SunCenter.<xmlattr>.Altitude",       -5.0);
		node4.add("UseWindSpeed.<xmlattr>.Option",         1);
		node4.add("<xmlcomment>", "WindSpeed Option #1: instant");
		node4.add("<xmlcomment>", "WindSpeed Option #2: 2min average");
		node4.add("<xmlcomment>", "WindSpeed Option #3: 10min average");
		node4.add("WindSpeed.<xmlattr>.Threshold",      10.0);
		node4.add("WindSpeed.<xmlattr>.ContinualNumber",   3);

		ptree& node5 = pt.add("SlitClose", "");
		node5.add("SunCenter.<xmlattr>.Altitude",       -5.0);
		node5.add("UseWindSpeed.<xmlattr>.Option",         1);
		node5.add("<xmlcomment>", "WindSpeed Option #1: instant");
		node5.add("<xmlcomment>", "WindSpeed Option #2: 2min average");
		node5.add("<xmlcomment>", "WindSpeed Option #3: 10min average");
		node5.add("Emergency.<xmlattr>.Threshold",      20.0);
		node5.add("WindSpeed.<xmlattr>.Threshold",      15.0);
		node5.add("WindSpeed.<xmlattr>.ContinualNumber",   3);

		boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
		write_xml(filepath, pt, std::locale(), settings);
	}

	/*!
	 * @brief 从文件filepath加载配置参数
	 * @param filepath 文件路径
	 */
	bool LoadFile(const std::string& filepath) {
		try {
			using boost::property_tree::ptree;

			ptree pt;
			read_xml(filepath, pt, boost::property_tree::xml_parser::trim_whitespace);
			BOOST_FOREACH(ptree::value_type const &child, pt.get_child("")) {
				if (boost::iequals(child.first, "NetworkServer")) {
					portClient     = child.second.get("Client.<xmlattr>.Port",     4020);
					portDome       = child.second.get("Dome.<xmlattr>.Port",       4021);
				}
				else if (boost::iequals(child.first, "NTP")) {
					ntpEnable  = child.second.get("<xmlattr>.Enable",        true);
					ntpHost    = child.second.get("<xmlattr>.IPv4",          "172.28.1.3");
					ntpMaxDiff = child.second.get("MaxDiff.<xmlattr>.Value", 100);
				}
				else if (boost::iequals(child.first, "Weather")) {
					pathWeather = child.second.get("<xmlattr>.Path", "");
				}
				else if (boost::iequals(child.first, "ObservationSite")) {
					sitename   = child.second.get("<xmlattr>.Name",             "");
					siteLon    = child.second.get("Longitude.<xmlattr>.Value", 0.0);
					siteLat    = child.second.get("Latitude.<xmlattr>.Value",  0.0);
					siteAlt    = child.second.get("Altitude.<xmlattr>.Value",  0.0);
					timezone   = child.second.get("Timezone.<xmlattr>.Value",    8);
				}
				else if (boost::iequals(child.first, "SlitOpen")) {
					openSunAlt   = child.second.get("SunCenter.<xmlattr>.Altitude",       -5.0);
					openWindOpt  = child.second.get("UseWindSpeed.<xmlattr>.Option",         1);
					openWindSpd  = child.second.get("WindSpeed.<xmlattr>.Threshold",      10.0);
					openContNum  = child.second.get("WindSpeed.<xmlattr>.ContinualNumber",   3);
				}
				else if (boost::iequals(child.first, "SlitClose")) {
					cloSunAlt   = child.second.get("SunCenter.<xmlattr>.Altitude",       -5.0);
					cloWindOpt  = child.second.get("UseWindSpeed.<xmlattr>.Option",         1);
					cloWindSpdEmergency = child.second.get("Emergency.<xmlattr>.Threshold", 20.0);
					cloWindSpd  = child.second.get("WindSpeed.<xmlattr>.Threshold",      15.0);
					cloContNum  = child.second.get("WindSpeed.<xmlattr>.ContinualNumber",   3);
				}
			}
			return true;
		}
		catch(boost::property_tree::xml_parser_error &ex) {
			InitFile(filepath);
			return false;
		}
	}
};
typedef boost::shared_ptr<Parameter> ParamPtr;

#endif // PARAMETER_H_
