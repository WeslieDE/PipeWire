// Adapts the real C++ GraphBridge (exposed via QWebChannel as "graphBridge")
// to the exact same window.MixPipeBridge shape that web/mock.js provides for
// standalone browser prototyping, so app.js does not need to know which one
// is actually backing it.
//
// One real difference has to be bridged here: QWebChannel calls that return a
// value are asynchronous in JS (they take a callback), but app.js calls
// getNodes()/getLinks() synchronously and uses the result immediately. So
// this adapter keeps a small local cache, seeded once via the async getters
// and then kept in sync purely from the nodeAdded/nodeRemoved/linkAdded/
// linkRemoved events - getNodes()/getLinks() just read that cache.
(function () {
  "use strict";

  new QWebChannel(qt.webChannelTransport, function (channel) {
    const backend = channel.objects.graphBridge;

    const nodesById = new Map();
    const linksById = new Map();

    const listeners = {
      nodeAdded: [], nodeRemoved: [], linkAdded: [], linkRemoved: [], volumeChanged: [],
    };
    const emit = (event, payload) => listeners[event].forEach((cb) => cb(payload));

    backend.nodeAdded.connect((node) => {
      nodesById.set(node.id, node);
      emit("nodeAdded", node);
    });
    backend.nodeRemoved.connect((id) => {
      nodesById.delete(id);
      emit("nodeRemoved", id);
    });
    backend.linkAdded.connect((link) => {
      linksById.set(link.id, link);
      emit("linkAdded", link);
    });
    backend.linkRemoved.connect((link) => {
      linksById.delete(link.id);
      emit("linkRemoved", link);
    });
    backend.volumeChanged.connect((payload) => emit("volumeChanged", payload));

    window.MixPipeBridge = {
      getNodes: () => [...nodesById.values()],
      getAvailableNodes: () => [],
      getLinks: () => [...linksById.values()],
      createLink: (outputNodeId, inputNodeId) => backend.createLink(outputNodeId, inputNodeId),
      removeLink: (outputNodeId, inputNodeId) => backend.removeLink(outputNodeId, inputNodeId),
      setVolume: (nodeId, volume) => backend.setVolume(nodeId, volume),
      setMute: (nodeId, muted) => backend.setMute(nodeId, muted),
      createVirtualDevice: (displayName) => backend.createVirtualDevice(displayName),
      removeVirtualDevice: (nodeId) => backend.removeVirtualDevice(nodeId),
      on: (event, cb) => {
        if (listeners[event]) listeners[event].push(cb);
      },
    };

    backend.getNodes(function (nodes) {
      nodes.forEach((n) => nodesById.set(n.id, n));
      backend.getLinks(function (links) {
        links.forEach((l) => linksById.set(l.id, l));
        window.dispatchEvent(new Event("mixpipe-bridge-ready"));
      });
    });
  });
})();
