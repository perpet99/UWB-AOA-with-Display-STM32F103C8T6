#include "serial_widget.h"
#include "ui_serial_widget.h"
#include "RTLSDisplayApplication.h"
#include "SerialConnection.h"
#include <QDebug>


QString serial_Uart_Cache;

serial_widget::serial_widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::serial_widget)
{
    ui->setupUi(this);
}

serial_widget::~serial_widget()
{
    delete ui;
}
/*
 * 打开串口调试助手
 */
void serial_widget::Slot_Open_Serial_assistant()
{
    ui->serial_comPort_UartPort->addItems(RTLSDisplayApplication::serialConnection()->portsList());
    Slot_Serial_Uart_State(RTLSDisplayApplication::serialConnection()->_serial->isOpen());
	show();
}


void serial_widget::Slot_Serial_Uart_State(bool state)
{
    QPalette palette = ui->serial_lineEdit_Uart_State->palette();
    if(state == true)
    {
        palette.setColor(QPalette::Base,Qt::green);

        ui->serial_pushButton_Uart_ReadCfg->setEnabled(true);
        ui->serial_pushButton_Uart_WriteCfg->setEnabled(true);
        ui->serial_pushButton_Uart_Reset_2->setEnabled(true);
        ui->serial_pushButton_Uart_Restore->setEnabled(true);
        ui->serial_pushButton_Uart_Save->setEnabled(true);
        ui->serial_pushButton_Uart_Getver->setEnabled(true);
    }
    else if(state == false)
    {
        palette.setColor(QPalette::Base,Qt::red);

        ui->serial_pushButton_Uart_ReadCfg->setEnabled(false);
        ui->serial_pushButton_Uart_WriteCfg->setEnabled(false);
        ui->serial_pushButton_Uart_Reset_2->setEnabled(false);
        ui->serial_pushButton_Uart_Restore->setEnabled(false);
        ui->serial_pushButton_Uart_Save->setEnabled(false);
        ui->serial_pushButton_Uart_Getver->setEnabled(false);
    }
    ui->serial_lineEdit_Uart_State->setPalette(palette);
}


