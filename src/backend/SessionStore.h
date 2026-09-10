#pragma once

#include <QList>
#include <QObject>
#include <QString>

// Persistiert die gewünschte MixPipe-Konfiguration (virtuelle Geräte,
// Verbindungsregeln, Lautstärken) als JSON unter
// ~/.config/MixPipe/session.json. Adressiert Nodes über stabile Identitäten
// (siehe NodeIdentity in AudioGraph.h), da PipeWire-Node-IDs pro Sitzung neu
// vergeben werden.
class SessionStore {
public:
    struct LinkRule {
        QString sourceKind;
        QString sourceKey;
        QString targetKind;
        QString targetKey;
    };

    struct NodeVolume {
        QString kind;
        QString key;
        float volume = 1.0f;
    };

    struct PinnedNode {
        QString kind;
        QString key;
    };

    struct Session {
        QList<QString> virtualDevices; // Anzeigenamen, in Erstellungsreihenfolge
        QList<LinkRule> linkRules;
        QList<NodeVolume> nodeVolumes;
        QList<PinnedNode> pinnedNodes;
    };

    Session load() const;
    void save(const Session &session) const;

private:
    QString filePath() const;
};
