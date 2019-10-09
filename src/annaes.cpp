/*
 Name        : annaes.cpp
 Author      : Xiaomeng Lu
 Version     : 0.1
 Copyright   : SVOM@NAOC, CAS
 Description : 冷湖圆顶天窗自动控制服务
 */

#include <boost/make_shared.hpp>
#include "globaldef.h"
#include "daemon.h"
#include "GLog.h"
#include "parameter.h"
#include "GeneralControl.h"

GLog _gLog;
int main(int argc, char **argv) {
	if (argc >= 2) {// 处理命令行参数
		if (strcmp(argv[1], "-d") == 0) {
			Parameter param;
			param.InitFile("annaes.xml");
		}
		else _gLog.Write("Usage: annaes <-d>\n");
	}
	else {// 常规工作模式
		boost::asio::io_service ios;
		boost::asio::signal_set signals(ios, SIGINT, SIGTERM);  // interrupt signal
		signals.async_wait(boost::bind(&boost::asio::io_service::stop, &ios));

		if (!MakeItDaemon(ios)) return 1;
		if (!isProcSingleton(gPIDPath)) {
			_gLog.Write("%s is already running or failed to access PID file", DAEMON_NAME);
			return 2;
		}

		_gLog.Write("Try to launch %s %s %s as daemon", DAEMON_NAME, DAEMON_VERSION, DAEMON_AUTHORITY);
		// 主程序入口
		boost::shared_ptr<GeneralControl> gc = boost::make_shared<GeneralControl>();
		if (gc->StartService()) {
			_gLog.Write("Daemon goes running");
			ios.run();
			gc->StopService();
			_gLog.Write("Daemon stop running");
		}
		else {
			_gLog.Write(LOG_FAULT, NULL, "Fail to launch %s", DAEMON_NAME);
		}
	}

	return 0;
}
