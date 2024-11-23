// SPDX-FileCopyrightText: 2024 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "logoprovider.h"

#include <DSysInfo>

#include <QUrl>

DCORE_USE_NAMESPACE

[[nodiscard]] QString LogoProvider::logo() noexcept
{
    auto url = QUrl::fromUserInput(DSysInfo::distributionOrgLogo(DSysInfo::Distribution,
                                                                 DSysInfo::Transparent,
                                                                 ":/dsg/logo.svg"));
    return url.toString();
}

[[nodiscard]] QString LogoProvider::version() const noexcept
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

void LogoProvider::updateLocale(const QLocale &newLocale) noexcept
{
    if (newLocale == locale) {
        return;
    }

    locale = newLocale;
    Q_EMIT versionChanged();
}
