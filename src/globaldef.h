/*
 * @file globaldef.h 声明全局唯一参数
 * @version 0.1
 * @date 2019/10/09
 */

#ifndef GLOBALDEF_H_
#define GLOBALDEF_H_

// 软件名称、版本与版权
#define DAEMON_NAME			"annaes"
#define DAEMON_VERSION		"v0.1 @ Oct, 2019"
#define DAEMON_AUTHORITY	"© NAOC"

// 日志文件路径与文件名前缀
const char gLogDir[]    = "/var/log/annaes";
const char gLogPrefix[] = "annaes_";

// 软件配置文件
const char gConfigPath[] = "/usr/local/etc/annaes.xml";

// 文件锁位置
const char gPIDPath[] = "/var/run/annaes.pid";

#endif /* GLOBALDEF_H_ */
