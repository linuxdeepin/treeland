// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "LogoProvider.h"

#include <DSysInfo>

#include <QUrl>

DCORE_USE_NAMESPACE

[[nodiscard]] QString logoProvider::logo() noexcept
{
    auto url = QUrl::fromUserInput(DSysInfo::distributionOrgLogo(DSysInfo::Distribution,
                                                                 DSysInfo::Transparent,
                                                                 "qrc:/dsg/icons/logo.svg"));
    return url.toString();
}

[[nodiscard]] QString logoProvider::version() const noexcept
{
    if (DSysInfo::uosEditionType() == DSysInfo::UosEdition::UosEducation) {
        return {};
    }

    QString version;
    if (DSysInfo::uosType() == DSysInfo::UosServer) {
        version = QString("%1").arg(DSysInfo::majorVersion());
    } else if (DSysInfo::isDeepin()) {
        version = QString("%1 %2").arg(DSysInfo::majorVersion(), DSysInfo::uosEditionName(locale));
    } else {
        version = QString("%1 %2").arg(DSysInfo::productVersion(), DSysInfo::productTypeString());
    }

    return version;
}

void logoProvider::updateLocale(const QLocale &newLocale) noexcept
{
    if (newLocale == locale) {
        return;
    }

    locale = newLocale;
    emit versionChanged();
}
