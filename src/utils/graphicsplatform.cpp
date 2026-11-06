// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "graphicsplatform.h"

#include <rhi/qrhi.h>

#include <QRegularExpression>

namespace {

using ChipClass = GraphicsPlatform::ChipClass;
using Driver = GraphicsPlatform::Driver;

// PCI vendor IDs. Reliable on Vulkan, 0 when the backend does not report them.
constexpr quint64 PciVendorAmd = 0x1002;
constexpr quint64 PciVendorNvidia = 0x10de;
constexpr quint64 PciVendorIntel = 0x8086;
constexpr quint64 PciVendorQualcomm = 0x5143;
constexpr quint64 PciVendorArm = 0x13b5;
constexpr quint64 PciVendorBroadcom = 0x14e4;
constexpr quint64 PciVendorVmware = 0x15ad;
constexpr quint64 PciVendorVirtualBox = 0x80ee;
constexpr quint64 PciVendorVirtio = 0x1af4;

bool containsAny(const QString &text, std::initializer_list<const char *> needles)
{
    for (const char *needle : needles) {
        if (text.contains(QLatin1StringView(needle), Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// Returns the portion of @p text matching @p pattern, or a null string.
QString extract(const QString &text, const QString &pattern)
{
    const QRegularExpressionMatch match = QRegularExpression(pattern).match(text);
    return match.hasMatch() ? match.captured() : QString();
}

// Strips a trailing non-digit left over from a "(M|\\b)" style pattern.
int trailingNumber(const QString &text, int digits)
{
    QString name = text.trimmed();
    while (!name.isEmpty() && !name.back().isDigit()) {
        name.chop(1);
    }
    return name.right(digits).toInt();
}

ChipClass detectRadeonClass(const QString &chipset)
{
    if (chipset.isEmpty()) {
        return ChipClass::Unknown;
    }

    if (containsAny(chipset, {"R100", "RV100", "RS100"})) {
        return ChipClass::R100;
    }

    if (containsAny(chipset, {"RV200", "RS200", "R200", "RV250", "RS300", "RV280"})) {
        return ChipClass::R200;
    }

    if (containsAny(chipset, {"R300", "R350", "R360", "RV350", "RV370", "RV380"})) {
        return ChipClass::R300;
    }

    if (containsAny(chipset, {"R420", "R423", "R430", "R480", "R481", "RV410", "RS400",
                              "RC410", "RS480", "RS482", "RS600", "RS690", "RS740"})) {
        return ChipClass::R400;
    }

    if (containsAny(chipset, {"RV515", "R520", "RV530", "R580", "RV560", "RV570"})) {
        return ChipClass::R500;
    }

    if (containsAny(chipset, {"R600", "RV610", "RV630", "RV670", "RV620", "RV635", "RS780",
                              "RS880"})) {
        return ChipClass::R600;
    }

    if (containsAny(chipset, {"R700", "RV770", "RV730", "RV710", "RV740"})) {
        return ChipClass::R700;
    }

    if (containsAny(chipset, {"EVERGREEN", "CEDAR", "REDWOOD", "JUNIPER", "CYPRESS", "HEMLOCK",
                              "PALM"})) {
        return ChipClass::Evergreen;
    }

    if (containsAny(chipset, {"SUMO", "SUMO2", "BARTS", "TURKS", "CAICOS", "CAYMAN"})) {
        return ChipClass::NorthernIslands;
    }

    if (containsAny(chipset, {"TAHITI", "PITCAIRN", "VERDE", "OLAND", "HAINAN"})) {
        return ChipClass::SouthernIslands;
    }

    if (containsAny(chipset, {"BONAIRE", "KAVERI", "KABINI", "HAWAII", "MULLINS"})) {
        return ChipClass::SeaIslands;
    }

    if (containsAny(chipset, {"TONGA", "TOPAZ", "FIJI", "CARRIZO", "STONEY"})) {
        return ChipClass::VolcanicIslands;
    }

    if (containsAny(chipset, {"POLARIS10", "POLARIS11", "POLARIS12", "POLARIS 1", "VEGAM"})) {
        return ChipClass::ArcticIslands;
    }

    if (containsAny(chipset, {"VEGA10", "VEGA12", "VEGA20", "VEGA 1", "VEGA 2", "RAVEN",
                              "RAVEN2", "RENOIR", "ARCTURUS"})) {
        return ChipClass::Vega;
    }

    if (containsAny(chipset, {"NAVI10", "NAVI12", "NAVI14", "NAVI21", "NAVI22", "NAVI23",
                              "NAVI24", "NAVI31", "NAVI32", "NAVI33", "NAVI 1", "NAVI 2",
                              "NAVI 3", "REMBRANDT", "PHOENIX", "CEZANNE", "BARCELO"})) {
        return ChipClass::Navi;
    }

    // Marketing names of older cards: Radeon HD 5xxx/6xxx, X1xxx, 9xxx, ...
    QString name = extract(chipset, QStringLiteral("HD [0-9]{4}"));
    if (!name.isEmpty()) {
        const int id = trailingNumber(name, 4);
        if (id == 6250 || id == 6310) { // Palm
            return ChipClass::Evergreen;
        }
        if (id >= 6000 && id < 7000) {
            return ChipClass::NorthernIslands;
        }
        if (id >= 5000 && id < 6000) {
            return ChipClass::Evergreen;
        }
        if (id >= 4000 && id < 5000) {
            return ChipClass::R700;
        }
        if (id >= 2000 && id < 4000) {
            return ChipClass::R600;
        }
        return ChipClass::Unknown;
    }

    name = extract(chipset, QStringLiteral("\\bX[0-9]{3,4}\\b"));
    if (!name.isEmpty()) {
        const int id = QStringView(name).mid(1).toInt();
        if (id >= 1300) {
            return ChipClass::R500;
        }
        if ((id >= 700 && id < 1000) || id >= 1200) {
            return ChipClass::R400;
        }
        if ((id >= 300 && id < 700) || (id >= 1000 && id < 1200)) {
            return ChipClass::R300;
        }
        return ChipClass::Unknown;
    }

    name = extract(chipset, QStringLiteral("\\b[0-9]{4}\\b"));
    if (!name.isEmpty()) {
        const int id = name.toInt();
        if (id >= 7000 && id < 8000) {
            return ChipClass::R100;
        }
        if (id >= 8000 && id < 9500) {
            return ChipClass::R200;
        }
        if (id >= 9500) {
            return ChipClass::R300;
        }
        if (id == 2100) {
            return ChipClass::R400;
        }
    }

    return ChipClass::Unknown;
}

// RTX/GTX/GT model numbers name the generation the chip class enum models.
// Returns ChipClass::Unknown when the string carries no such name.
ChipClass detectModernNVidiaClass(const QString &chipset)
{
    // Quadro RTX model numbers are tiers, not generations; they are all Turing.
    if (chipset.contains(QLatin1StringView("Quadro"), Qt::CaseInsensitive)) {
        return chipset.contains(QLatin1StringView("Quadro RTX"), Qt::CaseInsensitive)
            ? ChipClass::Turing
            : ChipClass::Unknown;
    }

    static const QRegularExpression fourDigit(QStringLiteral("(?:RTX|GTX|GT) ([0-9]{4})"));
    static const QRegularExpression threeDigit(QStringLiteral("(?:RTX|GTX|GT) ([0-9]{3})\\b"));

    QRegularExpressionMatch match = fourDigit.match(chipset);
    if (match.hasMatch()) {
        switch (match.captured(1).left(2).toInt()) {
        case 10: // GTX 10xx
            return ChipClass::Pascal;
        case 16: // GTX 16xx
        case 20: // RTX 20xx
            return ChipClass::Turing;
        case 30: // RTX 30xx
            return ChipClass::Ampere;
        case 40: // RTX 40xx
            return ChipClass::Ada;
        case 50: // RTX 50xx
            return ChipClass::Blackwell;
        default:
            return ChipClass::Unknown;
        }
    }

    match = threeDigit.match(chipset);
    if (match.hasMatch()) {
        // Desktop GTX 750 and GTX 750 Ti use Maxwell despite the 7xx name.
        if (match.captured().startsWith(QLatin1StringView("GTX "))
            && match.captured(1).toInt() == 750) {
            return ChipClass::Maxwell;
        }
        switch (match.captured(1).left(1).toInt()) {
        case 4: // GT/GTX 4xx/5xx, Fermi
        case 5:
            return ChipClass::GF100;
        case 6: // GTX 6xx/7xx, Kepler
        case 7:
            return ChipClass::Kepler;
        case 9: // GTX 9xx, Maxwell
            return ChipClass::Maxwell;
        default:
            return ChipClass::Unknown;
        }
    }

    return ChipClass::Unknown;
}

ChipClass detectNVidiaClass(const QString &chipset)
{
    QString name = extract(chipset, QStringLiteral("\\bNV[0-9,A-F]{2}\\b"));
    if (!name.isEmpty()) {
        const int id = QStringView(name).mid(2).toInt(nullptr, 16);
        switch (id & 0xf0) {
        case 0x00:
        case 0x10:
            return ChipClass::NV10;
        case 0x20:
            return ChipClass::NV20;
        case 0x30:
            return ChipClass::NV30;
        case 0x40:
        case 0x60:
            return ChipClass::NV40;
        case 0x50:
        case 0x80:
        case 0x90:
        case 0xA0:
            return ChipClass::G80;
        default:
            return ChipClass::Unknown;
        }
    }

    if (containsAny(chipset, {"GeForce2", "GeForce 256"})) {
        return ChipClass::NV10;
    }

    if (chipset.contains(QLatin1StringView("GeForce3"))) {
        return ChipClass::NV20;
    }

    if (chipset.contains(QLatin1StringView("GeForce4"))) {
        if (containsAny(chipset, {"MX 420", "MX 440", "MX 460", "MX 4000", "PCX 4300"})) {
            return ChipClass::NV10;
        }
        return ChipClass::NV20;
    }

    const ChipClass modern = detectModernNVidiaClass(chipset);
    if (modern != ChipClass::Unknown) {
        return modern;
    }

    // GeForce 5, 6, 7, 8, 9
    name = extract(chipset, QStringLiteral("GeForce (FX |PCX |Go )?\\d{4}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        const int id = trailingNumber(name, 4);
        if (id < 6000) {
            return ChipClass::NV30;
        }
        if (id < 8000) {
            return ChipClass::NV40;
        }
        return ChipClass::G80;
    }

    // GeForce 100/200/300/400/500
    name = extract(chipset, QStringLiteral("GeForce (G |GT |GTX |GTS )?\\d{3}(M|\\b)")).trimmed();
    if (!name.isEmpty()) {
        const int id = trailingNumber(name, 3);
        if (id >= 100) {
            // 100/200/300 are G80 class, 400 and later are Fermi (GF100).
            return id >= 400 ? ChipClass::GF100 : ChipClass::G80;
        }
        return ChipClass::Unknown;
    }

    return ChipClass::Unknown;
}

ChipClass detectIntelClass(const QString &chipset)
{
    if (containsAny(chipset, {"845G", "830M", "852GM/855GM", "865G"})) {
        return ChipClass::I8XX;
    }

    if (containsAny(chipset, {"915G", "E7221G", "915GM", "945G", "945GM", "945GME", "Q33",
                              "Q35", "G33", "965Q", "946GZ", "IGD"})) {
        return ChipClass::I915;
    }

    if (containsAny(chipset, {"965G", "G45/G43", "965GM", "965GME/GLE", "GM45", "Q45/Q43",
                              "G41", "B43", "Ironlake"})) {
        return ChipClass::I965;
    }

    if (containsAny(chipset, {"Sandybridge", "Sandy Bridge", "SNB GT"})) {
        return ChipClass::SandyBridge;
    }
    if (containsAny(chipset, {"Ivybridge", "Ivy Bridge", "IVB GT"})) {
        return ChipClass::IvyBridge;
    }
    if (containsAny(chipset, {"Haswell", "HSW GT"})) {
        return ChipClass::Haswell;
    }
    if (chipset.contains(QLatin1StringView("BYT"))) {
        return ChipClass::BayTrail;
    }
    if (containsAny(chipset, {"CHV", "BSW"})) {
        return ChipClass::Cherryview;
    }
    if (containsAny(chipset, {"BDW GT", "Broadwell"})) {
        return ChipClass::Broadwell;
    }
    if (containsAny(chipset, {"SKL GT", "Skylake"})) {
        return ChipClass::Skylake;
    }
    if (containsAny(chipset, {"APL", "Apollo Lake"})) {
        return ChipClass::ApolloLake;
    }
    if (containsAny(chipset, {"GLK GT", "Gemini Lake"})) {
        return ChipClass::GeminiLake;
    }
    if (containsAny(chipset, {"KBL GT", "Kaby Lake"})) {
        return ChipClass::KabyLake;
    }
    if (containsAny(chipset, {"WHL GT", "Whiskey Lake"})) {
        return ChipClass::WhiskeyLake;
    }
    if (containsAny(chipset, {"CML GT", "Comet Lake"})) {
        return ChipClass::CometLake;
    }
    if (containsAny(chipset, {"CNL GT", "Cannon Lake"})) {
        return ChipClass::CannonLake;
    }
    if (containsAny(chipset, {"CFL GT", "Coffee Lake"})) {
        return ChipClass::CoffeeLake;
    }
    if (containsAny(chipset, {"ICL GT", "Ice Lake"})) {
        return ChipClass::IceLake;
    }
    if (containsAny(chipset, {"TGL GT", "Tiger Lake"})) {
        return ChipClass::TigerLake;
    }

    return ChipClass::Unknown;
}

ChipClass detectAdrenoClass(const QString &chipset)
{
    const QRegularExpressionMatch match =
        QRegularExpression(QStringLiteral("Adreno[^0-9]*([0-9]{3})")).match(chipset);
    if (!match.hasMatch()) {
        return ChipClass::Unknown;
    }

    switch (match.captured(1).toInt() / 100) {
    case 1:
        return ChipClass::Adreno1XX;
    case 2:
        return ChipClass::Adreno2XX;
    case 3:
        return ChipClass::Adreno3XX;
    case 4:
        return ChipClass::Adreno4XX;
    case 5:
        return ChipClass::Adreno5XX;
    default:
        return ChipClass::Unknown;
    }
}

ChipClass detectPanfrostClass(const QString &chipset)
{
    // Keep in sync with https://docs.mesa3d.org/drivers/panfrost.html
    if (containsAny(chipset, {"T720", "T760"})) {
        return ChipClass::MaliT7XX;
    }

    if (containsAny(chipset, {"T820", "T830", "T860", "T880"})) {
        return ChipClass::MaliT8XX;
    }

    if (containsAny(chipset, {"G31", "G51", "G52", "G57", "G72", "G76", "G78", "G310", "G510",
                              "G610", "G710"})) {
        return ChipClass::MaliGXX;
    }

    return ChipClass::Unknown;
}

ChipClass detectLimaClass(const QString &chipset)
{
    if (chipset.contains(QLatin1StringView("400"))) {
        return ChipClass::Mali400;
    }
    if (chipset.contains(QLatin1StringView("450"))) {
        return ChipClass::Mali450;
    }
    if (chipset.contains(QLatin1StringView("470"))) {
        return ChipClass::Mali470;
    }
    return ChipClass::Unknown;
}

// Panfrost and Lima share the PCI vendor ID and both report "Mali" device
// names; only panvk (Vulkan) and lima (OpenGL) tell them apart in practice.
// shortcut: a Mali GPU using panfrost's GL driver is reported as Lima, upgrade
// once Mesa exposes the kernel driver name through QRhi.
Driver detectMaliDriver(const QString &reported, bool vulkan)
{
    if (reported.contains(QLatin1StringView("panfrost"), Qt::CaseInsensitive)) {
        return Driver::Panfrost;
    }
    if (reported.contains(QLatin1StringView("mali"), Qt::CaseInsensitive)) {
        return vulkan ? Driver::Panfrost : Driver::Lima;
    }
    return Driver::Panfrost;
}

Driver detectDriver(quint64 vendorId, const QString &reported, bool vulkan)
{
    switch (vendorId) {
    case PciVendorAmd:
        return Driver::Radeon;
    case PciVendorNvidia:
        return Driver::NVidia;
    case PciVendorIntel:
        return Driver::Intel;
    case PciVendorQualcomm:
        return Driver::Qualcomm;
    case PciVendorArm:
        return detectMaliDriver(reported, vulkan);
    case PciVendorBroadcom:
        return reported.contains(QLatin1StringView("V3D"), Qt::CaseInsensitive)
            ? Driver::VideoCore3D
            : Driver::VideoCore4;
    case PciVendorVmware:
        return Driver::VMware;
    case PciVendorVirtualBox:
        return Driver::VirtualBox;
    case PciVendorVirtio:
        return Driver::Virgl;
    default:
        break;
    }

    // OpenGL does not report PCI IDs, so fall back to the reported string.
    if (containsAny(reported, {"llvmpipe", "softpipe", "swrast", "software rasterizer",
                               "lavapipe", "swiftshader"})) {
        return Driver::Software;
    }
    if (containsAny(reported, {"virtualbox", "humper", "chromium"})) {
        return Driver::VirtualBox;
    }
    if (containsAny(reported, {"vmware", "svga"})) {
        return Driver::VMware;
    }
    if (containsAny(reported, {"virgl", "virtio"})) {
        return Driver::Virgl;
    }
    if (containsAny(reported, {"nvidia", "nouveau", "geforce"})) {
        return Driver::NVidia;
    }
    if (containsAny(reported, {"radeon", "amd", "advanced micro devices"})) {
        return Driver::Radeon;
    }
    if (reported.contains(QLatin1StringView("intel"), Qt::CaseInsensitive)) {
        return Driver::Intel;
    }
    if (containsAny(reported, {"adreno", "qualcomm"})) {
        return Driver::Qualcomm;
    }
    if (containsAny(reported, {"panfrost", "mali"})) {
        return detectMaliDriver(reported, vulkan);
    }
    if (reported.contains(QLatin1StringView("videocore iv"), Qt::CaseInsensitive)
        || reported.contains(QLatin1StringView("vc4"), Qt::CaseInsensitive)) {
        return Driver::VideoCore4;
    }
    if (reported.contains(QLatin1StringView("videocore 3d"), Qt::CaseInsensitive)
        || reported.contains(QLatin1StringView("v3d"), Qt::CaseInsensitive)) {
        return Driver::VideoCore3D;
    }

    return Driver::Unknown;
}

ChipClass detectChipClass(Driver driver, const QString &chipset)
{
    switch (driver) {
    case Driver::Radeon:
        return detectRadeonClass(chipset);
    case Driver::NVidia:
        return detectNVidiaClass(chipset);
    case Driver::Intel:
        return detectIntelClass(chipset);
    case Driver::Qualcomm:
        return detectAdrenoClass(chipset);
    case Driver::Panfrost:
        return detectPanfrostClass(chipset);
    case Driver::Lima:
        return detectLimaClass(chipset);
    case Driver::VideoCore4:
        return chipset.contains(QLatin1StringView("2.1"))
            ? ChipClass::VC4_2_1
            : ChipClass::Unknown;
    case Driver::VideoCore3D:
        return chipset.contains(QLatin1StringView("4.2"))
            ? ChipClass::V3D_4_2
            : ChipClass::Unknown;
    default:
        return ChipClass::Unknown;
    }
}

QString deviceTypeName(QRhiDriverInfo::DeviceType deviceType)
{
    switch (deviceType) {
    case QRhiDriverInfo::IntegratedDevice:
        return QStringLiteral("integrated");
    case QRhiDriverInfo::DiscreteDevice:
        return QStringLiteral("discrete");
    case QRhiDriverInfo::ExternalDevice:
        return QStringLiteral("external");
    case QRhiDriverInfo::VirtualDevice:
        return QStringLiteral("virtual");
    case QRhiDriverInfo::CpuDevice:
        return QStringLiteral("cpu");
    case QRhiDriverInfo::UnknownDevice:
    default:
        return QStringLiteral("unknown");
    }
}

} // namespace

GraphicsPlatform::GraphicsPlatform(const QRhi *rhi)
{
    if (!rhi) {
        return;
    }

    const QRhiDriverInfo info = rhi->driverInfo();
    m_deviceName = QString::fromUtf8(info.deviceName).trimmed();
    m_deviceType = deviceTypeName(info.deviceType);
    m_driver = detectDriver(info.vendorId, m_deviceName, rhi->backend() == QRhi::Vulkan);
    m_chipClass = detectChipClass(m_driver, m_deviceName);
    if (info.deviceType == QRhiDriverInfo::CpuDevice) {
        m_driver = Driver::Software;
        m_chipClass = ChipClass::Unknown;
    }
    m_virtualMachine = info.deviceType == QRhiDriverInfo::VirtualDevice
        || m_driver == Driver::Virgl || m_driver == Driver::VMware
        || m_driver == Driver::VirtualBox;
}

bool GraphicsPlatform::isRadeon() const
{
    return m_driver == Driver::Radeon;
}

bool GraphicsPlatform::isNvidia() const
{
    return m_driver == Driver::NVidia;
}

bool GraphicsPlatform::isIntel() const
{
    return m_driver == Driver::Intel;
}

bool GraphicsPlatform::isAdreno() const
{
    return m_driver == Driver::Qualcomm;
}

bool GraphicsPlatform::isPanfrost() const
{
    return m_driver == Driver::Panfrost;
}

bool GraphicsPlatform::isLima() const
{
    return m_driver == Driver::Lima;
}

bool GraphicsPlatform::isVideoCore4() const
{
    return m_driver == Driver::VideoCore4;
}

bool GraphicsPlatform::isVideoCore3D() const
{
    return m_driver == Driver::VideoCore3D;
}

bool GraphicsPlatform::isSoftware() const
{
    return m_driver == Driver::Software;
}

bool GraphicsPlatform::isVirtualMachine() const
{
    return m_virtualMachine;
}

QString GraphicsPlatform::toString() const
{
    QString summary = QStringLiteral("%1, %2 (%3), chip class: %4")
                          .arg(driverToString(m_driver),
                               m_deviceName.isEmpty() ? QStringLiteral("unknown device")
                                                      : m_deviceName,
                               m_deviceType,
                               chipClassToString(m_chipClass));
    if (isVirtualMachine()) {
        summary += QStringLiteral(", virtual machine");
    }
    return summary;
}

QString GraphicsPlatform::driverToString(Driver driver)
{
    switch (driver) {
    case Driver::Radeon:
        return QStringLiteral("Radeon");
    case Driver::NVidia:
        return QStringLiteral("NVIDIA");
    case Driver::Intel:
        return QStringLiteral("Intel");
    case Driver::Qualcomm:
        return QStringLiteral("Qualcomm");
    case Driver::Panfrost:
        return QStringLiteral("Panfrost");
    case Driver::Lima:
        return QStringLiteral("Mali (Lima)");
    case Driver::VideoCore4:
        return QStringLiteral("VideoCore IV");
    case Driver::VideoCore3D:
        return QStringLiteral("VideoCore 3D");
    case Driver::Virgl:
        return QStringLiteral("Virgl (virtio-gpu)");
    case Driver::VMware:
        return QStringLiteral("VMware (SVGA3D)");
    case Driver::VirtualBox:
        return QStringLiteral("VirtualBox (Chromium)");
    case Driver::Software:
        return QStringLiteral("Software rasterizer");
    default:
        return QStringLiteral("Unknown");
    }
}

QString GraphicsPlatform::chipClassToString(ChipClass chipClass)
{
    switch (chipClass) {
    case ChipClass::R100:
        return QStringLiteral("R100");
    case ChipClass::R200:
        return QStringLiteral("R200");
    case ChipClass::R300:
        return QStringLiteral("R300");
    case ChipClass::R400:
        return QStringLiteral("R400");
    case ChipClass::R500:
        return QStringLiteral("R500");
    case ChipClass::R600:
        return QStringLiteral("R600");
    case ChipClass::R700:
        return QStringLiteral("R700");
    case ChipClass::Evergreen:
        return QStringLiteral("Evergreen");
    case ChipClass::NorthernIslands:
        return QStringLiteral("Northern Islands");
    case ChipClass::SouthernIslands:
        return QStringLiteral("Southern Islands");
    case ChipClass::SeaIslands:
        return QStringLiteral("Sea Islands");
    case ChipClass::VolcanicIslands:
        return QStringLiteral("Volcanic Islands");
    case ChipClass::ArcticIslands:
        return QStringLiteral("Arctic Islands");
    case ChipClass::Vega:
        return QStringLiteral("Vega");
    case ChipClass::Navi:
        return QStringLiteral("Navi");

    case ChipClass::NV10:
        return QStringLiteral("NV10");
    case ChipClass::NV20:
        return QStringLiteral("NV20");
    case ChipClass::NV30:
        return QStringLiteral("NV30");
    case ChipClass::NV40:
        return QStringLiteral("NV40/G70");
    case ChipClass::G80:
        return QStringLiteral("G80/G90");
    case ChipClass::GF100:
        return QStringLiteral("GF100");
    case ChipClass::Kepler:
        return QStringLiteral("Kepler");
    case ChipClass::Maxwell:
        return QStringLiteral("Maxwell");
    case ChipClass::Pascal:
        return QStringLiteral("Pascal");
    case ChipClass::Turing:
        return QStringLiteral("Turing");
    case ChipClass::Ampere:
        return QStringLiteral("Ampere");
    case ChipClass::Ada:
        return QStringLiteral("Ada");
    case ChipClass::Blackwell:
        return QStringLiteral("Blackwell");

    case ChipClass::I8XX:
        return QStringLiteral("i830/i835");
    case ChipClass::I915:
        return QStringLiteral("i915/i945");
    case ChipClass::I965:
        return QStringLiteral("i965");
    case ChipClass::SandyBridge:
        return QStringLiteral("SandyBridge");
    case ChipClass::IvyBridge:
        return QStringLiteral("IvyBridge");
    case ChipClass::Haswell:
        return QStringLiteral("Haswell");
    case ChipClass::BayTrail:
        return QStringLiteral("Bay Trail");
    case ChipClass::Cherryview:
        return QStringLiteral("Cherryview");
    case ChipClass::Broadwell:
        return QStringLiteral("Broadwell");
    case ChipClass::ApolloLake:
        return QStringLiteral("Apollo Lake");
    case ChipClass::Skylake:
        return QStringLiteral("Skylake");
    case ChipClass::GeminiLake:
        return QStringLiteral("Gemini Lake");
    case ChipClass::KabyLake:
        return QStringLiteral("Kaby Lake");
    case ChipClass::CoffeeLake:
        return QStringLiteral("Coffee Lake");
    case ChipClass::WhiskeyLake:
        return QStringLiteral("Whiskey Lake");
    case ChipClass::CometLake:
        return QStringLiteral("Comet Lake");
    case ChipClass::CannonLake:
        return QStringLiteral("Cannon Lake");
    case ChipClass::IceLake:
        return QStringLiteral("Ice Lake");
    case ChipClass::TigerLake:
        return QStringLiteral("Tiger Lake");

    case ChipClass::Adreno1XX:
        return QStringLiteral("Adreno 1xx series");
    case ChipClass::Adreno2XX:
        return QStringLiteral("Adreno 2xx series");
    case ChipClass::Adreno3XX:
        return QStringLiteral("Adreno 3xx series");
    case ChipClass::Adreno4XX:
        return QStringLiteral("Adreno 4xx series");
    case ChipClass::Adreno5XX:
        return QStringLiteral("Adreno 5xx series");

    case ChipClass::MaliT7XX:
        return QStringLiteral("Mali T7xx series");
    case ChipClass::MaliT8XX:
        return QStringLiteral("Mali T8xx series");
    case ChipClass::MaliGXX:
        return QStringLiteral("Mali Gxx series");

    case ChipClass::Mali400:
        return QStringLiteral("Mali 400 series");
    case ChipClass::Mali450:
        return QStringLiteral("Mali 450 series");
    case ChipClass::Mali470:
        return QStringLiteral("Mali 470 series");

    case ChipClass::VC4_2_1:
        return QStringLiteral("VideoCore IV");
    case ChipClass::V3D_4_2:
        return QStringLiteral("VideoCore 3D");

    case ChipClass::Unknown:
    default:
        return QStringLiteral("Unknown");
    }
}
