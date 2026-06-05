#pragma once
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>

struct DeviceEntry {
    QString  ip;
    quint16  port;
    QString  username;
    QString  password;
    int      streamType;  // 0=main, 1=sub
    QList<int> channels;
};

struct AppConfig {
    int monitorIndex;  // which monitor index to use
    int gridSize;      // grid dimension: 1=1x1, 2=2x2, 3=3x3, 4=4x4
    bool renderRaw;        // bypass HCPreview pipeline: raw callback → PlayCtrl (no overlays, phone unaffected)
    bool hideVcaOverlay;   // fallback: disable motion display on device via NET_DVR_SetDVRConfig (persistent, affects all clients)
    bool optimizeRender;   // enable PlayCtrl quality settings: high-quality scaling, deflash, B-frame decode, VIE deblock+denoise
    QList<DeviceEntry> devices;
    bool valid;
};

inline AppConfig loadConfig(const QString &path)
{
    AppConfig cfg{};
    cfg.monitorIndex = 0;
    cfg.gridSize     = 1;
    cfg.renderRaw       = false;
    cfg.hideVcaOverlay  = false;
    cfg.optimizeRender  = false;
    cfg.valid          = false;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return cfg;

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        return cfg;

    QJsonObject root = doc.object();
    cfg.monitorIndex = root["monitorIndex"].toInt(0);
    cfg.gridSize     = root["gridSize"].toInt(1);
    cfg.renderRaw      = root["renderRaw"].toBool(false);
    cfg.hideVcaOverlay = root["hideVcaOverlay"].toBool(false);
    cfg.optimizeRender = root["optimizeRender"].toBool(false);

    for (const QJsonValue &dv : root["devices"].toArray()) {
        QJsonObject d = dv.toObject();
        DeviceEntry e;
        e.ip         = d["ip"].toString();
        e.port       = static_cast<quint16>(d["port"].toInt(8000));
        e.username   = d["username"].toString("admin");
        e.password   = d["password"].toString();
        e.streamType = d["streamType"].toInt(0);
        for (const QJsonValue &cv : d["channels"].toArray())
            e.channels.append(cv.toInt());
        cfg.devices.append(e);
    }
    cfg.valid = true;
    return cfg;
}
