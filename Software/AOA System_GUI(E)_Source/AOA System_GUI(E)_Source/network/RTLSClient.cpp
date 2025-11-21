// -------------------------------------------------------------------------------------------------------------------
//
//  File: RTLSClient.cpp
//
//  Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
// ==============================================================================
// =                                                                            =
// =                                                                            =
// =                D E C A W A V E    C O N F I D E N T I A L                  =
// =                                                                            =
// =                                                                            =
// ==============================================================================
//
// This software contains Decawave confidential information and techniques,
// subject to licence and non-disclosure agreements.  No part of this software
// package may be revealed to any third-party without the express permission of
// Decawave Ltd.
//
// ==============================================================================
//  Author:
//      Decawave
//
// -------------------------------------------------------------------------------------------------------------------

#include "RTLSClient.h"

#include "RTLSDisplayApplication.h"
#include "SerialConnection.h"
#include "mainwindow.h"

#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <QFile>
#include <QDebug>
#include <math.h>
#include <QMessageBox>

#include <QDomDocument>
#include <QFile>
#include "json_utils.h"
#include "windows.h"

//e.g. RAtagID(16bit) seq (8bit) range(32bit signed mm) PDOA (32bit signed mrad) mode (8bit)
//"RA%04x %02x %04x %04x %02x"
//RA0000 cd 0000057b 0000838b 00
#define PHASE_RANGE_REPORT_ARGS (9)
#define PHASE_RANGE_REPORT_LEN  (58)

#define ANT_FACTOR (1.8)  // ANT_FACTOR depends on antenna characteristics


static bool is_startLog = false;
static FILE* file_T;

/**
* @brief RTLSDisplayApplication
*        Constructor; The application consumes the data received over the COM port connection and sends the
*        processed data to the graphical display
* */
RTLSClient::RTLSClient(QObject *parent) :
    QObject(parent),
    _first(true)
{
    _motionFilterOn = false;

    _serial = NULL;

    _nodeConfig[0].phaseCorection = 0;
    _nodeConfig[0].rangeCorection = 0;
    _phaseCalibration = false;
    _calibrationDone = false;
    _calibrationDistance = 0 ;
    calibInx = 0;
    calibInx = 0;

    for (int i = 0; i<CALIB_HIS_LEN; i++)
    {
        phaseHisCalib[i] = 0;
        rangeHisCalib[i] = 0;
    }

    RTLSDisplayApplication::connectReady(this, "onReady()");

}

/**
* @brief onReady()
*        When onReady signal is received by the RTLS Client the client sets up a connection to serialConnection
*        serialOpened signal.
* */
void RTLSClient::onReady()
{
    QObject::connect(RTLSDisplayApplication::serialConnection(), SIGNAL(serialOpened(QString)),
                         this, SLOT(onConnected(QString)));

}

/**
* @brief onConnected()
*        When onConnected signal is received by the RTLS Client the client gets ready to consume the data from the
*        opened COM port (connectes to readyRead() signal). It also opens a new log file and updates the status bar message
*        on the main window.
* */
void RTLSClient::onConnected(QString conf)
{
    qDebug() << "RTLSClient.cpp onConnected";

    bool ok = true;

    //update the status bar message with Node's version
    emit statusBarMessage(QString("UWB-X2-AOA-N node connected (%1)").arg(conf));

    //save Node's version
    _verNode = conf ;
    //get application version to write into the log file
    _verGUI = RTLSDisplayApplication::mainWindow()->version();

    //get pointer to Serial Connection object and set up a connection to serial ports readyRead() signal
    _serial = RTLSDisplayApplication::serialConnection();
    connect(_serial->serialPort(), SIGNAL(readyRead()), this, SLOT(newData()));

    // send STOP command and then get a list of known tag's from the node
    _serial->clear();
    Sleep(100);

    _serial->writeData("getKList\r\n");
    Sleep(1000);

    //start the Node application (as it was stopped above)
    //start periodic timer, to periodically request a new tag's list
    _serial->timerUpdateStart(2000);

    //when conneted to the node the 1st time - emit signal to GraphicsWidget to add Node object.
    //notify the user which node the GUI is connected to (Node version)
    if(_first)
    {
        //set the position of the node on the screen
        emit nodePos(0, 0, 0); //it is always placed on 0,0 (origin)

        _first = false;
    }
}

