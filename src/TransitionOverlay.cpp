#include "TransitionOverlay.h"

#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QVariantAnimation>
#include <QtMath>

namespace {
// Draws pixmap scaled around the widget centre
void drawScaled(QPainter &p, const QRectF &rect, const QPixmap &pixmap, qreal scale, qreal opacity)
{
    QRectF target(0, 0, rect.width() * scale, rect.height() * scale);
    target.moveCenter(rect.center());
    p.setOpacity(opacity);
    p.drawPixmap(target, pixmap, pixmap.rect());
    p.setOpacity(1.0);
}
}

TransitionOverlay::TransitionOverlay(QWidget *parent)
    : QWidget(parent)
    , m_animation(new QVariantAnimation(this))
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    hide();

    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(DurationMs);
    m_animation->setEasingCurve(QEasingCurve::InOutCubic);

    connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_progress = value.toReal();
        update();
    });
    connect(m_animation, &QVariantAnimation::finished, this, [this] {
        hide();
        m_from = m_to = QPixmap();
        emit finished();
    });
}

void TransitionOverlay::start(const QPixmap &from, const QPixmap &to, Effect effect)
{
    stop();
    if (effect == Effect::Random) {
        QList<Effect> choices = effects();
        choices.removeAll(Effect::Random);
        choices.removeAll(Effect::None);
        effect = choices.at(QRandomGenerator::global()->bounded(int(choices.size())));
    }
    if (effect == Effect::None || from.isNull() || to.isNull()) {
        emit finished();
        return;
    }

    m_from = from;
    m_to = to;
    m_effect = effect;
    m_progress = 0.0;
    setGeometry(parentWidget()->rect());
    raise();
    show();
    m_animation->start();
}

void TransitionOverlay::stop()
{
    if (m_animation->state() == QAbstractAnimation::Running) {
        m_animation->stop();
        hide();
        m_from = m_to = QPixmap();
    }
}

bool TransitionOverlay::isRunning() const
{
    return m_animation->state() == QAbstractAnimation::Running;
}

QList<TransitionOverlay::Effect> TransitionOverlay::effects()
{
    // The order of the settings panel
    return {Effect::Random, Effect::Crossfade, Effect::FadeThroughBlack, Effect::Slide,
            Effect::Wipe, Effect::Zoom, Effect::Circle, Effect::None};
}

QString TransitionOverlay::effectName(Effect effect)
{
    switch (effect) {
    case Effect::None: return tr("None");
    case Effect::Crossfade: return tr("Crossfade");
    case Effect::FadeThroughBlack: return tr("Fade Out / In");
    case Effect::Slide: return tr("Slide");
    case Effect::Wipe: return tr("Wipe");
    case Effect::Zoom: return tr("Zoom");
    case Effect::Circle: return tr("Circle Reveal");
    case Effect::Random: return tr("Random");
    }
    return {};
}

void TransitionOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::SmoothPixmapTransform | QPainter::Antialiasing);
    const QRectF r = rect();
    const qreal t = m_progress;

    switch (m_effect) {
    case Effect::Crossfade:
        p.drawPixmap(r, m_from, m_from.rect());
        drawScaled(p, r, m_to, 1.0, t);
        break;
    case Effect::FadeThroughBlack:
        p.fillRect(r, Qt::black);
        if (t < 0.5)
            drawScaled(p, r, m_from, 1.0, 1.0 - 2 * t);
        else
            drawScaled(p, r, m_to, 1.0, 2 * t - 1.0);
        break;
    case Effect::Slide: {
        const qreal dx = r.width() * t;
        p.drawPixmap(r.translated(-dx, 0), m_from, m_from.rect());
        p.drawPixmap(r.translated(r.width() - dx, 0), m_to, m_to.rect());
        break;
    }
    case Effect::Wipe:
        p.drawPixmap(r, m_from, m_from.rect());
        p.setClipRect(QRectF(0, 0, r.width() * t, r.height()));
        p.drawPixmap(r, m_to, m_to.rect());
        break;
    case Effect::Zoom:
        // Fly through: old frame grows and fades, new one settles in from slightly smaller
        p.fillRect(r, Qt::black);
        drawScaled(p, r, m_from, 1.0 + 0.3 * t, 1.0 - t);
        drawScaled(p, r, m_to, 0.8 + 0.2 * t, t);
        break;
    case Effect::Circle: {
        p.drawPixmap(r, m_from, m_from.rect());
        const qreal radius = t * qHypot(r.width(), r.height()) / 2;
        QPainterPath clip;
        clip.addEllipse(r.center(), radius, radius);
        p.setClipPath(clip);
        p.drawPixmap(r, m_to, m_to.rect());
        break;
    }
    case Effect::None:
    case Effect::Random:
        p.drawPixmap(r, m_to, m_to.rect());
        break;
    }
}
