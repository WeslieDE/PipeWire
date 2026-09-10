// Standalone browser mock of the future QWebChannel bridge (GraphBridge, M7).
// Same call shape as the real bridge so app.js does not need to change when
// mock.js is swapped for the real qwebchannel.js-backed object.
(function () {
  "use strict";

  let idCounter = 1;
  const nextId = () => idCounter++;

  const GLYPHS = {
    music: "music",
    browser: "browser",
    game: "game",
    mic: "mic",
    headphones: "headphones",
    speaker: "speaker",
    capture: "capture",
    wave: "wave",
  };

  const nodes = [
    { id: nextId(), role: "source-app", name: "spotify", description: "Spotify", appName: "Spotify", glyph: GLYPHS.music, isVirtual: false, pinned: true, volume: 0.8, muted: false },
    { id: nextId(), role: "source-app", name: "firefox", description: "Firefox", appName: "Firefox", glyph: GLYPHS.browser, isVirtual: false, pinned: true, volume: 0.6, muted: false },
    { id: nextId(), role: "source-app", name: "steam-game", description: "Roughscale", appName: "Roughscale", glyph: GLYPHS.game, isVirtual: false, pinned: true, volume: 1.0, muted: false },
    { id: nextId(), role: "source-device", name: "usb-mic", description: "USB Microphone", appName: "", glyph: GLYPHS.mic, isVirtual: false, pinned: true, volume: 0.9, muted: false },
    { id: nextId(), role: "source-app", name: "discord-in", description: "Discord", appName: "Discord", glyph: GLYPHS.music, isVirtual: false, pinned: false, volume: 1.0, muted: false },

    { id: nextId(), role: "sink-device", name: "headset", description: "USB Headset", appName: "", glyph: GLYPHS.headphones, isVirtual: false, pinned: true, volume: 0.85, muted: false },
    { id: nextId(), role: "sink-device", name: "speakers", description: "Speakers", appName: "", glyph: GLYPHS.speaker, isVirtual: false, pinned: true, volume: 0.5, muted: false },
    { id: nextId(), role: "sink-app", name: "obs", description: "OBS Studio", appName: "OBS Studio", glyph: GLYPHS.capture, isVirtual: false, pinned: false, volume: 1.0, muted: false },
    { id: nextId(), role: "sink-device", name: "mixpipe-virtual-1", description: "Stream Mix", appName: "", glyph: GLYPHS.wave, isVirtual: true, pinned: true, volume: 1.0, muted: false },
  ];

  let linkId = 1;
  const links = [
    { id: linkId++, outputNodeId: nodeByName("spotify"), inputNodeId: nodeByName("headset") },
    { id: linkId++, outputNodeId: nodeByName("steam-game"), inputNodeId: nodeByName("headset") },
    { id: linkId++, outputNodeId: nodeByName("steam-game"), inputNodeId: nodeByName("mixpipe-virtual-1") },
    { id: linkId++, outputNodeId: nodeByName("usb-mic"), inputNodeId: nodeByName("mixpipe-virtual-1") },
  ];

  function nodeByName(name) {
    return nodes.find((n) => n.name === name).id;
  }

  const listeners = {
    nodeAdded: [], nodeRemoved: [], linkAdded: [], linkRemoved: [], volumeChanged: [],
  };

  function emit(event, payload) {
    listeners[event].forEach((cb) => cb(payload));
  }

  window.MixPipeBridge = {
    getNodes() {
      return nodes.filter((n) => n.pinned).map((n) => ({ ...n }));
    },
    getAvailableNodes(role, callback) {
      const wantSource = role === "source";
      const result = nodes.filter((n) => !n.pinned && (wantSource ? n.role.startsWith("source") : n.role.startsWith("sink")));
      if (callback) callback(result);
      return result;
    },
    getLinks() {
      return links.map((l) => ({ ...l }));
    },
    pinNode(id) {
      const n = nodes.find((x) => x.id === id);
      if (n) {
        n.pinned = true;
        emit("nodeAdded", { ...n });
      }
    },
    unpinNode(id) {
      const n = nodes.find((x) => x.id === id);
      if (n) {
        n.pinned = false;
        emit("nodeRemoved", id);
      }
    },
    createVirtualDevice(displayName) {
      const n = {
        id: nextId(), role: "sink-device", name: "mixpipe-virtual-" + idCounter,
        description: displayName, appName: "", glyph: GLYPHS.wave, isVirtual: true,
        pinned: true, volume: 1.0, muted: false,
      };
      nodes.push(n);
      emit("nodeAdded", { ...n });
      return n.id;
    },
    removeVirtualDevice(id) {
      const idx = nodes.findIndex((n) => n.id === id);
      if (idx >= 0) {
        nodes.splice(idx, 1);
        emit("nodeRemoved", id);
      }
    },
    createLink(outputNodeId, inputNodeId) {
      if (links.some((l) => l.outputNodeId === outputNodeId && l.inputNodeId === inputNodeId)) {
        return;
      }
      const l = { id: linkId++, outputNodeId, inputNodeId };
      links.push(l);
      emit("linkAdded", { ...l });
    },
    removeLink(outputNodeId, inputNodeId) {
      const idx = links.findIndex((l) => l.outputNodeId === outputNodeId && l.inputNodeId === inputNodeId);
      if (idx >= 0) {
        const [removed] = links.splice(idx, 1);
        emit("linkRemoved", removed);
      }
    },
    setVolume(nodeId, volume) {
      const n = nodes.find((x) => x.id === nodeId);
      if (n) {
        n.volume = volume;
        emit("volumeChanged", { nodeId, volume });
      }
    },
    setMute(nodeId, muted) {
      const n = nodes.find((x) => x.id === nodeId);
      if (n) {
        n.muted = muted;
        emit("volumeChanged", { nodeId, volume: n.volume, muted });
      }
    },
    on(event, cb) {
      if (listeners[event]) listeners[event].push(cb);
    },
  };
})();