/**
* @brief updateTagAddr16()
*        update the tag's 16-bit address when the tag added confirmation received from the node
*        Note: although the client sets the tag's address as part of adding of the tag to the node's known list,
*        the node may change the initial address if it sees any conflicts with the already added tags (i.e. the
*        given address matches one of the node's existing tags)
* */
void RTLSClient::updateTagAddr16(quint64 id64, int id16)
{
    int idx = -1;

    //find the tag in the _tagList
    for(idx=0; idx<_tagList.size(); idx++)
    {
        //if this is the tag update the 16-bit address
        if(_tagList.at(idx).id64 == id64)
        {
            tag_reports_t rp = _tagList.at(idx);

            rp.id16 = id16 ;

            //update the list entry
            _tagList.replace(idx, rp);

            continue; //break;
        }

        if(_tagList.at(idx).id16 == id16) //another tag with same 16 bit address
        {
            tag_reports_t rp = _tagList.at(idx);

            rp.id16 = -1 ;

            //update the list entry
            _tagList.replace(idx, rp);
        }

    }
}

/**
* @brief removeTagFromList()
*        remove existing tag from client's tag list
* */
void RTLSClient::removeTagFromList(quint64 id64)
{
    //first check if we have this tag already
    int idx;
    //find the tag in the list
    for(idx=0; idx<_tagList.size(); idx++)
    {
        //find this tag in the list
        if(_tagList.at(idx).id64 == id64)
        {
            break;
        }
    }

    //remove the tag from the list
    if(idx < _tagList.size())
    {
        _tagList.removeAt(idx);
    }

}

/**
* @brief addTagToList()
*        Add a newly discovered tag into the client's tag list and initialise the node tag aoa/range reports structure
*        Send a signal to the graphics display to add the tag to its database
* */
void RTLSClient::addTagToList(quint64 id64)
{
    tag_reports_t r;

    //first check if we have this tag already
    int idx;
    //find the tag in the list
    for(idx=0; idx<_tagList.size(); idx++)
    {
        //find this tag in the list
        if(_tagList.at(idx).id64 == id64)
        {
            return;
        }
    }

    //add new tag to our list
    memset(&r, 0, sizeof(tag_reports_t));
    r.id16 = - 1 ; //don't assign the 16 bit address here ... node will do it when tag joins
    r.id64 = id64;
    r.Mode = 0;
    r.multFast = 1;
    r.multSlow = 1;
    r.ready = false;
    r.filterReady = 0;

    r.filterHisIdx = 0;
    r.motionFilterReady = false;
    for (int i = 0; i<FILTER_SIZE; i++)
    {
       r.estXHis[i] = 0;
       r.estYHis[i] = 0;
    }

    _tagList.append(r);

    //Add newly discovered tag to the list (64-bit ID, 16-bit ID, false as it is new)
    emit addDiscoveredTag(r.id64, r.id16, false, -1, 0);
}

/**
* @brief addTagFromKList()
*        Add a tag (from received Node's Known List into the client's tag list and initialise the node tag aoa/range reports structure
*        Send a signal to the graphics display to add the tag to its database
* */
void RTLSClient::addTagFromKList(short slot, quint64 id64, short id16,
                                 short mFast, short mSlow, short mode)
{
    tag_reports_t r;
    memset(&r, 0, sizeof(tag_reports_t));
    r.id16 = id16;
    r.id64 = id64;
    r.Mode = mode;
    r.multFast = mFast;
    r.multSlow = mSlow;
    r.ready = true;
    r.filterReady = 0;
    //r.rangeSeq = -1;
    //memset(&r.rangeValue[0][0], -1, sizeof(r.rangeValue));

    r.filterHisIdx = 0;
    r.motionFilterReady = false;
    for (int i = 0; i<FILTER_SIZE; i++)
    {
       r.estXHis[i] = 0;
       r.estYHis[i] = 0;
    }

    _tagList.append(r);

    //Add a known tag to the list (64-bit ID, 16-bit ID, true as it is already known)
    emit addDiscoveredTag(r.id64, r.id16, true, mFast, (mode & 0x1));

    _serial->gotKlist(true);
}

