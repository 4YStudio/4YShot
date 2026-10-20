#include "selection_model.h"

namespace fouryshot::ui {

SelectionModel::SelectionModel(QObject* parent) : QObject(parent) {}

QRect SelectionModel::rect() const {
    return m_rect.normalized();
}

void SelectionModel::setRect(const QRect& r) {
    if (m_rect != r) {
        m_rect = r;
        emit rectChanged(m_rect.normalized());
    }
}

SelectionState SelectionModel::state() const {
    return m_state;
}

void SelectionModel::setState(SelectionState s) {
    if (m_state != s) {
        m_state = s;
        emit stateChanged(m_state);
    }
}

HandlePosition SelectionModel::activeHandle() const {
    return m_activeHandle;
}

void SelectionModel::setActiveHandle(HandlePosition h) {
    m_activeHandle = h;
}

bool SelectionModel::hasSelection() const {
    return !m_rect.isEmpty() && m_rect.width() > 3 && m_rect.height() > 3;
}

void SelectionModel::clear() {
    m_rect = QRect();
    m_state = SelectionState::Idle;
    m_activeHandle = HandlePosition::None;
    emit rectChanged(m_rect);
    emit stateChanged(m_state);
}

QRect SelectionModel::handleRect(HandlePosition pos, int handleSize) const {
    QRect r = m_rect.normalized();
    int hs = handleSize;
    int hhs = hs / 2;

    switch (pos) {
    case HandlePosition::TopLeft:
        return QRect(r.left() - hhs, r.top() - hhs, hs, hs);
    case HandlePosition::Top:
        return QRect(r.center().x() - hhs, r.top() - hhs, hs, hs);
    case HandlePosition::TopRight:
        return QRect(r.right() - hhs, r.top() - hhs, hs, hs);
    case HandlePosition::Right:
        return QRect(r.right() - hhs, r.center().y() - hhs, hs, hs);
    case HandlePosition::BottomRight:
        return QRect(r.right() - hhs, r.bottom() - hhs, hs, hs);
    case HandlePosition::Bottom:
        return QRect(r.center().x() - hhs, r.bottom() - hhs, hs, hs);
    case HandlePosition::BottomLeft:
        return QRect(r.left() - hhs, r.bottom() - hhs, hs, hs);
    case HandlePosition::Left:
        return QRect(r.left() - hhs, r.center().y() - hhs, hs, hs);
    default:
        return QRect();
    }
}

HandlePosition SelectionModel::hitTestHandles(const QPoint& pt, int handleSize) const {
    if (!hasSelection()) return HandlePosition::None;

    const HandlePosition positions[] = {
        HandlePosition::TopLeft, HandlePosition::Top, HandlePosition::TopRight,
        HandlePosition::Right, HandlePosition::BottomRight, HandlePosition::Bottom,
        HandlePosition::BottomLeft, HandlePosition::Left
    };

    for (auto pos : positions) {
        if (handleRect(pos, handleSize).contains(pt)) {
            return pos;
        }
    }
    return HandlePosition::None;
}

} // namespace fouryshot::ui
