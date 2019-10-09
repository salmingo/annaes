/*!
 * @file AstroDeviceDef.h 声明与光学天文望远镜观测相关的状态、指令、类型等相关的常量
 * @version 0.1
 * @date 2017-11-24
 */

#ifndef ASTRO_DEVICE_DEFINE_H_
#define ASTRO_DEVICE_DEFINE_H_

/* 状态与指令 */
/*!
 * ODT: Observation Duration Type, 观测时间类型
 */
enum {// 观测时间分类
	ODT_DAY = 1,	//< 白天
	ODT_NIGHT	//< 夜间
};

/*!
 * DSC: Dome Slit Command, 圆顶天窗开关指令
 */
enum {
	DSC_OPEN = 1,	//< 打开
	DSC_CLOSE		//< 关闭
};

/*!
 * DSS: Dome Slit State, 圆顶天窗状态
 */
enum {// 天窗状态
	DSS_ERROR,		//< 错误
	DSS_OPEN,		//< 已打开
	DSS_CLOSE,		//< 已关闭
	DSS_OPENING,	//< 打开中
	DSS_CLOSING		//< 关闭中
};

#endif
