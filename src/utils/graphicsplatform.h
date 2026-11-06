// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QString>

class QRhi;

// Identifies the GPU behind the render window: vendor family, chip class and
// the model the driver reports. The render API and its versions are left to
// Qt Quick's GraphicsInfo.
class GraphicsPlatform
{
public:
    enum class Driver {
        Radeon,
        NVidia,
        Intel,
        Qualcomm,
        Panfrost,
        Lima,
        VideoCore4,
        VideoCore3D,
        Virgl,
        VMware,
        VirtualBox,
        Software,
        Unknown,
    };

    // clang-format off
    enum class ChipClass {
        // Radeon
        R100, R200, R300, R400, R500, R600, R700, Evergreen, NorthernIslands,
        SouthernIslands, SeaIslands, VolcanicIslands, ArcticIslands, Vega, Navi,
        // NVIDIA
        NV10, NV20, NV30, NV40, G80, GF100, Kepler, Maxwell, Pascal, Turing,
        Ampere, Ada, Blackwell,
        // Intel
        I8XX, I915, I965, SandyBridge, IvyBridge, Haswell, BayTrail, Cherryview,
        Broadwell, ApolloLake, Skylake, GeminiLake, KabyLake, CoffeeLake,
        WhiskeyLake, CometLake, CannonLake, IceLake, TigerLake,
        // Qualcomm Adreno
        Adreno1XX, Adreno2XX, Adreno3XX, Adreno4XX, Adreno5XX,
        // Panfrost Mali
        MaliT7XX, MaliT8XX, MaliGXX,
        // Lima Mali
        Mali400, Mali450, Mali470,
        // Broadcom VideoCore IV and VideoCore 3D
        VC4_2_1, V3D_4_2,
        Unknown,
    };
    // clang-format on

    explicit GraphicsPlatform(const QRhi *rhi);

    Driver driver() const { return m_driver; }
    ChipClass chipClass() const { return m_chipClass; }
    // Model name exactly as the driver reports it, e.g.
    // "NVIDIA GeForce RTX 3060/PCIe/SSE2" on OpenGL; driver suffixes are kept.
    QString deviceName() const { return m_deviceName; }
    // "integrated", "discrete", "virtual", "cpu" or "unknown".
    QString deviceType() const { return m_deviceType; }

    bool isRadeon() const;
    bool isNvidia() const;
    bool isIntel() const;
    bool isAdreno() const;
    bool isPanfrost() const;
    bool isLima() const;
    bool isVideoCore4() const;
    bool isVideoCore3D() const;
    bool isSoftware() const;
    bool isVirtualMachine() const;

    // One line summary for logging.
    QString toString() const;

    static QString driverToString(Driver driver);
    static QString chipClassToString(ChipClass chipClass);

private:
    bool m_virtualMachine = false;
    Driver m_driver = Driver::Unknown;
    ChipClass m_chipClass = ChipClass::Unknown;
    QString m_deviceName;
    QString m_deviceType;
};
