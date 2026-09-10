#include "VirtualDeviceManager.h"

#include "AudioGraph.h"
#include "PipeWireEngine.h"

#include <utility>

#include <pipewire/impl-module.h>
#include <pipewire/pipewire.h>

namespace {

QString escapeForModuleArgs(const QString &text)
{
    QString out = text;
    out.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    out.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return out;
}

} // namespace

VirtualDeviceManager::VirtualDeviceManager(PipeWireEngine *engine, QObject *parent)
    : QObject(parent)
    , m_engine(engine)
{
}

VirtualDeviceManager::~VirtualDeviceManager()
{
    m_engine->runLocked([this]() {
        for (auto *module : std::as_const(m_modules)) {
            pw_impl_module_destroy(module);
        }
    });
}

QString VirtualDeviceManager::createVirtualDevice(const QString &displayName)
{
    const QString handleId
        = QStringLiteral("%1%2").arg(QLatin1String(kVirtualNodeNamePrefix)).arg(++m_counter);
    const QString escapedName = escapeForModuleArgs(displayName);

    // capture.props = das, was MixPipe (und Apps, die "in" das virtuelle
    // Gerät reinspielen sollen) als Audio/Sink-Ziel sieht.
    // playback.props = die dazu gekoppelte Audio/Source, die z.B. in Discords
    // Mikrofon-Auswahl auftaucht - bewusst ohne target.object, damit sie sich
    // nicht automatisch irgendwohin verbindet.
    //
    // audio.rate/channels/position explizit setzen: ohne festes Format bleibt
    // der Node bis zur ersten Verbindung "suspended" ohne verhandeltes
    // Format - Apps, die (wie die meisten, z.B. Discord/Browser) Geräte über
    // die PulseAudio-Kompatibilitätsschicht auflisten, sehen ihn dann
    // schlicht nicht in ihrer Mikrofon-/Geräteauswahl (pactl/pipewire-pulse
    // braucht ein bekanntes Format, um das Gerät überhaupt zu melden).
    const QString args
        = QStringLiteral("{ node.description = \"%1\" "
                          "audio.rate = 48000 audio.channels = 2 audio.position = [ FL FR ] "
                          "capture.props = { node.name = \"%2_sink\" media.class = \"Audio/Sink\" } "
                          "playback.props = { node.name = \"%2_source\" media.class = \"Audio/Source\" } }")
              .arg(escapedName, handleId);
    const QByteArray argsUtf8 = args.toUtf8();

    struct pw_impl_module *module = nullptr;
    m_engine->runLocked([this, &module, &argsUtf8]() {
        module = pw_context_load_module(m_engine->context(), "libpipewire-module-loopback",
                                         argsUtf8.constData(), nullptr);
    });

    if (!module) {
        return {};
    }

    m_modules.insert(handleId, module);
    emit deviceCreated(handleId, displayName);
    return handleId;
}

void VirtualDeviceManager::removeVirtualDevice(const QString &handleId)
{
    auto it = m_modules.find(handleId);
    if (it == m_modules.end()) {
        return;
    }
    struct pw_impl_module *module = *it;
    m_modules.erase(it);

    m_engine->runLocked([module]() { pw_impl_module_destroy(module); });
    emit deviceRemoved(handleId);
}

QStringList VirtualDeviceManager::activeHandles() const
{
    return m_modules.keys();
}
