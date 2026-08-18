// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "debughelpers.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>

QString stateName(int state)
{
    switch (state) {
    case 0: return QStringLiteral("Normal");
    case 1: return QStringLiteral("Maximized");
    case 2: return QStringLiteral("Minimized");
    case 3: return QStringLiteral("Fullscreen");
    case 4: return QStringLiteral("Tiling");
    default: return QStringLiteral("Unknown(%1)").arg(state);
    }
}

int buttonCode(const QString &name, bool *ok)
{
    *ok = true;
    const QString lower = name.toLower();
    if (lower == QLatin1String("left"))
        return 0x110; // BTN_LEFT
    if (lower == QLatin1String("right"))
        return 0x111; // BTN_RIGHT
    if (lower == QLatin1String("middle"))
        return 0x112; // BTN_MIDDLE
    bool parsed = false;
    const int code = name.toInt(&parsed);
    if (parsed)
        return code;
    *ok = false;
    return 0;
}

int keyCode(const QString &name, bool *ok)
{
    *ok = true;
    bool parsed = false;
    const int code = name.toInt(&parsed);
    if (parsed)
        return code;

    static const QHash<QString, int> table = {
        {QStringLiteral("esc"), 1},     {QStringLiteral("1"), 2},
        {QStringLiteral("2"), 3},       {QStringLiteral("3"), 4},
        {QStringLiteral("4"), 5},       {QStringLiteral("5"), 6},
        {QStringLiteral("6"), 7},       {QStringLiteral("7"), 8},
        {QStringLiteral("8"), 9},       {QStringLiteral("9"), 10},
        {QStringLiteral("0"), 11},      {QStringLiteral("minus"), 12},
        {QStringLiteral("equal"), 13},  {QStringLiteral("backspace"), 14},
        {QStringLiteral("tab"), 15},    {QStringLiteral("q"), 16},
        {QStringLiteral("w"), 17},      {QStringLiteral("e"), 18},
        {QStringLiteral("r"), 19},      {QStringLiteral("t"), 20},
        {QStringLiteral("y"), 21},      {QStringLiteral("u"), 22},
        {QStringLiteral("i"), 23},      {QStringLiteral("o"), 24},
        {QStringLiteral("p"), 25},      {QStringLiteral("enter"), 28},
        {QStringLiteral("leftctrl"), 29}, {QStringLiteral("a"), 30},
        {QStringLiteral("s"), 31},      {QStringLiteral("d"), 32},
        {QStringLiteral("f"), 33},      {QStringLiteral("g"), 34},
        {QStringLiteral("h"), 35},      {QStringLiteral("j"), 36},
        {QStringLiteral("k"), 37},      {QStringLiteral("l"), 38},
        {QStringLiteral("space"), 57},  {QStringLiteral("leftshift"), 42},
        {QStringLiteral("leftalt"), 56},{QStringLiteral("z"), 44},
        {QStringLiteral("x"), 45},      {QStringLiteral("c"), 46},
        {QStringLiteral("v"), 47},      {QStringLiteral("b"), 48},
        {QStringLiteral("n"), 49},      {QStringLiteral("m"), 50},
        {QStringLiteral("left"), 105},  {QStringLiteral("right"), 106},
        {QStringLiteral("up"), 103},    {QStringLiteral("down"), 108},
        {QStringLiteral("f1"), 59},     {QStringLiteral("f2"), 60},
        {QStringLiteral("f3"), 61},     {QStringLiteral("f4"), 62},
        {QStringLiteral("f5"), 63},     {QStringLiteral("f6"), 64},
        {QStringLiteral("f7"), 65},     {QStringLiteral("f8"), 66},
        {QStringLiteral("f9"), 67},     {QStringLiteral("f10"), 68},
        {QStringLiteral("f11"), 87},    {QStringLiteral("f12"), 88},
        {QStringLiteral("del"), 111},   {QStringLiteral("insert"), 110},
        {QStringLiteral("home"), 102},  {QStringLiteral("end"), 107},
        {QStringLiteral("pageup"), 104},{QStringLiteral("pagedown"), 109},
    };
    const auto it = table.constFind(name.toLower());
    if (it != table.constEnd())
        return it.value();
    *ok = false;
    return 0;
}

QString saveCapture(const QByteArray &data, const QString &userPath)
{
    if (data.isEmpty())
        return {};
    QString path = userPath;
    if (path.isEmpty())
        path = QStringLiteral("/tmp/treeland-debug-%1.png").arg(QDateTime::currentMSecsSinceEpoch());
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".png");
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly))
        return {};
    if (f.write(data) != data.size())
        return {};
    f.close();
    return path;
}

QJsonObject pointToJson(const QPointF &point)
{
    return {
        {"x", point.x()},
        {"y", point.y()},
    };
}

QJsonObject rectToJson(const QRectF &rect)
{
    return {
        {"x", rect.x()},
        {"y", rect.y()},
        {"width", rect.width()},
        {"height", rect.height()},
    };
}
