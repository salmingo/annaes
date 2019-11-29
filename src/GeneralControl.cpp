/*!
 * @file GeneralControl.cpp annaes服务接口
 * @date 09 Oct, 2019
 * @version 0.1
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include "GeneralControl.h"
#include "GLog.h"
#include "globaldef.h"
#include "ADefine.h"

using namespace boost;
using namespace AstroUtil;

GeneralControl::GeneralControl() {
	bufrcv_.reset(new char[TCP_PACK_SIZE]);
	ascproto_ = boost::make_shared<AsciiProtocol>();
	param_    = boost::make_shared<Parameter>();
}

GeneralControl::~GeneralControl() {

}

//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::StartService() {
	if (!param_->LoadFile(gConfigPath)) {
		_gLog.Write(LOG_FAULT, NULL, "failed to access configuration file[%s]", gConfigPath);
		return false;
	}

	ats_.SetSite(param_->siteLon, param_->siteLat, param_->siteAlt, param_->timezone);
	if (1 <= param_->openWindOpt && param_->openWindOpt <= 3 && 1 <= param_->cloWindOpt && param_->cloWindOpt <= 3)
		thrd_weather_.reset(new boost::thread(boost::bind(&GeneralControl::thread_weather, this)));
	else {
		_gLog.Write(LOG_FAULT, NULL, "UseWindSpd option of SlitOpen or SlitClose was wrong");
		return false;
	}

	register_messages();
	std::string name = "msgque_";
	name += DAEMON_NAME;
	if (!Start(name.c_str())) return false;
	if (!create_all_server()) return false;
	if (param_->ntpEnable) {
		ntp_ = make_ntp(param_->ntpHost.c_str(), 123, param_->ntpMaxDiff);
		ntp_->EnableAutoSynch(true);
	}

	return true;
}

void GeneralControl::StopService() {
	Stop();
	interrupt_thread(thrd_weather_);
}

//////////////////////////////////////////////////////////////////////////////
void GeneralControl::register_messages() {
	const CBSlot& slot11 = boost::bind(&GeneralControl::on_receive_client,  this, _1, _2);
	const CBSlot& slot12 = boost::bind(&GeneralControl::on_receive_dome,    this, _1, _2);
	const CBSlot& slot21 = boost::bind(&GeneralControl::on_close_client,    this, _1, _2);
	const CBSlot& slot22 = boost::bind(&GeneralControl::on_close_dome,      this, _1, _2);

	RegisterMessage(MSG_RECEIVE_CLIENT,  slot11);
	RegisterMessage(MSG_RECEIVE_DOME,    slot12);
	RegisterMessage(MSG_CLOSE_CLIENT,    slot21);
	RegisterMessage(MSG_CLOSE_DOME,      slot22);
}

void GeneralControl::on_receive_client(const long param1, const long param2) {
	resolve_protocol_ascii((TCPClient*) param1, PEER_CLIENT);
}

void GeneralControl::on_receive_dome(const long param1, const long param2) {
	resolve_protocol_ascii((TCPClient*) param1, PEER_DOME);
}

void GeneralControl::on_close_client(const long param1, const long param2) {
	mutex_lock lck(mtx_tcpc_client_);
	TCPClient* ptr = (TCPClient*) param1;
	TcpCVec::iterator it;

	for (it = tcpc_client_.begin(); it != tcpc_client_.end() && ptr != (*it).get(); ++it);
	if (it != tcpc_client_.end()) tcpc_client_.erase(it);
}

void GeneralControl::on_close_dome(const long param1, const long param2) {
	mutex_lock lck(mtx_tcpc_dome_);
	TCPClient* ptr = (TCPClient*) param1;
	DomeNetVec::iterator it;

	for (it = tcpc_dome_.begin(); it != tcpc_dome_.end() && ptr != (*it).tcp.get(); ++it);
	if (it != tcpc_dome_.end()) tcpc_dome_.erase(it);
}

/*!
 * @brief 解析与用户/数据库、转台、相机相关网络信息
 * @param client 网络资源
 * @param peer   远程主机类型
 */
