#ifndef SERIAL_WIDGET_H
#define SERIAL_WIDGET_H

#include <QWidget>

namespace Ui {
class serial_widget;
}

class serial_widget : public QWidget
{
    Q_OBJECT

public:
    explicit serial_widget(QWidget *parent = nullptr);
    ~serial_widget();

public slots:
    void Slot_Open_Serial_assistant();
    void Slot_Serial_Uart_State(bool state);
    void Slot_Uart_Recvdata(QByteArray data);

private slots:

    void on_serial_pushButton_Uart_WriteCfg_clicked();

    void on_serial_pushButton_Uart_ReadCfg_clicked();

    void on_serial_pushButton_Uart_Save_clicked();

    void on_serial_pushButton_Uart_Reset_2_clicked();

    void on_serial_pushButton_Uart_Restore_clicked();

    void on_serial_pushButton_Uart_Getver_clicked();

private:
    Ui::serial_widget *ui;
};

#endif // SERIAL_WIDGET_H
