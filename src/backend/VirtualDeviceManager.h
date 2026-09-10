#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

struct pw_impl_module;

class PipeWireEngine;

// Legt virtuelle Geräte an (Sink+gekoppelte Source über
// libpipewire-module-loopback, geladen im eigenen pw_context) - das
// PipeWire-Äquivalent zu MixLines "virtuellen Aufnahmegeräten". Da das Modul
// im eigenen Prozess-Context geladen wird, verschwindet das virtuelle Gerät
// automatisch, sobald MixPipe beendet wird (siehe Architekturentscheidung:
// virtuelle Geräte sind an die GUI-Lebenszeit gebunden).
class VirtualDeviceManager : public QObject {
    Q_OBJECT

public:
    explicit VirtualDeviceManager(PipeWireEngine *engine, QObject *parent = nullptr);
    ~VirtualDeviceManager() override;

    // Gibt eine lokale Handle-ID zurück (nicht die PipeWire-Node-id - die
    // beiden tatsächlichen Nodes tauchen asynchron über den
    // Registry-Listener im AudioGraph auf, erkennbar an
    // AudioNode::isVirtual).
    QString createVirtualDevice(const QString &displayName);
    void removeVirtualDevice(const QString &handleId);

    QStringList activeHandles() const;

private:
    PipeWireEngine *m_engine;
    QMap<QString, struct pw_impl_module *> m_modules;
    int m_counter = 0;
};