void GeneralControl::resolve_protocol_ascii(TCPClient* client, int peer) {
	char term[] = "\n";	   // 换行符作为信息结束标记
	int len = strlen(term);// 结束符长度
	int pos;      // 标志符位置
	int toread;   // 信息长度
	apbase proto;

	while (client->IsOpen() && (pos = client->Lookup(term, len)) >= 0) {
		if ((toread = pos + len) > TCP_PACK_SIZE) {
			string ip = client->GetSocket().remote_endpoint().address().to_string();
			_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii",
					"too long message from IP<%s>. peer type is %s", ip.c_str(),
					peer == PEER_CLIENT ? "CLIENT" : "DOME");
			client->Close();
		}
		else {// 读取协议内容并解析执行
			client->Read(bufrcv_.get(), toread);
			bufrcv_[pos] = 0;

			proto = ascproto_->Resolve(bufrcv_.get());
			// 检查: 协议有效性及设备标志基本有效性
			if (!proto.use_count()) {
				_gLog.Write(LOG_FAULT, "GeneralControl::receive_protocol_ascii",
						"illegal protocol[%s]", bufrcv_.get());
				client->Close();
			}
			else if (peer == PEER_CLIENT) process_protocol_client(proto, client);
			else process_protocol_dome(proto, client);
		}
	}
}

void GeneralControl::process_protocol_client(apbase proto, TCPClient* client) {
	string type = proto->type;
	string gid  = proto->gid;

	if (iequals(type, APTYPE_RELOAD)) {// 重新加载配置参数
		ParamPtr param = boost::make_shared<Parameter>();
		param->LoadFile(gConfigPath);

		ats_.SetSite(param->siteLon, param->siteLat, param->siteAlt, param->timezone);
		if (1 <= param->openWindOpt && param->openWindOpt <= 3 && 1 <= param->cloWindOpt && param->cloWindOpt <= 3) {
			if (!iequals(param_->pathWeather, param->pathWeather)) {
				interrupt_thread(thrd_weather_);
				thrd_weather_.reset(new boost::thread(boost::bind(&GeneralControl::thread_weather, this)));
			}
		}
		else {
			_gLog.Write(LOG_FAULT, NULL, "UseWindSpd option of SlitOpen or SlitClose was wrong");
			interrupt_thread(thrd_weather_);
		}

		// 更新参数访问地址
		param_ = param;
	}
	else if (iequals(type, APTYPE_SLIT)) {// 手动控制天窗开关
		apslit slit = from_apbase<ascii_proto_slit>(proto);
		int cmd = slit->command;
		int n;
		const char *s = ascproto_->CompactSlit(slit, n);
		mutex_lock lck(mtx_tcpc_dome_);
		for (DomeNetVec::iterator it = tcpc_dome_.begin(); it != tcpc_dome_.end(); ++it) {
			if ((*it).automode || !(*it).IsMatched(gid)) continue;
			if ((cmd == DSC_OPEN && (*it).state == DSS_CLOSE)
					|| (cmd == DSC_CLOSE && (*it).state == DSS_OPEN)) {
				(*it).tcp->Write(s, n);
			}
		}
	}
	else if (iequals(type, APTYPE_START)) {// 启用自动开关天窗
		mutex_lock lck(mtx_tcpc_dome_);
		for (DomeNetVec::iterator it = tcpc_dome_.begin(); it != tcpc_dome_.end(); ++it) {
			if ((*it).IsMatched(gid)) (*it).automode = true;
		}
	}
	else if (iequals(type, APTYPE_STOP)) {// 禁用自动开关天窗
		mutex_lock lck(mtx_tcpc_dome_);
		for (DomeNetVec::iterator it = tcpc_dome_.begin(); it != tcpc_dome_.end(); ++it) {
			if ((*it).IsMatched(gid)) (*it).automode = false;
		}
	}
}

