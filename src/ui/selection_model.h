#pragma once

#include <QObject>
#include <QRect>
#include <QPoint>

namespace fouryshot::ui {

enum class HandlePosition {
    None,
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

enum class SelectionState {
    Idle,
    Selecting,
    Selected,
    Moving,
    Resizing
};

class SelectionModel : public QObject {
    Q_OBJECT
public:
    explicit SelectionModel(QObject* parent = nullptr);

    QRect rect() const;
    void setRect(const QRect& r);

    SelectionState state() const;
    void setState(SelectionState s);

    HandlePosition activeHandle() const;
    void setActiveHandle(HandlePosition h);

    bool hasSelection() const;
    void clear();

    HandlePosition hitTestHandles(const QPoint& pt, int handleSize = 8) const;
    QRect handleRect(HandlePosition pos, int handleSize = 8) const;

signals:
    void rectChanged(const QRect& rect);
    void stateChanged(SelectionState state);

private:
    QRect m_rect;
    SelectionState m_state = SelectionState::Idle;
    HandlePosition m_activeHandle = HandlePosition::None;
};

} // namespace fouryshot::ui
