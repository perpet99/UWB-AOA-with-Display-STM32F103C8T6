// -------------------------------------------------------------------------------------------------------------------
//
//  File: json_utils.h
//
//  Copyright 2016 (c) Decawave Ltd, Dublin, Ireland.
//
//  All rights reserved.
//
//
// -------------------------------------------------------------------------------------------------------------------
//

#ifndef __JSON_UTILS_H__
#define __JSON_UTILS_H__

#include "stdio.h"

#include <QDebug>
#include <QString>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

int  check_json_stream(const QByteArray pd);
void check_json_version(const QByteArray st, QString *device, QString *version);

#endif