void GeneralControl::process_protocol_dome(apbase proto, TCPClient* client) {
	string type = proto->type;
	string gid  = proto->gid;

	if (iequals(type, APTYPE_SLIT) && !gid.empty()) {// 天窗状态
		apslit slit = from_apbase<ascii_proto_slit>(proto);
		DomeNetVec::iterator it;
		mutex_lock lck(mtx_tcpc_dome_);

		for (it = tcpc_dome_.begin(); it != tcpc_dome_.end() && client != (*it).tcp.get(); ++it);
		if (it != tcpc_dome_.end()) {
			if ((*it).gid.empty()) (*it).gid = gid;
			(*it).state = slit->state;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
int GeneralControl::create_server(TcpSPtr *server, const uint16_t port) {
	const TCPServer::CBSlot& slot = boost::bind(&GeneralControl::network_accept, this, _1, _2);
	*server = maketcp_server();
	(*server)->RegisterAccespt(slot);
	return (*server)->CreateServer(port);
}

bool GeneralControl::create_all_server() {
	int ec;

	if ((ec = create_server(&tcps_client_, param_->portClient))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for client on port<%d>. ErrorCode<%d>",
				param_->portClient, ec);
		return false;
	}
	if ((ec = create_server(&tcps_dome_, param_->portDome))) {
		_gLog.Write(LOG_FAULT, "GeneralControl::create_all_server",
				"Failed to create server for dome on port<%d>. ErrorCode<%d>",
				param_->portDome, ec);
		return false;
	}

	return true;
}

void GeneralControl::network_accept(const TcpCPtr& client, const long server) {
	TCPServer* ptr = (TCPServer*) server;

	/* 不使用消息队列, 需要互斥 */
	if (ptr == tcps_client_.get()) {// 客户端
		mutex_lock lck(mtx_tcpc_client_);
		tcpc_client_.push_back(client);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_client, this, _1, _2);
		client->RegisterRead(slot);
	}
	else if (ptr == tcps_dome_.get()) {// 转台
		mutex_lock lck(mtx_tcpc_dome_);
		DomeNetwork netdome;
		netdome.tcp = client;
		tcpc_dome_.push_back(netdome);
		client->UseBuffer();
		const TCPClient::CBSlot& slot = boost::bind(&GeneralControl::receive_dome, this, _1, _2);
		client->RegisterRead(slot);
	}
}

void GeneralControl::receive_client(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_CLIENT : MSG_RECEIVE_CLIENT, client);
}

void GeneralControl::receive_dome(const long client, const long ec) {
	PostMessage(ec ? MSG_CLOSE_DOME : MSG_RECEIVE_DOME, client);
}

//////////////////////////////////////////////////////////////////////////////
bool GeneralControl::read_weather(string &tmloc, double &spdopen, double &spdclose) {
	char line[200];
	char seps[] = " ";
	char *token;
	int pos(0);
	double spdreal[3];

	/* 尝试访问文件, 读取风速 */
	FILE *fp = fopen(param_->pathWeather.c_str(), "r");
	if (!fp) {
		_gLog.Write(LOG_FAULT, NULL, "failed to open weather file[%s]",
				param_->pathWeather.c_str());
	}
	else {
		while (!feof(fp)) {
			if (NULL == fgets(line, 200, fp)) continue;

			token = strtok(line, seps);
			while (token && pos < 9) {
				if (++pos == 1)    tmloc = token;
				else if (pos == 2) { tmloc += "T"; tmloc += token; }
				else if (pos == 7) spdreal[0] = atof(token);
				else if (pos == 8) spdreal[1] = atof(token);
				else if (pos == 9) spdreal[2] = atof(token);

				token = strtok(NULL, seps);
			}
		}
		fclose(fp);
	}
	if (pos == 9) {
		spdopen  = spdreal[param_->openWindOpt - 1];
		spdclose = spdreal[param_->cloWindOpt - 1];
	}

	return pos == 9;
}

