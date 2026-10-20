#include "annotation_engine.h"

namespace fouryshot::annotation {

AnnotationEngine::AnnotationEngine(QObject* parent) : QObject(parent) {}

void AnnotationEngine::setTool(ToolType tool) {
    m_tool = tool;
    emit stateChanged();
}

ToolType AnnotationEngine::currentTool() const {
    return m_tool;
}

void AnnotationEngine::setColor(const QColor& color) {
    m_color = color;
    emit stateChanged();
}

QColor AnnotationEngine::currentColor() const {
    return m_color;
}

void AnnotationEngine::setStrokeWidth(int width) {
    m_strokeWidth = width;
    emit stateChanged();
}

int AnnotationEngine::currentStrokeWidth() const {
    return m_strokeWidth;
}

void AnnotationEngine::setFontSize(int size) {
    m_fontSize = size;
    emit stateChanged();
}

int AnnotationEngine::currentFontSize() const {
    return m_fontSize;
}

void AnnotationEngine::setFontFamily(const QString& family) {
    m_fontFamily = family;
    emit stateChanged();
}

QString AnnotationEngine::currentFontFamily() const {
    return m_fontFamily;
}

void AnnotationEngine::setFilterIntensity(int intensity) {
    m_filterIntensity = intensity;
    emit stateChanged();
}

int AnnotationEngine::currentFilterIntensity() const {
    return m_filterIntensity;
}

void AnnotationEngine::setIsBlur(bool isBlur) {
    m_isBlur = isBlur;
    emit stateChanged();
}

bool AnnotationEngine::isBlur() const {
    return m_isBlur;
}

void AnnotationEngine::setStepSize(int size) {
    m_stepSize = size;
    emit stateChanged();
}

int AnnotationEngine::currentStepSize() const {
    return m_stepSize;
}

void AnnotationEngine::setStepStartNumber(int num) {
    m_stepStartNumber = num;
    emit stateChanged();
}

int AnnotationEngine::stepStartNumber() const {
    return m_stepStartNumber;
}

void AnnotationEngine::addItem(std::shared_ptr<AnnotationItem> item) {
    if (!item) return;
    m_items.push_back(item);
    m_undoHistory.clear();
    emit stateChanged();
}

void AnnotationEngine::removeItem(std::shared_ptr<AnnotationItem> item) {
    auto it = std::find(m_items.begin(), m_items.end(), item);
    if (it != m_items.end()) {
        m_items.erase(it);
        emit stateChanged();
    }
}

void AnnotationEngine::undo() {
    if (m_items.empty()) return;
    m_undoHistory.push_back(m_items.back());
    m_items.pop_back();
    emit stateChanged();
}

void AnnotationEngine::redo() {
    if (m_undoHistory.empty()) return;
    m_items.push_back(m_undoHistory.back());
    m_undoHistory.pop_back();
    emit stateChanged();
}

void AnnotationEngine::clear() {
    m_items.clear();
    m_undoHistory.clear();
    emit stateChanged();
}

bool AnnotationEngine::canUndo() const {
    return !m_items.empty();
}

bool AnnotationEngine::canRedo() const {
    return !m_undoHistory.empty();
}

int AnnotationEngine::nextStepNumber() const {
    int count = 0;
    for (const auto& item : m_items) {
        if (item->type == ItemType::StepNumber) {
            count++;
        }
    }
    return m_stepStartNumber + count;
}

void AnnotationEngine::draw(QPainter& painter) {
    for (const auto& item : m_items) {
        item->draw(painter);
    }
}

const std::vector<std::shared_ptr<AnnotationItem>>& AnnotationEngine::items() const {
    return m_items;
}

} // namespace fouryshot::annotation
