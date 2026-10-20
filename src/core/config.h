#pragma once

#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QStandardPaths>

namespace fouryshot::core {

class Config {
public:
    static Config& instance();

    void load();
    void save() const;

    QString shortcutCapture = "Print";
    QString shortcutPin = "F3";
    QString shortcutOcr = "Alt+Print";

    bool autoCopy = true;
    bool autoSave = false;
    QString savePath;
    QString saveFormat = "png";

    bool magnifierEnabled = true;
    QColor defaultColor = QColor("#1677ff");
    int defaultStrokeWidth = 3;

private:
    Config();
    QString configFilePath() const;
};

} // namespace fouryshot::core
