// -------------------------------------------------------------------------------------------------------------------
//
//  File: SerialConnection.cpp
//
//  Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//  Author:
//
// -------------------------------------------------------------------------------------------------------------------

#include "SerialConnection.h"

#include <QDebug>
#include <QSerialPortInfo>
#include <QMessageBox>
#include "json_utils.h"
#include "windows.h"

#define INST_VERSION_LEN  (64)
#define MIN_JSON_HEADER   (6)   //JS + 4 chars of length e.g. 'JSxxxx{.....}'


SerialConnection::SerialConnection(QObject *parent) :
    QObject(parent)
{
    _serial = new QSerialPort(this);

    connect(_serial, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleError(QSerialPort::SerialPortError)));

    //periodic timer, to periodically request a new tag's list
    //this timer is started from the RTLS client
    _timer = new QTimer(this);
    connect(_timer, SIGNAL(timeout()), this, SLOT(timerUpdateExpire()));

    _processingData = true;
    qDebug() << "------------SerialConnection 123------------";

}

SerialConnection::~SerialConnection()
{
    if(_serial->isOpen())
        _serial->close();

    delete _serial;
}

QStringList SerialConnection::portsList()
{
    return _ports;
}

void SerialConnection::findSerialDevices()
{
    _portInfo.clear();
    _ports.clear();

    foreach (const QSerialPortInfo &port, QSerialPortInfo::availablePorts())
    //for (QSerialPortInfo port : QSerialPortInfo::availablePorts())
    {
        //Their is some sorting to do for just list the port I want, with vendor Id & product Id
        qDebug() << port.portName() << port.vendorIdentifier() << port.productIdentifier()
                 << port.hasProductIdentifier() << port.hasVendorIdentifier()
                 //<< port.isBusy()
                 << port.manufacturer() << port.description();

        if( (port.description() == DEVICE_STR_UART1) || (port.description() == DEVICE_STR_UART2)|| (port.description()== DEVICE_STR_USB) )
		{
            _portInfo += port;
            _ports += port.portName();
		}
    }

    //connect(_serial, SIGNAL(readyRead()), this, SLOT(readData()));
}

int SerialConnection::openSerialPort(QSerialPortInfo x)
{
    int error = 0;
    _serial->setPort(x);

    if(!_serial->isOpen())
    {
        if (_serial->open(QIODevice::ReadWrite))
        {
            _serial->setBaudRate(QSerialPort::Baud115200/*p.baudRate*/);
            _serial->setDataBits(QSerialPort::Data8/*p.dataBits*/);
            _serial->setParity(QSerialPort::NoParity/*p.parity*/);
            _serial->setStopBits(QSerialPort::OneStop/*p.stopBits*/);
            _serial->setFlowControl(QSerialPort::NoFlowControl /*p.flowControl*/);

            emit statusBarMessage(tr("Connected to %1").arg(x.portName()));

            connect(_serial, SIGNAL(readyRead()), this, SLOT(readData()));


            _serial->clear();

            writeData("deca$\r\n");

			//久凌电子
			//获取打开端口的Name	

            //emit serialOpened(); - wait until we get reply from the unit

        }
        else
        {
            //QMessageBox::critical(NULL, tr("Error"), _serial->errorString());

            emit statusBarMessage(tr("打开失败"));
            //emit statusBarMessage(tr("Open error"));

            qDebug() << "Serial error: " << _serial->error();

            _serial->close();

            emit serialError();

            error = 1;
        }
    }
    else
    {
        qDebug() << "port already open!";

        error = 0;
    }


    if(_serial->isOpen())
    {
        emit connectionStateChanged(Connected);
    }

    return error;
}

int SerialConnection::openConnection(int index)
{
    QSerialPortInfo x;
    int foundit = -1;
    int open = false;

    //open port from list
    x = _portInfo.at(index);

    qDebug() << "port is busy? "
             //<< x.isBusy()
             << "index " << index << " = found " << foundit;

    //if(!open) return -1;

    qDebug() << "open serial port " << index << x.portName();

    //open serial port
    return openSerialPort(x);
}

void SerialConnection::closeConnection(bool error)
{
    _timer->stop();

//    if(!error) //the serial port is closing gracefully (e.g. cable has not been unplugged)
//        writeData("stop\r\n");

    _serial->close();

    while(_serial->isOpen())
    {
        _serial->clear();
    }

    emit statusBarMessage(tr("COM port Disconnected"));
    emit connectionStateChanged(Disconnected);

    _processingData = true;
}

void SerialConnection::writeData(const QByteArray &data)
{
    if(_serial->isOpen())
    {
        _serial->write(data);
		//久凌电子
        //_serial->waitForBytesWritten(1);
        //waitForData = true;
    }
    else
    {
        qDebug() << "not open - can't write?";
    }

    qDebug() << "send:" << data.constData() << _serial->isOpen();
    //emit connectionStateChanged(Connected);
}

void SerialConnection::clear()
{
    _serial->clear();
}

void SerialConnection::cancelConnection()
{
    emit connectionStateChanged(ConnectionFailed);
}

void SerialConnection::readData(void)
{
    if(_processingData)
    {
        QByteArray data = _serial->readAll();
        QByteArray dataChunk;

        int offset = 0;

        _data.append(data);

        int length = _data.length();

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

                    if(ok && (length >= jlength+6))
                    {
                        //get the JSON object including curly brackets from the stream
                        dataChunk = _data.mid(offset+MIN_JSON_HEADER, jlength);

                        qDebug() << "SerialConnection::readData" << jlength << "data" << dataChunk;
                        {//awaiting for version object, no more

                            QString device, version;

                            check_json_version(dataChunk, &device, &version );

                            offset += (jlength+MIN_JSON_HEADER);
                            length -= (jlength+MIN_JSON_HEADER);

                            if(device.contains("Node"))
                            {
                                qDebug() << "Node version: " << version;

                                _connectionConfig = version.mid(0,10);
                                _processingData = false;
                                _gotKlist = false;

                                _serial->clear();

								//久凌电子 处理数据槽函数 newdata()
                                emit serialOpened(_connectionConfig);

                                return;
                            }
                        }
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
        _data = _data.right(length);
        writeData("deca$\r\n");
    }
}


void SerialConnection::timerUpdateStart(int t)
{
    _timer->start(t);
}

//this slot should be periodically called by the _timer e.g. every 30sec
void SerialConnection::timerUpdateExpire(void)
{//send
	//久凌电子
    writeData("getDlist\r\n");   //get the list of discovered tags

    if(!_gotKlist)
        writeData("getKList\r\n");

    _timer->setInterval(20000);
}

void SerialConnection::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        //QMessageBox::critical(this, tr("Critical Error"), serial->errorString());
        closeConnection(true);
    }
}

void SerialConnection::gotKlist(bool gotit)
{
    _gotKlist = gotit;
}


