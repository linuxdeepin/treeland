// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <wserver.h>
#include <wxdgsurface.h>
#include <WWrapPointer>

#include <QObject>
#include <QQmlEngine>
#include <QQuickItem>

class SurfaceWrapper;
class PersonalizationManagerInterfaceV1;

QW_USE_NAMESPACE
WAYLIB_SERVER_USE_NAMESPACE

struct Shadow
{
    int32_t radius;
    QPoint offset;
    QColor color;
    bool operator==(const Shadow &other) const = default;
};

struct Border
{
    int32_t width;
    QColor color;
    bool operator==(const Border &other) const = default;
};

class PersonalizationManagerInterfaceV1Private;
class PersonalizationWindowContextV1Private;
class PersonalizationWindowContextV1 : public QObject
{
    Q_OBJECT
public:
    enum WindowState
    {
        NoTitleBar = 1,
    };
    Q_ENUM(WindowState)
    Q_DECLARE_FLAGS(WindowStates, WindowState)

    enum class ShadowState
    {
        Unset,
        Disabled,
        Enabled,
    };
    Q_ENUM(ShadowState)

    enum class BorderState
    {
        Unset,
        Disabled,
        Enabled,
    };
    Q_ENUM(BorderState)

    ~PersonalizationWindowContextV1() override;

    wl_resource *resource() const;
    wlr_surface *surface() const;
    int32_t backgroundType() const;
    int32_t cornerRadius() const;
    ShadowState shadowState() const;
    Shadow shadow() const;
    BorderState borderState() const;
    Border border() const;
    WindowStates states() const;

    static PersonalizationWindowContextV1 *get(wl_resource *resource);
    static PersonalizationWindowContextV1 *getWindowContext(WSurface *surface);
Q_SIGNALS:
    void backgroundTypeChanged();
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void windowStateChanged();

private:
    explicit PersonalizationWindowContextV1(wl_resource *resource,
                                            wlr_surface *surface);

private:
    std::unique_ptr<PersonalizationWindowContextV1Private> d;

    friend class PersonalizationManagerInterfaceV1Private;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PersonalizationWindowContextV1::WindowStates)

class PersonalizationCursorContextV1Private;
class PersonalizationCursorContextV1 : public QObject
{
    Q_OBJECT
public:
    ~PersonalizationCursorContextV1() override;

    wl_resource *resource() const;
    QSize size() const;
    QString theme() const;

    void setTheme(const QString &theme);
    void setSize(const QSize &size);
    void verify(bool verified);

    void sendTheme();
    void sendSize();

    static PersonalizationCursorContextV1 *get(wl_resource *resource);

Q_SIGNALS:
    void commit(PersonalizationCursorContextV1 *context);
    void getSize();
    void getTheme();

private:
    explicit PersonalizationCursorContextV1(wl_resource *resource);

private:
    std::unique_ptr<PersonalizationCursorContextV1Private> d;

    friend class PersonalizationManagerInterfaceV1Private;
};

class PersonalizationAppearanceContextV1Private;
class PersonalizationAppearanceContextV1 : public QObject
{
    Q_OBJECT
public:
    ~PersonalizationAppearanceContextV1() override;

    wl_resource *resource() const;
    void setRoundCornerRadius(int32_t radius);
    void sendRoundCornerRadius(int32_t radius);

    void setIconTheme(const QString &theme);
    void sendIconTheme(const QString &theme);

    void setActiveColor(const QString &color);
    void sendActiveColor(const QString &color);

    void setWindowOpacity(uint32_t opacity);
    void sendWindowOpacity(uint32_t opacity);

    void setWindowThemeType(uint32_t type);
    void sendWindowThemeType(uint32_t type);

    void setWindowTitlebarHeight(uint32_t height);
    void sendWindowTitlebarHeight(uint32_t height);

    static PersonalizationAppearanceContextV1 *get(wl_resource *resource);

Q_SIGNALS:
    void roundCornerRadiusChanged(int32_t radius);
    void iconThemeChanged(const QString &iconTheme);
    void activeColorChanged(const QString &color);
    void windowOpacityChanged(uint32_t opacity);
    void windowThemeTypeChanged(uint32_t type);
    void titlebarHeightChanged(uint32_t height);

    void requestRoundCornerRadius();
    void requestIconTheme();
    void requestActiveColor();
    void requestWindowOpacity();
    void requestWindowThemeType();
    void requestWindowTitlebarHeight();

private:
    explicit PersonalizationAppearanceContextV1(wl_resource *resource);

private:
    std::unique_ptr<PersonalizationAppearanceContextV1Private> d;

    friend class PersonalizationManagerInterfaceV1Private;
};

class PersonalizationFontContextV1Private;
class PersonalizationFontContextV1 : public QObject
{
    Q_OBJECT
public:
    ~PersonalizationFontContextV1() override;

    wl_resource *resource() const;
    void sendFont(const QString &font);
    void sendMonospaceFont(const QString &font);
    void sendFontSize(uint32_t size);