double GeneralControl::sun_altitude() {
	double ra, dec, lmst;
	double azi, alt;
	ptime now = second_clock::universal_time();
	ptime::date_type today = now.date();

	ats_.SetUTC(today.year(), today.month(), today.day(), now.time_of_day().total_seconds() / 86400.0);
	lmst = ats_.LocalMeanSiderealTime();
	ats_.SunPosition(ra, dec);
	ats_.Eq2Horizon(lmst - ra, dec, azi, alt);

	return (alt * R2D);
}

void GeneralControl::switch_slit(DomeNetwork &dome, int odt, double spdopen, double spdclo) {
	if (dome.state == DSS_OPENING || dome.state == DSS_CLOSING) {
		ptime now = second_clock::universal_time();
		if (!dome.tmlast.is_special() && (now - dome.tmlast).total_seconds() >= 300) {
			_gLog.Write(LOG_WARN, NULL, "Dome[%s] had taken too long time to %s slit",
					dome.gid.c_str(), dome.state == DSS_OPENING ? "open" : "close");
		}
		dome.tmlast = now;
	}
	else {
		apslit slit = boost::make_shared<ascii_proto_slit>();
		int n(0);
		const char *s;
		if (odt == ODT_DAY) {// 白天: 检查天窗是否未关闭
			if (dome.state == DSS_OPEN) {// 需要关闭
				slit->command = DSC_CLOSE;
			}
		}
		else {
			if (dome.state == DSS_OPEN) {// 判断是否需要关闭
				if (spdclo >= param_->cloWindSpdEmergency) dome.cntclose = param_->cloContNum;
				else if (spdclo >= param_->cloWindSpd) ++dome.cntclose;
				else if (dome.cntclose) dome.cntclose = 0;

				if (dome.cntclose >= param_->cloContNum) slit->command = DSC_CLOSE;
			}
			else if (dome.state == DSS_CLOSE) {// 判断是否需要打开
				if (spdopen < param_->openWindSpd) ++dome.cntopen;
				else if (dome.cntopen) dome.cntopen = 0;

				if (dome.cntopen >= param_->openContNum) slit->command = DSC_OPEN;
			}
		}

		if (slit->command >= DSC_OPEN) {
			dome.tmlast = second_clock::universal_time();
			s = ascproto_->CompactSlit(slit, n);
		}
		if (n) dome.tcp->Write(s, n);
	}
}

//////////////////////////////////////////////////////////////////////////////
void GeneralControl::thread_weather() {
	boost::chrono::minutes period(1);	// 周期: 1分钟
	string tmold, tmnew;	// 气象数据文件中的本地时
	double spdopen, spdclo;	// 实时风速: 用于开关天窗判据
	double altsun;
	int odt; // 观测时段类型

	while(1) {
		boost::this_thread::sleep_for(period);

		if (!read_weather(tmnew, spdopen, spdclo)) {
			_gLog.Write(LOG_FAULT, NULL, "failed to access weather file or wrong file style");
		}
		else if (tmold == tmnew) {
			_gLog.Write(LOG_FAULT, NULL, "gotten same time flag from weather file");
		}
		else {
			tmold = tmnew;
			// 计算太阳高度角和时段类型
			altsun = sun_altitude();
			odt = altsun >= param_->openSunAlt && altsun >= param_->cloSunAlt ? ODT_DAY : ODT_NIGHT;
			// 逐一检查并改变天窗开关状态
			mutex_lock lck(mtx_tcpc_dome_);
			for (DomeNetVec::iterator it = tcpc_dome_.begin(); it != tcpc_dome_.end(); ++it) {
				if ((*it).automode) switch_slit(*it, odt, spdopen, spdclo);
			}
		}
	}
}