void serial_widget::Slot_Uart_Recvdata(QByteArray data)
{
    QString str =  tr(data);
    serial_Uart_Cache.append(str);

    int cmd_getver_StartIndex = str.indexOf("getver");
    int cmd_getcfg_StartIndex = str.indexOf("getcfg");
    int cmd_setcfg_StartIndex = str.indexOf("setcfg");

    int cmd_getver_EndIndex = 0;
    int cmd_getcfg_EndIndex = 0;
    int cmd_setcfg_EndIndex = 0;


    //读取版本信息
    if(cmd_getver_StartIndex != -1)
    {
        cmd_getver_EndIndex = str.mid(cmd_getver_StartIndex).indexOf("\r\n");
        if(cmd_getver_StartIndex != -1 && cmd_getver_EndIndex != -1)
        {
            QString tmp = str.mid(cmd_getver_StartIndex, cmd_getver_EndIndex);

            char sotfware[20],hardware[20];
            int n = sscanf(tmp.toLatin1().constData(),"getver software:%16s,hardware:%15s",sotfware,hardware);

            if(n != 2)
                return;

            ui->serial_lineEdit_UartsoftwareVer->setText(sotfware);
            ui->serial_lineEdit_UartHardwareVer->setText(hardware);
        }
    }

    //读取版本信息
    if(cmd_getcfg_StartIndex != -1)
    {
        cmd_getcfg_EndIndex = str.mid(cmd_getcfg_StartIndex).indexOf("\r\n");
        if(cmd_getcfg_StartIndex != -1 && cmd_getcfg_EndIndex != -1)
        {
            QString tmp = str.mid(cmd_getcfg_StartIndex, cmd_getcfg_EndIndex);
            int Dlist, Klist, Net, AncID, Rate, Filter, UserCmd, pdoa_offset, rng_offset;
            //eg getcfg ID:2, Role:0, CH:0, Rate:1
            //eg getcfg ID:255, Role:255, CH:255, Rate:255

            int n = sscanf(tmp.toLatin1().constData(),"getcfg Dlist:%d KList:%d Net:%04X AncID:%d Rate:%d Filter:%d UserCmd:%d pdoaOffset:%d rngOffset:%d",&Dlist,&Klist,&Net,&AncID, &Rate, &Filter, &UserCmd, &pdoa_offset, &rng_offset);

            if(n != 9)
                return;

            ui->serial_lineEdit_Net->setText(QString::number(Net,16).toUpper());
            ui->serial_comboBox_ancid->setCurrentIndex(AncID-1);
            ui->serial_comboBox_filter->setCurrentIndex(Filter);
            ui->serial_comboBox_usercmd->setCurrentIndex(UserCmd);
            ui->serial_lineEdit_pdoaOffset->setText(QString::number(pdoa_offset));
            ui->serial_lineEdit_rngOffset->setText(QString::number(rng_offset));

#if 0
            if((Role == 0 || Role == 1) &&
                (ID >= 0 || ID <= 9) &&
                (CH == 0 || CH == 1) &&
                (Rate == 0 || Rate == 1))
            {
                ui->serial_comboBox_UartRole->setCurrentIndex(Role);
                ui->serial_comboBox_UartID->setCurrentIndex(ID);
                ui->serial_comboBox_UartCH->setCurrentIndex(CH);
                ui->serial_comboBox_UartRate->setCurrentIndex(Rate);
            }
            else if(ID == 0xff && Role == 0xff && CH == 0xff && Rate == 0xff)
            {
                ui->serial_comboBox_UartRole->setCurrentIndex(ui->serial_comboBox_UartRole->count()-1);
                ui->serial_comboBox_UartID->setCurrentIndex(ui->serial_comboBox_UartID->count()-1);
                ui->serial_comboBox_UartCH->setCurrentIndex(ui->serial_comboBox_UartCH->count()-1);
                ui->serial_comboBox_UartRate->setCurrentIndex(ui->serial_comboBox_UartRate->count()-1);
                return;
            }
#endif
        }
    }

    //刷新显示
    ui->serial_textEdit_recv->setText(serial_Uart_Cache);
    ui->serial_textEdit_recv->moveCursor(QTextCursor::End);

    if(serial_Uart_Cache.length() > 9096)
    {
        serial_Uart_Cache.clear();
    }

}


void serial_widget::on_serial_pushButton_Uart_WriteCfg_clicked()
{
	//setcfg 1 1 3333 1 100 0 0
    QString PanID = ui->serial_lineEdit_Net->text().toLatin1();
    int AncID = ui->serial_comboBox_ancid->currentIndex()+1;
    int Serail_Rate = 100;
    int Motion_filter = ui->serial_comboBox_filter->currentIndex();
    int User_Cmd = ui->serial_comboBox_usercmd->currentIndex();;

    QString setcfg = QString("setcfg 1 1 %1 %2 %3 %4 %5\r\n").arg(PanID).arg(AncID).arg(Serail_Rate).arg(Motion_filter).arg(User_Cmd);
    RTLSDisplayApplication::serialConnection()->_serial->write(setcfg.toLocal8Bit());

    qDebug() << "PanID" << ui->serial_lineEdit_Net->text().toLatin1() << "msg" << setcfg;
}

void serial_widget::on_serial_pushButton_Uart_ReadCfg_clicked()
{
    RTLSDisplayApplication::serialConnection()->_serial->write("getcfg\r\n");
}

void serial_widget::on_serial_pushButton_Uart_Save_clicked()
{
    RTLSDisplayApplication::serialConnection()->_serial->write("save\r\n");
}

void serial_widget::on_serial_pushButton_Uart_Reset_2_clicked()
{
    RTLSDisplayApplication::serialConnection()->_serial->write("reset\r\n");
}

void serial_widget::on_serial_pushButton_Uart_Restore_clicked()
{
    RTLSDisplayApplication::serialConnection()->_serial->write("rtoken\r\n");
}

void serial_widget::on_serial_pushButton_Uart_Getver_clicked()
{
    RTLSDisplayApplication::serialConnection()->_serial->write("getver\r\n");
}