/**
* @brief updatePDOAandRangeOffset()
*        Function to update the range and PDOA offsets in the client and on the GUI
* */
void RTLSClient::updatePDOAandRangeOffset(int pdoa_offset_mrad, int range_offset_mm)
{
    _nodeConfig[0].phaseCorection = (double)pdoa_offset_mrad/1000 ;
    _nodeConfig[0].rangeCorection = (double)range_offset_mm/1000 ;

	qDebug() << "------------------------updatePDOAandRangeOffset" << _nodeConfig[0].phaseCorection << _nodeConfig[0].rangeCorection;

    emit phaseOffsetUpdated(_nodeConfig[0].phaseCorection);
    emit rangeOffsetUpdated(_nodeConfig[0].rangeCorection);
}

/**
* @brief stddev()
*        Function to calculate STDDEV of the elements of the array
* */
double stddev(double *array, int length)
{
    int i;
    double mean = 0;
    double sum = 0;

    //find a mean
    for(i=0; i<length; i++)
    {
        mean += array[i];
    }

    mean /= length;

    for(i=0; i<length; i++)
    {
        sum += ((array[i] - mean)*(array[i] - mean));
    }

    sum /= (length - 1) ;

    return sqrt (sum) ;
}

/**
* @brief updateTagStatistics()
*        Update tag's statistics: the location history array and the average
* */
void RTLSClient::updateTagStatistics(int i, double x, double y)
{
    int idx = _tagList.at(i).arr_idx;

    tag_reports_t rp = _tagList.at(i);

    //update the value in the array
    rp.x_arr[idx] = x;
    rp.y_arr[idx] = y;

    rp.arr_idx++;
    //wrap the index
    if(rp.arr_idx >= HIS_LENGTH)
    {
        rp.arr_idx = 0;
        rp.ready = true;
        rp.filterReady = 1;
    }

    rp.count++;

    if(rp.filterReady > 0)
    {

        if(rp.filterReady == 1)
        {
            rp.filterReady++;
        }

    }

    //update the list entry
    _tagList.replace(i, rp);
}

/**
* @brief enableMotionFilter()
*        Set _motionFilterOn flag to the given value
* */
void RTLSClient::enableMotionFilter(bool enabled)
{
    _motionFilterOn = enabled;
}

/**
* @brief getPhaseOffset()
*        Get phase corection / offset value
* */
double RTLSClient::getPhaseOffset(void)
{
    return _nodeConfig[0].phaseCorection ;
}

/**
* @brief getRangeOffset()
*        Get range corection / offset value
* */
double RTLSClient::getRangeOffset(void)
{
    return _nodeConfig[0].rangeCorection ;
}

/**
* @brief enablePhaseAndDistCalibration()
*        Clear calibration data and enable calibration; clear the calibration data displayed on the GUI
* */
void RTLSClient::enablePhaseAndDistCalibration(quint64 id64, double distance)
{
    _calibrationTagID = id64;
    _phaseCalibration = true;
    _calibrationDone = false;
    _calibrationDistance = distance;
    _nodeConfig[0].rangeCorection = 0;
    _nodeConfig[0].phaseCorection = 0;

    for (int i = 0; i<CALIB_HIS_LEN; i++)
    {
        phaseHisCalib[i] = 0;
        rangeHisCalib[i] = 0;
    }

    calibInx = 0;
    calibInx = 0;

    //clear the values in the GUI - maybe not needed...
    emit phaseOffsetUpdated(_nodeConfig[0].phaseCorection);
    emit rangeOffsetUpdated(_nodeConfig[0].rangeCorection);

    //clear the values in the node, so that reported range is raw
    sendPhaseAndRangeCorrectionToNode(0, 0);
}

