// Copyright (C) 2026 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: Apache-2.0 OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#pragma once

#include <QPointF>
#include <QQuickItem>
#include <QRectF>
#include <QtQml/qqmlregistration.h>

namespace Treeland {

class RadiusEffectItemPrivate;
class RadiusEffectItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(qreal topLeftRadius READ topLeftRadius WRITE setTopLeftRadius RESET resetTopLeftRadius NOTIFY topLeftRadiusChanged FINAL)
    Q_PROPERTY(qreal topRightRadius READ topRightRadius WRITE setTopRightRadius RESET resetTopRightRadius NOTIFY topRightRadiusChanged FINAL)
    Q_PROPERTY(qreal bottomLeftRadius READ bottomLeftRadius WRITE setBottomLeftRadius RESET resetBottomLeftRadius NOTIFY bottomLeftRadiusChanged FINAL)
    Q_PROPERTY(qreal bottomRightRadius READ bottomRightRadius WRITE setBottomRightRadius RESET resetBottomRightRadius NOTIFY bottomRightRadiusChanged FINAL)
    Q_PROPERTY(QQuickItem *sourceItem READ sourceItem WRITE setSourceItem NOTIFY sourceItemChanged)
    Q_PROPERTY(bool hideSource READ hideSource WRITE setHideSource NOTIFY hideSourceChanged)
    Q_PROPERTY(QPointF sourceOffset READ sourceOffset WRITE setSourceOffset NOTIFY sourceOffsetChanged FINAL)
    Q_PROPERTY(qreal sourceWidthScale READ sourceWidthScale WRITE setSourceWidthScale NOTIFY sourceWidthScaleChanged FINAL)
    Q_PROPERTY(qreal sourceHeightScale READ sourceHeightScale WRITE setSourceHeightScale NOTIFY sourceHeightScaleChanged FINAL)
    Q_PROPERTY(QRectF renderRect READ renderRect WRITE setRenderRect RESET resetRenderRect NOTIFY renderRectChanged FINAL)
    QML_NAMED_ELEMENT(TRadiusEffect)

public:
    explicit RadiusEffectItem(QQuickItem *parent = nullptr);
    ~RadiusEffectItem() override;

    qreal radius() const;
    void setRadius(qreal radius);

    qreal topLeftRadius() const;
    void setTopLeftRadius(qreal radius);
    void resetTopLeftRadius();
    qreal topRightRadius() const;
    void setTopRightRadius(qreal radius);
    void resetTopRightRadius();
    qreal bottomLeftRadius() const;
    void setBottomLeftRadius(qreal radius);
    void resetBottomLeftRadius();
    qreal bottomRightRadius() const;
    void setBottomRightRadius(qreal radius);
    void resetBottomRightRadius();

    QQuickItem *sourceItem() const;
    void setSourceItem(QQuickItem *item);
    bool hideSource() const;
    void setHideSource(bool hide);

    QPointF sourceOffset() const;
    void setSourceOffset(const QPointF &offset);
    qreal sourceWidthScale() const;
    void setSourceWidthScale(qreal scale);
    qreal sourceHeightScale() const;
    void setSourceHeightScale(qreal scale);

    QRectF renderRect() const;
    void setRenderRect(const QRectF &rect);
    void resetRenderRect();

Q_SIGNALS:
    void radiusChanged();
    void topLeftRadiusChanged();
    void topRightRadiusChanged();
    void bottomLeftRadiusChanged();
    void bottomRightRadiusChanged();
    void sourceItemChanged();
    void hideSourceChanged();
    void sourceOffsetChanged();
    void sourceWidthScaleChanged();
    void sourceHeightScaleChanged();
    void renderRectChanged();

private Q_SLOTS:
    void sourceItemDestroyed(QObject *item);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &value) override;

private:
    Q_DISABLE_COPY(RadiusEffectItem)
    Q_DECLARE_PRIVATE(RadiusEffectItem)
};

} // namespace Treeland
