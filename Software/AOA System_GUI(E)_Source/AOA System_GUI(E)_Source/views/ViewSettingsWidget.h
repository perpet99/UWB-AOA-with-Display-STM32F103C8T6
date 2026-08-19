// -------------------------------------------------------------------------------------------------------------------
//
//  File: ViewSettingsWidget.h
//
//  Copyright 2015 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//  Author:
//
// -------------------------------------------------------------------------------------------------------------------

#ifndef VIEWSETTINGSWIDGET_H
#define VIEWSETTINGSWIDGET_H

#include <QWidget>

#include <QtGui>
#include <QComboBox>
#include <QListView>
#include <QTableView>
#include <QVBoxLayout>

namespace Ui {
class ViewSettingsWidget;
}

class ViewSettingsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ViewSettingsWidget(QWidget *parent = 0);
    ~ViewSettingsWidget();

    int applyFloorPlanPic(const QString &path);

signals:
    void saveViewSettings(void);
    void toolDone(void);
    void updateScene(void);
    void GetList(void);
    void save(void);


    void Signal_Open_Serial_assistant();
    void Signal_RangeLog_Generate(void);
public slots:
    void calibrationBar(int currentVal);

protected slots:
    void ViewGetList(void);
    void Viewsave(void);
    void onReady();
    void onConnected(QString conf);

    void floorplanOpenClicked();
    void originClicked();
    void scaleClicked();

    void gridShowClicked();
    void originShowClicked();
    void tagHistoryShowClicked();

    void saveFPClicked();
    void motionFilterClicked();
    void doCalibrationClicked();
    void tagHistoryNumberValueChanged(int);

    void startGeoFencingClicked();
    void gfCentreClicked();
    void applyClicked();
    void setGFCentreY(double);
    void setGFCentreX(double);
    void setGFCentreXY(double, double);
    void cxValueChanged(double);
    void cyValueChanged(double);
    void hValueChanged(double);
    void wValueChanged(double);
    void tabSelected();

    void showOriginGrid(bool orig, bool grid);
    void getFloorPlanPic(void);
    void showSave(bool);

    void setTagHistory(int h);

    void writePhaseOffset(double updatedPhaseOffset);
    void writeRangeOffset(double updatedRangeOffset);

private slots:
    void on_pushButton_3_clicked();

    void on_startLog_clicked();

private:
    Ui::ViewSettingsWidget *ui;

    bool _floorplanOpen;
    bool _geofencing;
    bool _applied;
    double _new_x;
    double _new_y;
};

#endif // VIEWSETTINGSWIDGET_H
