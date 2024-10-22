// Copyright (C) 2024 groveer <guoyao@uniontech.com>.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include <QQuickItem>

class Q_DECL_HIDDEN EventItem : public QQuickItem
{
    Q_OBJECT
    QML_NAMED_ELEMENT(EventItem)
public:
    explicit EventItem(QQuickItem *parent = nullptr);

    inline bool isValid() const
    {
        return parent();
    }

private:
    bool event(QEvent *event) override;
};
