#pragma once

#include <QPixmap>
#include <QWidget>

class QVariantAnimation;

// Animates from a snapshot of the old frame to a snapshot of the new one,
// drawn on top of its parent widget.
class TransitionOverlay : public QWidget
{
    Q_OBJECT

public:
    enum class Effect { None, Crossfade, FadeThroughBlack, Slide, Wipe, Zoom, Circle, Random };
    Q_ENUM(Effect)

    static constexpr int DurationMs = 700;

    explicit TransitionOverlay(QWidget *parent);

    void start(const QPixmap &from, const QPixmap &to, Effect effect);
    void stop();
    bool isRunning() const;

    static QList<Effect> effects();
    static QString effectName(Effect effect);

signals:
    void finished();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVariantAnimation *m_animation = nullptr;
    QPixmap m_from;
    QPixmap m_to;
    Effect m_effect = Effect::Crossfade;
    qreal m_progress = 0.0;
};