/**
* @brief sendPhaseCorrectionToNode()
*        Send the calculated phase correction offset to the Node also issue the SAVE command to save it
* */
void RTLSClient::sendPhaseAndRangeCorrectionToNode(double phase, double range)
{
    //久凌电子 计算出角度 = 弧度*180/π
    uint16_t    tmp = (uint16_t)(180*phase/M_PI);
    QString s_pdof = QString("pdoaoff %1\r\n").arg(tmp, 4, 10, QChar('0'));
    _serial->writeData(s_pdof.toLocal8Bit());

    Sleep(50);

    tmp = (uint16_t)(range*1000);
    s_pdof = QString("rngoff %1\r\n").arg(tmp, 4, 10, QChar('0'));
    _serial->writeData(s_pdof.toLocal8Bit());

    Sleep(50);

    _serial->writeData("save\r\n");
}

/**
* @brief processRangeAndPDOAReport()
*        Function to parse the JSON format range and PDOA reports from the node
* */
void RTLSClient::processRangeAndPDOAReport(int tid,
                                            int seq,
                                            double range_m,
                                            double x_m,
                                            double y_m,
                                            double pdoa_deg,
                                            int mode,
                                            int vec_x,
                                            int vec_y,
                                            int vec_z)
{
    int n, idx;
    int tag_index = -1;
    int angle;

    double pdoa_rad = (pdoa_deg / 180.0) * M_PI ;   //弧度 = 角度/180.0 * π

    //1st need to check if we know about this tag, if we do it will be in the list,
    //find the tag in the list
    for(idx=0; idx<_tagList.size(); idx++)
    {
        //find this tag in the list
        if(_tagList.at(idx).id16 == tid)
        {
            tag_index = idx;
            break;
        }
    }

    if(tag_index == -1)
    {
        //error - this tag is not in the list ...
        //must send command to node to obtain known tag list
        return;
    }

    QDateTime now = QDateTime::currentDateTime();
    QString nowstr = now.toString("T:hhmmsszzz:");

    //if invalid ("DEAD") value report to log
    if(((int) x_m) == 0xDEADBEEF ||\
       ((int) y_m) == 0xDEADBEEF ||\
       ((int) (x_m*1000)) == 0 ||\
       ((int) (y_m*1000)) == 0)
    {
    }
    // have rang/PDOA report  - update position on the GUI
    {
        tag_reports_t rp1;
        double x, y; //coordinates for plotting on the GUI
        double estCoordPhaseDeg; //this is the filtered phase (if motion filtering is on)

        x = x_m; y = -y_m; //for GUI the y-axis increases downwards

        // Motion Filter of estimation coordinates and phase correction part of stationary node filter
        motionFilter(&x, &y, tag_index);

        angle = atan(x / y) * 180.0 / M_PI;


        // update position on screen
        emit tagPos(_tagList.at(tag_index).id64, x, y, mode);

        //PDOA calibration
        if (_phaseCalibration && (_tagList.at(tag_index).id64 == _calibrationTagID)) //check if correct tag
        {
            //when calculating pdoa offset, the raw pdoa should be used (i.e. before any offset is applied),
            //need to make sure the ranges/pdoa reported from node are using offsets of 0
            if((mode & 0xC000) == 0xC000) //offsets are clear
            {
                phaseAndRangeCalibration(pdoa_rad, (range_m - _calibrationDistance));

                emit calibrationBar (calibInx);

                if (_calibrationDone)
                {
                	qDebug() << "校准";
                    _phaseCalibration = false;
                    emit phaseOffsetUpdated(_nodeConfig[0].phaseCorection);
                    emit rangeOffsetUpdated(_nodeConfig[0].rangeCorection);
                    emit centerOnNodes();

                    sendPhaseAndRangeCorrectionToNode(_nodeConfig[0].phaseCorection, _nodeConfig[0].rangeCorection);
                }
            }
        }

        {
            double a1, stdev;
            //update tag position statistics
            updateTagStatistics(tag_index, x, y); //phase changed to degrees before STDEV calc.

            rp1 = _tagList.at(tag_index);

            emit tagRange(rp1.id64, range_m, x, y, angle, mode, vec_x, vec_y, vec_z);
        }
    } //end of PDOA processing


	_dbg_printf3("Tag_Addr:%04X, Seq:%d, Xcm:%.2f, Ycm:%.2f, Range:%.2f, Angle:%d\n",
							tid,
							seq,
							x_m,
							y_m,
							range_m,
                            angle);


}
void RTLSClient::GetList()
{
    _serial->writeData("GetList\r\n");

}
void RTLSClient::save()
{
    _serial->writeData("save\r\n");

}

