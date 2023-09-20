/***************************************************************************
* Copyright (c) 2015-2018 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
* Copyright (c) 2013 Abdurrahman AVCI <abdurrahmanavci@gmail.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the
* Free Software Foundation, Inc.,
* 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
***************************************************************************/

#include "GreeterApp.h"
#include "Configuration.h"
#include "GreeterProxy.h"
#include "Constants.h"
#include "ScreenModel.h"
#include "SessionModel.h"
#include "SignalHandler.h"
#include "ThemeConfig.h"
#include "ThemeMetadata.h"
#include "UserModel.h"
#include "KeyboardModel.h"

#include "MessageHandler.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickView>
#include <QQmlContext>
#include <QQmlEngine>
#include <QDebug>
#include <QTimer>
#include <QTranslator>
#include <QLibraryInfo>
#include <QVersionNumber>
#include <QSurfaceFormat>

#include <iostream>

#define TR(x) QT_TRANSLATE_NOOP("Command line parser", QStringLiteral(x))

static const QEvent::Type StartupEventType = static_cast<QEvent::Type>(QEvent::registerEventType());
