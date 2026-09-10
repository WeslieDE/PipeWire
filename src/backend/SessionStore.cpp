#include "SessionStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

QString SessionStore::filePath() const
{
    const QString configDir
        = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
          + QStringLiteral("/MixPipe");
    QDir().mkpath(configDir);
    return configDir + QStringLiteral("/session.json");
}

SessionStore::Session SessionStore::load() const
{
    Session session;

    QFile file(filePath());
    if (!file.open(QIODevice::ReadOnly)) {
        return session;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return session;
    }
    const QJsonObject root = doc.object();

    for (const QJsonValue &v : root.value(QStringLiteral("virtualDevices")).toArray()) {
        session.virtualDevices.append(v.toString());
    }

    for (const QJsonValue &v : root.value(QStringLiteral("linkRules")).toArray()) {
        const QJsonObject o = v.toObject();
        session.linkRules.append(LinkRule{
            o.value(QStringLiteral("sourceKind")).toString(),
            o.value(QStringLiteral("sourceKey")).toString(),
            o.value(QStringLiteral("targetKind")).toString(),
            o.value(QStringLiteral("targetKey")).toString(),
        });
    }

    for (const QJsonValue &v : root.value(QStringLiteral("nodeVolumes")).toArray()) {
        const QJsonObject o = v.toObject();
        session.nodeVolumes.append(NodeVolume{
            o.value(QStringLiteral("kind")).toString(),
            o.value(QStringLiteral("key")).toString(),
            static_cast<float>(o.value(QStringLiteral("volume")).toDouble(1.0)),
        });
    }

    for (const QJsonValue &v : root.value(QStringLiteral("pinnedNodes")).toArray()) {
        const QJsonObject o = v.toObject();
        session.pinnedNodes.append(PinnedNode{
            o.value(QStringLiteral("kind")).toString(),
            o.value(QStringLiteral("key")).toString(),
        });
    }

    return session;
}

void SessionStore::save(const Session &session) const
{
    QJsonArray virtualDevices;
    for (const QString &name : session.virtualDevices) {
        virtualDevices.append(name);
    }

    QJsonArray linkRules;
    for (const LinkRule &rule : session.linkRules) {
        QJsonObject o;
        o[QStringLiteral("sourceKind")] = rule.sourceKind;
        o[QStringLiteral("sourceKey")] = rule.sourceKey;
        o[QStringLiteral("targetKind")] = rule.targetKind;
        o[QStringLiteral("targetKey")] = rule.targetKey;
        linkRules.append(o);
    }

    QJsonArray nodeVolumes;
    for (const NodeVolume &nv : session.nodeVolumes) {
        QJsonObject o;
        o[QStringLiteral("kind")] = nv.kind;
        o[QStringLiteral("key")] = nv.key;
        o[QStringLiteral("volume")] = nv.volume;
        nodeVolumes.append(o);
    }

    QJsonArray pinnedNodes;
    for (const PinnedNode &p : session.pinnedNodes) {
        QJsonObject o;
        o[QStringLiteral("kind")] = p.kind;
        o[QStringLiteral("key")] = p.key;
        pinnedNodes.append(o);
    }

    QJsonObject root;
    root[QStringLiteral("virtualDevices")] = virtualDevices;
    root[QStringLiteral("linkRules")] = linkRules;
    root[QStringLiteral("nodeVolumes")] = nodeVolumes;
    root[QStringLiteral("pinnedNodes")] = pinnedNodes;

    QFile file(filePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
