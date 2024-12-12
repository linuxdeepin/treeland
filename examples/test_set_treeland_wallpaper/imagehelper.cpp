// Copyright (C) 2024 rewine <luhongxu@deepin.org>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "imagehelper.h"

#include <QQmlInfo>

ImageHelper::ImageHelper(QObject *parent)
    : QObject{ parent }
{
}

bool ImageHelper::isDarkType(const QImage &img)
{
    int r = 0, g = 0, b = 0;
    for (int i = 0; i < img.width(); i++)
        for (int j = 0; j < img.height(); j++) {
            r += qRed(img.pixel(i, j));
            g += qGreen(img.pixel(i, j));
            b += qBlue(img.pixel(i, j));
        }
    auto size = img.width() * img.height();
    float luminance = 0.299 * r / size + 0.587 * g / size + 0.114 * b / size;
    return qRound(luminance) <= 170;
}