/**
* @brief newData()
*        Function to consume and parse the data received on the serial connection from the Node
* */
QByteArray report_data;

void RTLSClient::newData()
{
    QByteArray data = _serial->serialPort()->readAll();
	
    report_data.append(data);
    //界面显示上位机数据
    if(report_data.lastIndexOf("\r\n") != -1)
	{	
        QByteArray report_byte = report_data.left(report_data.lastIndexOf("\r\n")+2);
        report_data = report_data.mid(report_data.lastIndexOf("\r\n")+2);

        emit Signal_Uart_Recvdata(report_byte);
	}
    if(report_data.size() > 9096)
	    report_data.clear();	


#if 1

    QByteArray dataChunk;

    int offset = 0;

    _data.append(data);

    int length = _data.length();

	//qDebug() << "_data_len" << length << "data_len" << data.length();

    while(length > 1) // we have received a range report from the node
    {
        QByteArray header = _data.mid(offset, 2);

        if(header.contains("JS")) //loop here until we reach header ("JS")
        {
//123456789....  : length
//      12345... : jlength : JSON object length, including curly brackets
//JS  xx{"TWR": {"a16":"2E5C","R":3,"T":8605,"D":343,"P":1695,"Xcm":165,"Ycm":165,"O":14,"V":1,"X":53015,"Y":60972,"Z":10797}}
            if(length > 6)
            {
                bool ok = false;
                int jlength =  _data.mid(offset+2, 4).toInt(&ok,16);
                //qDebug() << "json len "<< jlength;
                if(jlength == 0)
                {
                    qDebug() << _data;
                    length = 0;     //清除所有字符
                    break;
                }
                if(ok && (length >= jlength+6))
                {
                	//qDebug() << "check_json_stream";
                    dataChunk = _data.mid(offset+6, jlength);
                    check_json_stream(dataChunk);

                    offset += (jlength+6);
                    length -= (jlength+6);
                }
                else
                {
                    break;
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            offset += 1;
            length -= 1;
        }

    } //end of while
	//qDebug() << "ritgh length" << length;
    _data = _data.right(length);	
#endif

}

/**
* @brief connectionStateChanged()
*        Consumes the state changed signal from the serial connection, if
*        serial port is closed/disconnected then close the log file and disconnect the readyRead signal
* */
void RTLSClient::connectionStateChanged(SerialConnection::ConnectionState state)
{
	//久凌电子  	0:断开    1:连接，并读取数据，槽newData()
    qDebug() << "RTLSClient::connectionStateChanged " << state;

    if(state == SerialConnection::Disconnected) //disconnect from Serial Port
    {
        if(_serial != NULL)
        {
            disconnect(_serial->serialPort(), SIGNAL(readyRead()), this, SLOT(newData()));
            _serial = NULL;
        }

        _tagList.clear();

        //clear tags in the GraphicsWidget table
    }

}

/**
* @brief phaseAndRangeCalibration()
*        called during calibration to calculate the PDOA and range offsets.
* */
void RTLSClient::phaseAndRangeCalibration(double phase, double range)
{
	//久凌电子
    if ((calibInx >= CALIB_IGNORE_LEN) && (calibInx < (CALIB_HIS_LEN + CALIB_IGNORE_LEN)))
    {
        phaseHisCalib [calibInx - CALIB_IGNORE_LEN] = phase;
        rangeHisCalib [calibInx - CALIB_IGNORE_LEN] = range;
        calibInx = calibInx + 1;

    }
    else
    {
        if (calibInx == (CALIB_HIS_LEN + CALIB_IGNORE_LEN))
        {
            double phaseSum = 0, rangeSum = 0;

            for (int i = 0; i<CALIB_HIS_LEN; i++)
            {
                phaseSum = phaseHisCalib[i] + phaseSum;
                rangeSum = rangeHisCalib[i] + rangeSum;
            }

            _nodeConfig[0].rangeCorection = rangeSum/CALIB_HIS_LEN;
            _nodeConfig[0].phaseCorection = phaseSum/CALIB_HIS_LEN;

            _calibrationDone = true;
        }
        else
        {
            calibInx++;
        }

    }

}

/**
* @brief motionFilter()
*        perfrom the motion-filter on the x and y inputs
* */
void RTLSClient::motionFilter(double* x, double* y, int i)
{
     tag_reports_t rp = _tagList.at(i);

     if (rp.filterHisIdx >= FILTER_SIZE)
     {
         rp.filterHisIdx = fmod(rp.filterHisIdx,FILTER_SIZE);
         rp.motionFilterReady = true;
     }

     rp.estXHis[rp.filterHisIdx] = *x;
     rp.estYHis[rp.filterHisIdx] = *y;

     rp.filterHisIdx = rp.filterHisIdx + 1;

     if (_motionFilterOn)
     {
         if(rp.motionFilterReady)
         {
             double tempX[FILTER_SIZE];
             memcpy(tempX, &rp.estXHis[0], sizeof(tempX));

             double tempY[FILTER_SIZE];
             memcpy(tempY, &rp.estYHis[0], sizeof(tempY));

             r95Sort(tempX,0,FILTER_SIZE-1);

             *x = (tempX[FILTER_SIZE/2] + tempX[FILTER_SIZE/2-1])/2;

             r95Sort(tempY,0,FILTER_SIZE-1);

             *y = (tempY[FILTER_SIZE/2] + tempY[FILTER_SIZE/2-1])/2;
         }

     }

     //update the list entry
     _tagList.replace(i, rp);
 }

/**
* @brief r95Sort()
*        R95 sort used by the motion filter function above
* */
void r95Sort (double s[], int l, int r)
{
    int i,j;
    double x;
    if(l<r)
    {
        i = l;
        j = r;
        x = s[i];
        while(i<j)
        {
            while (i<j&&s[j]>x) j--;
            if (i<j) s[i++] = s[j];
            while (i<j&&s[i]<x) i++;
            if (i < j) s[j--] = s[i];
        }
        s[i] = x;
        r95Sort(s, l, i-1);
        r95Sort(s, i+1, r);
    }

}

void RTLSClient::Slot_RangeLog_Generate(void)
{
	qDebug() << "Slot_RangeLog_connect";
	is_startLog = true;
	time_t now ;
	struct tm *tm_now ;
	char buf[20] = {0};
	time(&now) ;
	tm_now = localtime(&now) ;
	sprintf(buf,"./Logs/Tag %d-%d-%d.log", tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
	file_T = fopen(buf,"w");
}


/*
 *	内容：打印log到控制平台即log日志
 *	形参：
 *	返参：
 *
 */
void RTLSClient::_dbg_printf3(const char *format, ...)
{
    uint32_t length;
    char _dbg_TXBuff[1024];
    va_list args;

    va_start(args, format);
    length = vsnprintf((char*)_dbg_TXBuff, sizeof(_dbg_TXBuff), (char*)format, args);
    va_end(args);

    QDateTime now = QDateTime::currentDateTime();

    qDebug("打印时间:%s,%s",now.toString("yyyy-MM-dd hh:mm:ss").toLatin1().data(), _dbg_TXBuff);

    if(is_startLog)
    {
        QDateTime now = QDateTime::currentDateTime();

        qDebug("打印时间:%s,%s",now.toString("yyyy-MM-dd hh:mm:ss").toLatin1().data(), _dbg_TXBuff);
        fprintf(file_T,"打印时间:%s,%s",now.toString("yyyy-MM-dd hh:mm:ss").toLatin1().data(), _dbg_TXBuff);
    }
}


