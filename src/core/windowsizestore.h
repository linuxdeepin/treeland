#pragma once
#include <QObject>
#include <QSize>
#include <QSettings>
#include <QHash>

// 简单窗口尺寸持久化：根据 appId 记录最后一次正常关闭(销毁)时的 normalGeometry 尺寸
class WindowSizeStore : public QObject {
    Q_OBJECT
public:
    explicit WindowSizeStore(QObject *parent = nullptr)
        : QObject(parent)
        , m_settings("deepin", "treeland-window-size") {}

    QSize lastSizeFor(const QString &appId) const {
        if (appId.isEmpty()) return {};
        return m_settings.value(appId + "/normalSize").toSize();
    }

    void saveSize(const QString &appId, const QSize &size) {
        if (appId.isEmpty() || !size.isValid()) return;
        m_settings.setValue(appId + "/normalSize", size);
        m_settings.sync();
    }
private:
    mutable QSettings m_settings; // org/app 形式，写入 XDG_CONFIG_HOME
};
