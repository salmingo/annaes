/*!
 * @file GeneralControl.h annaes服务接口
 * @date 09 Oct, 2019
 * @version 0.1
 */

#ifndef GENERALCONTROL_H_
#define GENERALCONTROL_H_

#include <boost/container/stable_vector.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "MessageQueue.h"
#include "AsciiProtocol.h"
#include "tcpasio.h"
#include "NTPClient.h"
#include "parameter.h"
#include "ATimeSpace.h"

using namespace boost::posix_time;

class GeneralControl: public MessageQueue {
public:
	GeneralControl();
	virtual ~GeneralControl();

protected:
	/* 数据类型 */
	enum {// 总控服务消息
		MSG_RECEIVE_CLIENT = MSG_USER,	//< 收到客户端信息
		MSG_RECEIVE_DOME,	//< 收到天窗信息
		MSG_CLOSE_CLIENT,	//< 客户端断开网络连接
		MSG_CLOSE_DOME,		//< 天窗断开网络连接
		MSG_LAST	//< 占位, 不使用
	};

	enum {// 对应主机类型
		PEER_CLIENT,	//< 客户端
		PEER_DOME		//< 天窗
	};

	struct DomeNetwork {
		string gid;		//< 组标志
		TcpCPtr tcp;	//< TCP连接
		bool automode;	//< 天窗自动控制模式
		int state;		//< 状态
		int cntopen;	//< 计数: 打开
		int cntclose;	//< 计数: 关闭
		ptime tmlast;	//< 最后一次操作时间

	public:
		DomeNetwork() {
			automode  = true;	//< 初始化: 自动开关
			state     = -1;
			cntopen   = 0;
			cntclose  = 0;
		}

		/*!
		 * @brief 检查ID是否匹配
		 * @param id 组标志
		 * @return
		 * 1: 强匹配
		 * 2: 弱匹配
		 * 0: 不匹配
		 */
		int IsMatched(const string &id) {
			if (id == gid) return 1;
			if (id.empty()) return 2;
			return 0;
		}
	};
	typedef boost::container::stable_vector<DomeNetwork> DomeNetVec;
	typedef boost::container::stable_vector<TcpCPtr> TcpCVec;

protected:
	/*---------------- 成员变量 ----------------*/
//////////////////////////////////////////////////////////////////////////////
	/* 网络资源 */
	TcpSPtr tcps_client_;	//< TCP服务: 客户端
	TcpSPtr tcps_dome_;		//< TCP服务: 圆顶
	TcpCVec tcpc_client_;	//< TCP连接: 客户端
	DomeNetVec tcpc_dome_;	//< TCP连接: 圆顶
	boost::shared_array<char> bufrcv_;	//< 网络信息存储区: 消息队列中调用
	AscProtoPtr ascproto_;		//< 通用协议解析接口

//////////////////////////////////////////////////////////////////////////////
	/* 互斥锁 */
	boost::mutex mtx_tcpc_client_;	//< 互斥锁: 客户端
	boost::mutex mtx_tcpc_dome_;	//< 互斥锁: 圆顶

//////////////////////////////////////////////////////////////////////////////
	ParamPtr param_;	//< 配置参数
	NTPPtr ntp_;		//< NTP时钟同步接口
	AstroUtil::ATimeSpace ats_;	//< 天文时空变换接口

//////////////////////////////////////////////////////////////////////////////
	/* 多线程 */
	threadptr thrd_weather_;	//< 线程: 监测气象环境参数

public:
	/*!
	 * @brief 启动系统服务
	 */
	bool StartService();
	/*!
	 * @brief 停止系统服务
	 */
	void StopService();

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 消息响应函数 */
	void register_messages();
	void on_receive_client(const long param1, const long param2);
	void on_receive_dome  (const long param1, const long param2);
	void on_close_client  (const long param1, const long param2);
	void on_close_dome    (const long param1, const long param2);
	/*!
	 * @brief 解析与用户/数据库、转台、相机相关网络信息
	 * @param client 网络资源
	 * @param peer   远程主机类型
	 */
	void resolve_protocol_ascii(TCPClient* client, int peer);
	/*!
	 * @brief 处理来自用户/数据库的网络信息
	 * @param proto  信息主体
	 * @param peer   远程主机类型
	 * @param client 网络资源
	 */
	void process_protocol_client(apbase proto, TCPClient* client);
	/*!
	 * @brief 处理来自转台的网络信息
	 * @param proto  信息主体
	 * @param client 网络资源
	 */
	void process_protocol_dome(apbase proto, TCPClient* client);

protected:
//////////////////////////////////////////////////////////////////////////////
	/* 网络通信 */
	/*!
	 * @brief 创建网络服务
	 * @param server 网络服务
	 * @param port   监听端口
	 * @return
	 * 创建结果
	 * 0 -- 成功
	 * 其它 -- 错误代码
	 */
	int create_server(TcpSPtr *server, const uint16_t port);
	/*!
	 * @brief 依据配置文件, 创建所有网络服务
	 * @return
	 * 创建结果
	 */
	bool create_all_server();
	/*!
	 * @brief 处理网络连接请求
	 * @param client 为连接请求分配额网络资源
	 * @param server 服务器标识
	 */
	void network_accept(const TcpCPtr& client, const long server);
	/*!
	 * @brief 处理客户端信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_client(const long client, const long ec);
	/*!
	 * @brief 处理天窗状态信息
	 * @param client 网络资源
	 * @param ec     错误代码. 0: 正确
	 */
	void receive_dome(const long client, const long ec);

protected:
	/*!
	 * @brief 读取天窗开关判据(风速)
	 * @param tmloc     时标
	 * @param spdopen   用于判断是否打开天窗的风速
	 * @param spdclose  用于判断是否关闭天窗的风速
	 * @return
	 * 数据读取结果
	 */
	bool read_weather(string &tmloc, double &spdopen, double &spdclose);
	/*!
	 * @brief 计算太阳高度角
	 * @return
	 * 太阳高度角, 量纲: 角度
	 */
	double sun_altitude();
	/*!
	 * @brief 检查并改变天窗开关状态
	 * @param dome     圆顶资源访问地址
	 * @param odt      观测时段类型
	 * @param spdopen  用于判断是否可以打开天窗的风速判据
	 * @param spdclo   用于判断是否需要关闭天窗的风速判据
	 */
	void switch_slit(DomeNetwork &dome, int odt, double spdopen, double spdclo);

protected:
	/* 多线程 */
	/*!
	 * @brief 监测气象环境参数
	 */
	void thread_weather();
};

#endif /* GENERALCONTROL_H_ */
