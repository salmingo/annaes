/*!
 * @file AsciiProtocol.h 封装通信协议
 * @date 09 Oct, 2019
 * @version 0.1
 */

#ifndef ASCIIPROTOCOL_H_
#define ASCIIPROTOCOL_H_

#include <string>
#include <list>
#include <boost/smart_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/format.hpp>

using std::string;
using std::list;

//////////////////////////////////////////////////////////////////////////////
/*--------------- 声明通信协议 ---------------*/
#define APTYPE_START	"start"		//< 启用自动开关天窗
#define APTYPE_STOP		"stop"		//< 禁用自动开关天窗
#define APTYPE_RELOAD	"reload"	//< 重新加载配置参数
#define APTYPE_SLIT		"slit"		//< 天窗状态与指令

/*!
 * @struct ascii_proto_base 通信协议基类
 */
struct ascii_proto_base {
	string type;	//< 协议类型
	string gid;		//< 组编号

public:
	void set_id(const string& _1) {
		gid = _1;
	}
};
typedef boost::shared_ptr<ascii_proto_base> apbase;

/*!
 * @struct ascii_proto_start 启用自动开关天窗
 */
struct ascii_proto_start : public ascii_proto_base {
public:
	ascii_proto_start() {
		type = APTYPE_START;
	}
};
typedef boost::shared_ptr<ascii_proto_start> apstart;

/*!
 * @struct ascii_proto_stop 禁用自动开关天窗
 */
struct ascii_proto_stop : public ascii_proto_base {
public:
	ascii_proto_stop() {
		type = APTYPE_STOP;
	}
};
typedef boost::shared_ptr<ascii_proto_stop> apstop;

/*!
 * @struct ascii_proto_reload 重新加载配置文件参数
 */
struct ascii_proto_reload : public ascii_proto_base {
public:
	ascii_proto_reload() {
		type = APTYPE_RELOAD;
	}
};
typedef boost::shared_ptr<ascii_proto_reload> apreload;

/*!
 * @struct ascii_proto_slit 天窗控制指令和工作状态
 */
struct ascii_proto_slit : public ascii_proto_base {
	int command;	//< 控制指令
	int state;		//< 天窗状态

public:
	ascii_proto_slit() {
		type    = APTYPE_SLIT;
		command = -1;
		state   = -1;
	}
};
typedef boost::shared_ptr<ascii_proto_slit> apslit;

/*!
 * @brief 将ascii_proto_base继承类的boost::shared_ptr型指针转换为apbase类型
 * @param proto 协议指针
 * @return
 * apbase类型指针
 */
template <class T>
apbase to_apbase(T proto) {
	return boost::static_pointer_cast<ascii_proto_base>(proto);
}

/*!
 * @brief 将apbase类型指针转换为其继承类的boost::shared_ptr型指针
 * @param proto 协议指针
 * @return
 * apbase继承类指针
 */
template <class T>
boost::shared_ptr<T> from_apbase(apbase proto) {
	return boost::static_pointer_cast<T>(proto);
}

//////////////////////////////////////////////////////////////////////////////

class AsciiProtocol {
public:
	AsciiProtocol();
	virtual ~AsciiProtocol();

protected:
	/* 数据类型 */
	struct pair_key_val {// 关键字-键值对
		string keyword;
		string value;
	};
	typedef list<pair_key_val> likv;	//< pair_key_val列表

	typedef list<string> listring;	//< string列表
	typedef boost::unique_lock<boost::mutex> mutex_lock;	//< 互斥锁
	typedef boost::shared_array<char> charray;	//< 字符数组

protected:
	/* 成员变量 */
	boost::mutex mtx_;	//< 互斥锁
	int ibuf_;			//< 存储区索引
	charray buff_;		//< 存储区

protected:
	/*!
	 * @brief 输出编码后字符串
	 * @param compacted 已编码字符串
	 * @param n         输出字符串长度, 量纲: 字节
	 * @return
	 * 编码后字符串
	 */
	const char* output_compacted(string& output, int& n);
	const char* output_compacted(const char* s, int& n);
	/*!
	 * @brief 封装协议共性内容
	 * @param base    转换为基类的协议指针
	 * @param output  输出字符串
	 */
	void compact_base(apbase base, string &output);
	/*!
	 * @brief 连接关键字和对应数值, 并将键值对加入output末尾
	 * @param output   输出字符串
	 * @param keyword  关键字
	 * @param value    非字符串型数值
	 */
	template <class T1, class T2>
	void join_kv(string& output, T1& keyword, T2& value) {
		boost::format fmt("%1%=%2%,");
		fmt % keyword % value;
		output += fmt.str();
	}

	/*!
	 * @brief 解析单个关键字和对应数值
	 * @param kv       keyword=value对
	 * @param keyword  关键字
	 * @param value    对应数值
	 * @return
	 * 关键字和数值非空
	 */
	bool resolve_kv(string& kv, string& keyword, string& value);
	/**
	 * @brief 解析键值对集合并提取通用项
	 */
	void resolve_kv_array(listring &tokens, likv &kvs, ascii_proto_base &basis);

protected:
	/**
	 * @brief 启用自动开关天窗
	 * */
	apbase resolve_start();
	/**
	 * @brief 禁用自动开关天窗
	 * */
	apbase resolve_stop();
	/**
	 * @brief 重新加载配置参数
	 * */
	apbase resolve_reload();
	/**
	 * @brief 解析天窗状态/指令
	 * */
	apbase resolve_slit(likv &kvs);

public:
	/*---------------- 解析通信协议 ----------------*/
	/*!
	 * @brief 解析字符串生成结构化通信协议
	 * @param rcvd 待解析字符串
	 * @return
	 * 统一转换为apbase类型
	 */
	apbase Resolve(const char *rcvd);
	/*!
	 * @brief 封装: 重新加载配置参数
	 */
	const char *CompactStart(const string &gid, int &n);
	/*!
	 * @brief 封装: 重新加载配置参数
	 */
	const char *CompactStop(const string &gid, int &n);
	/*!
	 * @brief 封装: 重新加载配置参数
	 */
	const char *CompactReload(int &n);
	/*!
	 * @brief 封装: 天窗状态/指令
	 */
	const char *CompactSlit(apslit proto, int &n);
};
typedef boost::shared_ptr<AsciiProtocol> AscProtoPtr;

#endif /* ASCIIPROTOCOL_H_ */
