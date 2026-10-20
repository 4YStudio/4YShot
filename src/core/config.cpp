#include "config.h"
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

namespace fouryshot::core {

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

Config::Config() {
    savePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/Screenshots";
    load();
}

QString Config::configFilePath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    return configDir + "/config.json";
}

void Config::load() {
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    if (obj.contains("shortcutCapture")) shortcutCapture = obj["shortcutCapture"].toString(shortcutCapture);
    if (obj.contains("shortcutPin")) shortcutPin = obj["shortcutPin"].toString(shortcutPin);
    if (obj.contains("shortcutOcr")) shortcutOcr = obj["shortcutOcr"].toString(shortcutOcr);
    if (obj.contains("autoCopy")) autoCopy = obj["autoCopy"].toBool(autoCopy);
    if (obj.contains("autoSave")) autoSave = obj["autoSave"].toBool(autoSave);
    if (obj.contains("savePath")) savePath = obj["savePath"].toString(savePath);
    if (obj.contains("saveFormat")) saveFormat = obj["saveFormat"].toString(saveFormat);
    if (obj.contains("magnifierEnabled")) magnifierEnabled = obj["magnifierEnabled"].toBool(magnifierEnabled);
    if (obj.contains("defaultColor")) defaultColor = QColor(obj["defaultColor"].toString(defaultColor.name()));
    if (obj.contains("defaultStrokeWidth")) defaultStrokeWidth = obj["defaultStrokeWidth"].toInt(defaultStrokeWidth);
}

void Config::save() const {
    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonObject obj;
    obj["shortcutCapture"] = shortcutCapture;
    obj["shortcutPin"] = shortcutPin;
    obj["shortcutOcr"] = shortcutOcr;
    obj["autoCopy"] = autoCopy;
    obj["autoSave"] = autoSave;
    obj["savePath"] = savePath;
    obj["saveFormat"] = saveFormat;
    obj["magnifierEnabled"] = magnifierEnabled;
    obj["defaultColor"] = defaultColor.name();
    obj["defaultStrokeWidth"] = defaultStrokeWidth;

    QJsonDocument doc(obj);
    file.write(doc.toJson(QJsonDocument::Indented));
}

} // namespace fouryshot::core