    static PersonalizationFontContextV1 *get(wl_resource *resource);

Q_SIGNALS:
    void fontChanged(const QString &font);
    void monoFontChanged(const QString &font);
    void fontSizeChanged(uint32_t size);
    void requestFont();
    void requestMonoFont();
    void requestFontSize();

private:
    explicit PersonalizationFontContextV1(wl_resource *resource);

private:
    std::unique_ptr<PersonalizationFontContextV1Private> d;

    friend class PersonalizationManagerInterfaceV1Private;
};

class Personalization : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(BackgroundType backgroundType READ backgroundType NOTIFY backgroundTypeChanged)
    Q_PROPERTY(int32_t cornerRadius READ cornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(Shadow shadow READ shadow NOTIFY shadowChanged)
    Q_PROPERTY(Border border READ border NOTIFY borderChanged)
    Q_PROPERTY(bool noTitlebar READ noTitlebar NOTIFY windowStateChanged)
    Q_PROPERTY(bool shadowEnabled READ shadowEnabled NOTIFY shadowChanged)
    Q_PROPERTY(bool borderEnabled READ borderEnabled NOTIFY borderChanged)

public:
    enum BackgroundType
    {
        Normal,
        Wallpaper,
        Blur
    };
    Q_ENUM(BackgroundType)

    Personalization(WToplevelSurface *target,
                    PersonalizationManagerInterfaceV1 *manager,
                    SurfaceWrapper *parent);

    SurfaceWrapper *surfaceWrapper() const;
    Personalization::BackgroundType backgroundType() const;

    int32_t cornerRadius() const
    {
        return m_cornerRadius;
    }

    Shadow shadow() const
    {
        return m_shadow;
    }

    Border border() const
    {
        return m_border;
    }

    bool noTitlebar() const;
    bool shadowEnabled() const;
    bool borderEnabled() const;

Q_SIGNALS:
    void backgroundTypeChanged();
    void cornerRadiusChanged();
    void shadowChanged();
    void borderChanged();
    void windowStateChanged();

private Q_SLOTS:
    void resetProperties();

private:
    void setBackgroundType(BackgroundType type);
    void setCornerRadius(int32_t radius);
    void setShadow(const Shadow &shadow);
    void setBorder(const Border &border);
    void setWindowStates(PersonalizationWindowContextV1::WindowStates states);
    void setShadowState(PersonalizationWindowContextV1::ShadowState state);
    void setBorderState(PersonalizationWindowContextV1::BorderState state);

private:
    WWrapPointer<WToplevelSurface> m_target;
    PersonalizationManagerInterfaceV1 *m_manager = nullptr;
    BackgroundType m_backgroundType = BackgroundType::Normal;
    int32_t m_cornerRadius = 0;
    Shadow m_shadow {};
    Border m_border {};
    PersonalizationWindowContextV1::WindowStates m_states {};
    PersonalizationWindowContextV1::ShadowState m_shadowState = PersonalizationWindowContextV1::ShadowState::Unset;
    PersonalizationWindowContextV1::BorderState m_borderState = PersonalizationWindowContextV1::BorderState::Unset;
};

class PersonalizationManagerInterfaceV1 : public QObject, public WServerInterface
{
    Q_OBJECT

    Q_PROPERTY(uid_t userId READ userId WRITE setUserId NOTIFY userIdChanged FINAL)
    Q_PROPERTY(QString cursorTheme READ cursorTheme WRITE setCursorTheme NOTIFY cursorThemeChanged FINAL)
    Q_PROPERTY(QSize cursorSize READ cursorSize WRITE setCursorSize NOTIFY cursorSizeChanged FINAL)

public:
    explicit PersonalizationManagerInterfaceV1(QObject *parent = nullptr);
    ~PersonalizationManagerInterfaceV1() override;

    void onCursorContextCreated(PersonalizationCursorContextV1 *context);
    void onAppearanceContextCreated(PersonalizationAppearanceContextV1 *context);
    void onFontContextCreated(PersonalizationFontContextV1 *context);

    void onWindowPersonalizationChanged();

    void onCursorCommit(PersonalizationCursorContextV1 *context);

    uid_t userId();
    void setUserId(uid_t uid);

    QString cursorTheme();
    void setCursorTheme(const QString &name);

    QSize cursorSize();
    void setCursorSize(const QSize &size);

    int32_t windowRadius() const;

    QString iconTheme() const;

    QByteArrayView interfaceName() const override;

    static constexpr int InterfaceVersion = 2;
Q_SIGNALS:
    void userIdChanged(uid_t uid);
    void lockscreenChanged();
    void cursorThemeChanged(const QString &name);
    void cursorSizeChanged(const QSize &size);
    void windowContextCreated(PersonalizationWindowContextV1 *context);

protected:
    void create(WServer *server) override;
    void destroy(WServer *server) override;
    wl_global *global() const override;

private:
    std::unique_ptr<PersonalizationManagerInterfaceV1Private> d;
};
