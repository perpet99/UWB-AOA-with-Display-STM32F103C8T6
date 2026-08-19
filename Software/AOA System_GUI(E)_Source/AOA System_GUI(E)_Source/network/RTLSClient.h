// -------------------------------------------------------------------------------------------------------------------
//
//  File: RTLSClient.h
//
//  Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//  Author:
//
// -------------------------------------------------------------------------------------------------------------------

#ifndef RTLSCLIENT_H
#define RTLSCLIENT_H

#include <QObject>

#include "SerialConnection.h"
#include <stdint.h>


/*
 *	V1.0.7:
 *			一.更改平滑滤波，从c语言切换成C++
 *
 *  V2.0.0:
 *          一.增加串口调试助手界面
 *          二.修改View配置界面
 *              ①增加低电压显示
 *              ②增加角度显示
 *
 *  V2.0.3:
 *          一.发布版本
 *          二.串口界面有卡顿(原因未知)
 *
 */


class QFile;

#define HIS_LENGTH 100
#define FILTER_SIZE 25/*10*/  //NOTE: filter size needs to be > 2

#define CALIB_IGNORE_LEN 200 //NOTE: If a node is started from "cold", it will take a number of ranges to come up to
                             // the operational temperature. This temperature drift will cause offset to drift during
                             // the initial number of ranges. Thus while doing calibration the 1st 200 ranges will be ignored.
#define CALIB_HIS_LEN 200



typedef struct{
	/*是否低电量*/
	uint16_t is_lowbattery:1;
	/*是否报警*/
	uint16_t is_alarm:1;
	/*是否充电*/
	uint16_t is_chrg:1;
	/*是否充满电*/
	uint16_t is_tdby:1;		
	/*电池电压		350=3.50V*/
	uint16_t battery_val:10;
	/*保留*/
    uint16_t is_offset_range_zero_bit:1;
    uint16_t is_offset_pdoa_zero_bit:1;
}__attribute__((packed))
user_cmd_t;


typedef struct
{
    double x_arr[HIS_LENGTH]; //array containing history of x coordinates
    double y_arr[HIS_LENGTH]; //array containing history of y coordinates

    quint64 id64;
    int     id16;
    short   multFast;
    short   multSlow;
    short   Mode;

    int arr_idx;
    int count;
    int filterReady;
    bool ready;

    //motion filter
    bool motionFilterReady; //set to true when enough data accumulated in the filter array
    int filterHisIdx;
    double estXHis[FILTER_SIZE];
    double estYHis[FILTER_SIZE];

} tag_reports_t;

typedef struct
{
    double x, y, z;
    int id;
    QString label;
    double phaseCorection;
    double rangeCorection;
} node_struct_t;


class RTLSClient : public QObject
{
    Q_OBJECT

public:
    explicit RTLSClient(QObject *parent = 0);

    void updateTagStatistics(int i, double x, double y);
    void addTagToList(quint64 id64);
    void updateTagAddr16(quint64 id64, int id16);
    void addTagFromKList(short slot, quint64 id64, short id16,short mFast, short mSlow, short mode);

    void enableMotionFilter(bool enabled);
    void enablePhaseAndDistCalibration(quint64 id64, double distance);

    void phaseAndRangeCalibration(double phase, double range);
    void motionFilter(double *x, double *y, int tid);

    double getPhaseOffset(void);
    double getRangeOffset(void);

    void processRangeAndPDOAReport(int tid, int seq,
                                    double range_m, double x_m, double y_m,
                                    double pdoa_rad,
                                    int mode, int vec_x, int vec_y, int vec_z);

    void sendPhaseAndRangeCorrectionToNode(double phase, double range);

    void updatePDOAandRangeOffset(int pdoa_offset_mrad, int range_offset_cm);

    void removeTagFromList(quint64 id64);

    void _dbg_printf3(const char *format, ...);

signals:

    void clearTags();
    void tagPos(quint64 tagId, double x, double y, int mode);
    void nodePos(int nodeId, double x, double y);
    void tagRange(quint64 tagID, double range, double x, double y, int angle, int mode,int Acc_x,int Acc_y, int Acc_z);
    void statusBarMessage(QString status);
    void Signal_Uart_Recvdata(QByteArray data);

    void centerOnNodes();

    //void hideAll();

    void addDiscoveredTag(quint64 tagID, int ID16, bool known, int fastrate, int imu);

    void calibrationBar(int currentVal);
    void phaseOffsetUpdated(double phaseOffset);
    void rangeOffsetUpdated(double rangeOffset);

protected slots:
    void onReady();
    void onConnected(QString conf);
    void GetList();
    void save();

    void Slot_RangeLog_Generate(void);

private slots:
    void newData();
    void connectionStateChanged(SerialConnection::ConnectionState);

private:
    bool _first;

    QList <tag_reports_t> _tagList;

    SerialConnection *_serial;

    node_struct_t _nodeConfig[1]; //current demo has only 1 PDOA node
    int _nodeAdd;

    QString _verNode;
    QString _verGUI;

    bool _motionFilterOn;    //set to true when motion filter is running

    int calibInx;

    double phaseHisCalib[CALIB_HIS_LEN];
    double rangeHisCalib[CALIB_HIS_LEN];
    double _calibrationDistance;
    bool _phaseCalibration;
    bool _calibrationDone;
    quint64 _calibrationTagID;

    QByteArray _data;

};

void r95Sort(double s[], int l, int r);

#endif // RTLSCLIENT_H
