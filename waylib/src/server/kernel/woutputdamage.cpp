/*
 * Copyright (C) 2022 zkyd
 *
 * Author:     zkyd <zkyd@zjide.org>
 *
 * Maintainer: zkyd <zkyd@zjide.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "woutputdamage.h"
#include "woutput.h"
#include "wsignalconnector.h"
#include "wtools.h"

#include <qwoutput.h>

extern "C" {
#include <wlr/types/wlr_output_damage.h>
#include <wlr/util/box.h>
}

QW_USE_NAMESPACE
WAYLIB_SERVER_BEGIN_NAMESPACE

class Q_DECL_HIDDEN WOutputDamagePrivate : public WObjectPrivate
{
public:
    WOutputDamagePrivate(WOutputDamage *qq)
        : WObjectPrivate(qq) {
        handle = wlr_output_damage_create(output()->nativeInterface<QWOutput>()->handle());
    }

    inline WOutput *output() {
        return qobject_cast<WOutput*>(q_func()->parent());
    }
    void connect();

    // begin slot function
    void on_frame(void*);
    // end slot function

    W_DECLARE_PUBLIC(WOutputDamage);

    wlr_output_damage *handle = nullptr;
    WSignalConnector sc;
};

void WOutputDamagePrivate::connect()
{
    sc.connect(&handle->events.frame, this, &WOutputDamagePrivate::on_frame);
    sc.connect(&handle->events.destroy, &sc, &WSignalConnector::invalidate);
}

void WOutputDamagePrivate::on_frame(void *)
{
    W_Q(WOutputDamage);
    Q_EMIT q->frame();
}

WOutputDamage::WOutputDamage(WOutput *output)
    : QObject(output)
    , WObject(*new WOutputDamagePrivate(this))
{

}

WOutputDamageHandle *WOutputDamage::handle() const
{
    W_DC(WOutputDamage);
    return reinterpret_cast<WOutputDamageHandle*>(d->handle);
}

void WOutputDamage::add(const QRegion &region)
{
    W_D(WOutputDamage);
    pixman_region32_t pixmanRegion;
    WTools::toPixmanRegion(region, &pixmanRegion);
    wlr_output_damage_add(d->handle, &pixmanRegion);
}

void WOutputDamage::addWhole()
{
    W_D(WOutputDamage);
    wlr_output_damage_add_whole(d->handle);
}

void WOutputDamage::addRect(const QRect &rect)
{
    W_D(WOutputDamage);
    wlr_box box;
    WTools::toWLRBox(rect, &box);
    wlr_output_damage_add_box(d->handle, &box);
}

WAYLIB_SERVER_END_NAMESPACE
