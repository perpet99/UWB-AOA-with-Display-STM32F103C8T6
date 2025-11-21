// -------------------------------------------------------------------------------------------------------------------
//
//  File: SerialConnection.h
//
//  Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//  Author:
//
// -------------------------------------------------------------------------------------------------------------------

#ifndef SERIALCONNECTION_H
#define SERIALCONNECTION_H

#include <QObject>
#include <QtSerialPort/QSerialPort>
#include <QStringList>
#include <QTimer>

#define DEVICE_STR_USB ("STMicroelectronics Virtual COM Port")
#define DEVICE_STR_UART1 ("USB-SERIAL CH340")
#define DEVICE_STR_UART2 ("Silicon Labs CP210x USB to UART Bridge")

/**
* @brief SerialConnection
*        Constructor, it initialises the Serial Connection its parts
*        it is used for managing the COM port connection.
*/
class SerialConnection : public QObject
{
    Q_OBJECT
public:
    explicit SerialConnection(QObject *parent = 0);
    ~SerialConnection();

    enum ConnectionState
    {
        Disconnected = 0,
        Connecting,
        Connected,
        ConnectionFailed
    };

    QSerialPort *_serial;
    

    void findSerialDevices(); //find any tags or PDOA nodes that are connected to the PC

    int openSerialPort(QSerialPortInfo x); //open selected serial port

    QStringList portsList(); //return list of available serial ports (list of ports with tag/PDOA node connected)

    QSerialPort* serialPort() { return _serial; }

    void timerUpdateStart(int);

signals:
    void serialError(void);
    void getCfg(void);
    void nextCmd(void);

    void statusBarMessage(QString status);
    void connectionStateChanged(SerialConnection::ConnectionState);
    void serialOpened(QString);

public slots:
    void closeConnection(bool);
    void cancelConnection();
    int  openConnection(int index);
    void readData(void);
    void timerUpdateExpire(void);
    void writeData(const QByteArray &data);
    void clear(void);

    void gotKlist(bool gotit);

protected slots:

    void handleError(QSerialPort::SerialPortError error);

private:
    QList<QSerialPortInfo>    _portInfo ;
    QStringList _ports;

    QList<QByteArray> _cmdList ;

    QString _connectionVersion;
    QString _connectionConfig;
    bool _processingData;
    bool _gotKlist;

    QTimer *_timer;
    QByteArray _data;
};

#endif // SERIALCONNECTION_H
