/*!
 * @file AsciiProtocol.cpp 封装通信协议
 * @date 09 Oct, 2019
 * @version 0.1
 */

#include <boost/make_shared.hpp>
#include <boost/algorithm/string.hpp>
#include "AsciiProtocol.h"
using namespace boost;

AsciiProtocol::AsciiProtocol() {
	ibuf_ = 0;
	buff_.reset(new char[1024 * 10]); //< 存储区
}

AsciiProtocol::~AsciiProtocol() {

}
const char* AsciiProtocol::output_compacted(string& output, int& n) {
	trim_right_if(output, is_punct() || is_space());
	return output_compacted(output.c_str(), n);
}

const char* AsciiProtocol::output_compacted(const char* s, int& n) {
	mutex_lock lck(mtx_);
	char* buff = buff_.get() + ibuf_ * 1024;
	if (++ibuf_ == 10) ibuf_ = 0;
	n = sprintf(buff, "%s\n", s);
	return buff;
}

void AsciiProtocol::compact_base(apbase base, string &output) {
	output = base->type + " ";
	if (!base->gid.empty()) join_kv(output, "gid",  base->gid);
}

bool AsciiProtocol::resolve_kv(string& kv, string& keyword, string& value) {
	char seps[] = "=";	// 分隔符: 等号
	listring tokens;

	keyword = "";
	value   = "";
	algorithm::split(tokens, kv, is_any_of(seps), token_compress_on);
	if (!tokens.empty()) { keyword = tokens.front(); trim(keyword); tokens.pop_front(); }
	if (!tokens.empty()) { value   = tokens.front(); trim(value); }
	return (!(keyword.empty() || value.empty()));
}

void AsciiProtocol::resolve_kv_array(listring &tokens, likv &kvs, ascii_proto_base &basis) {
	string keyword, value;

	for (listring::iterator it = tokens.begin(); it != tokens.end(); ++it) {// 遍历键值对
		if (!resolve_kv(*it, keyword, value)) continue;
		// 识别通用项
		if (iequals(keyword, "gid"))  basis.gid = value;
		else {// 存储非通用项
			pair_key_val kv;
			kv.keyword = keyword;
			kv.value   = value;
			kvs.push_back(kv);
		}
	}
}
//////////////////////////////////////////////////////////////////////////////
apbase AsciiProtocol::resolve_start() {
	apstart proto = boost::make_shared<ascii_proto_start>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_stop() {
	apstop proto = boost::make_shared<ascii_proto_stop>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_reload() {
	apreload proto = boost::make_shared<ascii_proto_reload>();
	return to_apbase(proto);
}

apbase AsciiProtocol::resolve_slit(likv &kvs) {
	apslit proto = boost::make_shared<ascii_proto_slit>();
	string keyword;

	for (likv::iterator it = kvs.begin(); it != kvs.end(); ++it) {// 遍历键值对
		keyword = (*it).keyword;
		// 识别关键字
		if      (iequals(keyword, "command"))  proto->command  = stoi((*it).value);
		else if (iequals(keyword, "state"))    proto->state    = stoi((*it).value);
	}

	return to_apbase(proto);
}

apbase AsciiProtocol::Resolve(const char *rcvd) {
	const char seps[] = ",", *ptr;
	char ch;
	listring tokens;
	apbase proto;
	string type;
	likv kvs;
	ascii_proto_base basis;

	// 提取协议类型
	for (ptr = rcvd; *ptr && *ptr != ' '; ++ptr) type += *ptr;
	while (*ptr && *ptr == ' ') ++ptr;
	// 分解键值对
	if (*ptr) algorithm::split(tokens, ptr, is_any_of(seps), token_compress_on);
	resolve_kv_array(tokens, kvs, basis);
	// 按照协议类型解析键值对
	if (type[0] == 's' || type[0] == 'S') {
		if      (iequals(type, APTYPE_SLIT))   proto = resolve_slit(kvs);
		else if (iequals(type, APTYPE_START))  proto = resolve_start();
		else if (iequals(type, APTYPE_STOP))   proto = resolve_stop();
	}
	else if (iequals(type, APTYPE_RELOAD))  proto = resolve_reload();

	if (proto.use_count()) {
		proto->type = type;
		proto->gid  = basis.gid;
	}

	return proto;
}

const char *AsciiProtocol::CompactStart(const string &gid, int &n) {
	string output = APTYPE_START;
	output += " ";
	if (!gid.empty()) join_kv(output, "gid", gid);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactStop(const string &gid, int &n) {
	string output = APTYPE_STOP;
	output += " ";
	if (!gid.empty()) join_kv(output, "gid", gid);
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactReload(int &n) {
	string output = APTYPE_RELOAD;
	output += " ";
	return output_compacted(output, n);
}

const char *AsciiProtocol::CompactSlit(apslit proto, int &n) {
	if (!proto.use_count()) return NULL;

	string output;
	compact_base(to_apbase(proto), output);
	if (proto->command != -1) join_kv(output, "command", proto->command);
	if (proto->state != -1)   join_kv(output, "state",   proto->state);
	return output_compacted(output, n);
}
