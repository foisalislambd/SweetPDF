#pragma once

#include <QRectF>
#include <QSizeF>
#include <QTransform>

#include <mupdf/fitz.h>

namespace sweet::core {

inline QRectF toQRectF(const fz_rect &r)
{
    return QRectF(QPointF(r.x0, r.y0), QPointF(r.x1, r.y1));
}

inline fz_rect toFzRect(const QRectF &r)
{
    return fz_make_rect(static_cast<float>(r.left()), static_cast<float>(r.top()),
                        static_cast<float>(r.right()), static_cast<float>(r.bottom()));
}

inline QRectF toQRectF(const fz_quad &q)
{
    return toQRectF(fz_rect_from_quad(q));
}

inline QSizeF sizeOf(const fz_rect &r)
{
    return QSizeF(r.x1 - r.x0, r.y1 - r.y0);
}

inline QTransform toQTransform(const fz_matrix &m)
{
    return QTransform(m.a, m.b, m.c, m.d, m.e, m.f);
}

inline fz_matrix toFzMatrix(const QTransform &t)
{
    return fz_make_matrix(static_cast<float>(t.m11()), static_cast<float>(t.m12()),
                          static_cast<float>(t.m21()), static_cast<float>(t.m22()),
                          static_cast<float>(t.dx()), static_cast<float>(t.dy()));
}

} // namespace sweet::core
