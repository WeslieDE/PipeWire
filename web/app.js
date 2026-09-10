(function () {
  "use strict";

  // window.MixPipeBridge is only guaranteed to exist once "mixpipe-bridge-ready"
  // fires (mock.js sets it synchronously and fires immediately; the real
  // qtbridge.js sets it after the async QWebChannel handshake completes) -
  // see init() at the bottom of this file.
  let bridge;

  const ICONS = {
    music: '<svg viewBox="0 0 24 24" fill="none"><path d="M9 18V6.4l10-2v9.6" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/><circle cx="6.5" cy="18" r="2.5" stroke="currentColor" stroke-width="1.5"/><circle cx="16.5" cy="16" r="2.5" stroke="currentColor" stroke-width="1.5"/></svg>',
    browser: '<svg viewBox="0 0 24 24" fill="none"><circle cx="12" cy="12" r="8.5" stroke="currentColor" stroke-width="1.5"/><path d="M3.5 12h17M12 3.5c2.2 2.3 3.3 5.3 3.3 8.5s-1.1 6.2-3.3 8.5c-2.2-2.3-3.3-5.3-3.3-8.5S9.8 5.8 12 3.5Z" stroke="currentColor" stroke-width="1.4"/></svg>',
    game: '<svg viewBox="0 0 24 24" fill="none"><rect x="2.5" y="8" width="19" height="9.5" rx="4" stroke="currentColor" stroke-width="1.5"/><path d="M7 10.5v4.5M4.8 12.75h4.4" stroke="currentColor" stroke-width="1.4" stroke-linecap="round"/><circle cx="16" cy="11.3" r="1" fill="currentColor"/><circle cx="18.2" cy="13.8" r="1" fill="currentColor"/></svg>',
    mic: '<svg viewBox="0 0 24 24" fill="none"><rect x="9" y="3" width="6" height="11" rx="3" stroke="currentColor" stroke-width="1.5"/><path d="M6 11a6 6 0 0 0 12 0M12 17v3.5M9 20.5h6" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/></svg>',
    headphones: '<svg viewBox="0 0 24 24" fill="none"><path d="M4 14v-2a8 8 0 0 1 16 0v2" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/><rect x="3" y="13.5" width="4.5" height="6.5" rx="1.6" stroke="currentColor" stroke-width="1.5"/><rect x="16.5" y="13.5" width="4.5" height="6.5" rx="1.6" stroke="currentColor" stroke-width="1.5"/></svg>',
    speaker: '<svg viewBox="0 0 24 24" fill="none"><rect x="6" y="2.5" width="12" height="19" rx="2.5" stroke="currentColor" stroke-width="1.5"/><circle cx="12" cy="8.2" r="2.1" stroke="currentColor" stroke-width="1.4"/><circle cx="12" cy="15.2" r="3" stroke="currentColor" stroke-width="1.4"/></svg>',
    capture: '<svg viewBox="0 0 24 24" fill="none"><rect x="2.5" y="6" width="13" height="12" rx="2.2" stroke="currentColor" stroke-width="1.5"/><path d="m15.5 10.5 6-3.3v9.6l-6-3.3" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/></svg>',
    wave: '<svg viewBox="0 0 24 24" fill="none"><path d="M2.5 12h2.3l1.6-6 3 12 2.6-9 2 6.5 1.7-4.5 1.6 4 2-3h2.2" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/></svg>',
  };

  const canvasWrap = document.getElementById("canvasWrap");
  const canvas = document.getElementById("linkCanvas");
  const sourceList = document.getElementById("sourceList");
  const sinkList = document.getElementById("sinkList");
  const cardTemplate = document.getElementById("cardTemplate");
  const statusPill = document.getElementById("statusPill");

  const cardEls = new Map(); // nodeId -> card element
  let dragState = null; // { fromNodeId, fromSide, x, y }
  let selectedLink = null; // {outputNodeId, inputNodeId}

  const linkColorForNode = (nodeId) => `var(--color-link-${(nodeId % 5) + 1})`;

  function roleSide(role) {
    return role.startsWith("source") ? "source" : "sink";
  }

  function glyphForNode(node) {
    if (node.glyph) return node.glyph; // mock.js nodes already carry one
    if (node.isVirtual) return "wave";
    switch (node.role) {
      case "source-device":
        return "mic";
      case "sink-device":
        return /headphone|headset/i.test(node.description || "") ? "headphones" : "speaker";
      case "sink-app":
        return "capture";
      default:
        return "music";
    }
  }

  function render() {
    const nodes = bridge.getNodes();
    renderColumn(sourceList, nodes.filter((n) => roleSide(n.role) === "source"), "source");
    renderColumn(sinkList, nodes.filter((n) => roleSide(n.role) === "sink"), "sink");
    requestAnimationFrame(drawLinks);
  }

  function renderColumn(listEl, nodes, side) {
    const existingIds = new Set(nodes.map((n) => n.id));
    for (const [id, el] of [...cardEls.entries()]) {
      if (el.parentElement === listEl && !existingIds.has(id)) {
        el.remove();
        cardEls.delete(id);
      }
    }

    nodes.forEach((node, index) => {
      let card = cardEls.get(node.id);
      if (!card) {
        card = cardTemplate.content.firstElementChild.cloneNode(true);
        cardEls.set(node.id, card);
        wireCard(card, node.id);
      }
      card.dataset.nodeId = String(node.id);
      card.dataset.side = side;
      card.dataset.virtual = String(node.isVirtual);
      // Real nodes appear/disappear on their own as the underlying app or
      // device comes and goes - there's nothing to manage on them yet, so
      // only virtual devices (created by MixPipe itself) get a menu.
      card.querySelector(".card-menu").hidden = !node.isVirtual;
      card.querySelector(".card-icon").innerHTML = ICONS[glyphForNode(node)] || ICONS.wave;
      card.querySelector(".card-name").textContent = node.description || node.name;
      card.querySelector(".card-tag").hidden = !node.isVirtual;

      const muteBtn = card.querySelector(".mute-btn");
      muteBtn.setAttribute("aria-pressed", String(!!node.muted));

      const slider = card.querySelector(".volume-slider");
      const pct = Math.round((node.volume ?? 1) * 100);
      slider.value = String(pct);
      slider.style.setProperty("--fill", pct + "%");
      card.querySelector(".volume-value").textContent = pct + "%";

      const dot = card.querySelector(".connector-dot");
      dot.style.setProperty("--dot-color", side === "source" ? linkColorForNode(node.id) : "var(--color-ink-dim)");

      const targetIndex = [...listEl.children].indexOf(card);
      if (targetIndex !== index) {
        listEl.insertBefore(card, listEl.children[index] || null);
      }
    });
  }

  function wireCard(card, initialId) {
    const menuBtn = card.querySelector(".card-menu-btn");
    const popover = card.querySelector(".card-popover");
    menuBtn.addEventListener("click", (e) => {
      e.stopPropagation();
      popover.innerHTML =
        '<button class="card-popover-item is-danger" data-action="delete-virtual">Delete virtual device</button>';
      closeAllPopovers();
      popover.hidden = !popover.hidden;
      menuBtn.setAttribute("aria-expanded", String(!popover.hidden));
    });
    popover.addEventListener("click", (e) => {
      const action = e.target.closest("[data-action]")?.dataset.action;
      if (!action) return;
      const nodeId = Number(card.dataset.nodeId);
      if (action === "delete-virtual") bridge.removeVirtualDevice(nodeId);
      popover.hidden = true;
    });

    card.querySelector(".mute-btn").addEventListener("click", () => {
      const nodeId = Number(card.dataset.nodeId);
      const pressed = card.querySelector(".mute-btn").getAttribute("aria-pressed") === "true";
      bridge.setMute(nodeId, !pressed);
    });

    const slider = card.querySelector(".volume-slider");
    slider.addEventListener("input", () => {
      const nodeId = Number(card.dataset.nodeId);
      const pct = Number(slider.value);
      slider.style.setProperty("--fill", pct + "%");
      card.querySelector(".volume-value").textContent = pct + "%";
      bridge.setVolume(nodeId, pct / 100);
    });

    const dot = card.querySelector(".connector-dot");
    dot.addEventListener("mousedown", (e) => {
      e.preventDefault();
      const nodeId = Number(card.dataset.nodeId);
      dragState = { fromNodeId: nodeId, fromSide: card.dataset.side, x: e.clientX, y: e.clientY };
      dot.classList.add("is-dragging");
      document.body.style.cursor = "crosshair";
    });
  }

  function closeAllPopovers() {
    document.querySelectorAll(".card-popover").forEach((p) => (p.hidden = true));
    document.querySelectorAll(".card-menu-btn").forEach((b) => b.setAttribute("aria-expanded", "false"));
  }

  document.addEventListener("click", closeAllPopovers);

  // ---------- link drawing ----------

  function dotCenter(nodeId) {
    const card = cardEls.get(nodeId);
    if (!card) return null;
    const dot = card.querySelector(".connector-dot");
    const dr = dot.getBoundingClientRect();
    const wr = canvasWrap.getBoundingClientRect();
    return { x: dr.left + dr.width / 2 - wr.left, y: dr.top + dr.height / 2 - wr.top };
  }

  function bezierPath(p1, p2) {
    const dx = Math.max(60, Math.abs(p2.x - p1.x) * 0.5);
    const c1x = p1.x + dx;
    const c2x = p2.x - dx;
    return `M ${p1.x} ${p1.y} C ${c1x} ${p1.y}, ${c2x} ${p2.y}, ${p2.x} ${p2.y}`;
  }

  function bezierPoint(p1, p2, t) {
    const dx = Math.max(60, Math.abs(p2.x - p1.x) * 0.5);
    const c1 = { x: p1.x + dx, y: p1.y };
    const c2 = { x: p2.x - dx, y: p2.y };
    const mt = 1 - t;
    const x = mt ** 3 * p1.x + 3 * mt ** 2 * t * c1.x + 3 * mt * t ** 2 * c2.x + t ** 3 * p2.x;
    const y = mt ** 3 * p1.y + 3 * mt ** 2 * t * c1.y + 3 * mt * t ** 2 * c2.y + t ** 3 * p2.y;
    return { x, y };
  }

  const SVGNS = "http://www.w3.org/2000/svg";

  function drawLinks() {
    canvas.innerHTML = "";
    const links = bridge.getLinks();
    const hasSelection = selectedLink !== null;

    for (const link of links) {
      const p1 = dotCenter(link.outputNodeId);
      const p2 = dotCenter(link.inputNodeId);
      if (!p1 || !p2) continue;

      const isSelected = selectedLink && selectedLink.outputNodeId === link.outputNodeId && selectedLink.inputNodeId === link.inputNodeId;

      const g = document.createElementNS(SVGNS, "g");

      const path = document.createElementNS(SVGNS, "path");
      path.setAttribute("d", bezierPath(p1, p2));
      path.setAttribute("class", "link-path" + (isSelected ? " is-selected" : "") + (hasSelection && !isSelected ? " is-dimmed" : ""));
      path.setAttribute("stroke", linkColorForNode(link.outputNodeId));
      g.appendChild(path);

      const hit = document.createElementNS(SVGNS, "path");
      hit.setAttribute("d", bezierPath(p1, p2));
      hit.setAttribute("class", "link-hitbox");
      hit.addEventListener("mouseenter", () => path.classList.add("is-hover"));
      hit.addEventListener("mouseleave", () => path.classList.remove("is-hover"));
      hit.addEventListener("click", (e) => {
        e.stopPropagation();
        selectedLink = isSelected ? null : { outputNodeId: link.outputNodeId, inputNodeId: link.inputNodeId };
        drawLinks();
      });
      g.appendChild(hit);

      if (isSelected) {
        const mid = bezierPoint(p1, p2, 0.5);
        const del = document.createElementNS(SVGNS, "g");
        del.setAttribute("class", "link-delete");
        del.setAttribute("transform", `translate(${mid.x}, ${mid.y})`);
        del.innerHTML = '<circle r="9"/><path d="M-3.2 -3.2 L3.2 3.2 M3.2 -3.2 L-3.2 3.2"/>';
        del.addEventListener("click", (e) => {
          e.stopPropagation();
          bridge.removeLink(link.outputNodeId, link.inputNodeId);
          selectedLink = null;
        });
        g.appendChild(del);
      }

      canvas.appendChild(g);
    }

    if (dragState) {
      const from = dotCenter(dragState.fromNodeId);
      const wr = canvasWrap.getBoundingClientRect();
      const to = { x: dragState.x - wr.left, y: dragState.y - wr.top };
      const path = document.createElementNS(SVGNS, "path");
      path.setAttribute("class", "drag-path");
      path.setAttribute("d", dragState.fromSide === "source" ? bezierPath(from, to) : bezierPath(to, from));
      canvas.appendChild(path);
    }
  }

  document.addEventListener("click", (e) => {
    if (!e.target.closest(".link-hitbox") && !e.target.closest(".link-delete")) {
      if (selectedLink) {
        selectedLink = null;
        drawLinks();
      }
    }
  });

  window.addEventListener("mousemove", (e) => {
    if (!dragState) return;
    dragState.x = e.clientX;
    dragState.y = e.clientY;
    drawLinks();

    const overDot = document.elementFromPoint(e.clientX, e.clientY)?.closest(".connector-dot");
    document.querySelectorAll(".card.is-drag-target").forEach((c) => c.classList.remove("is-drag-target"));
    if (overDot) {
      const overCard = overDot.closest(".card");
      if (overCard && overCard.dataset.side !== dragState.fromSide) {
        overCard.classList.add("is-drag-target");
      }
    }
  });

  window.addEventListener("mouseup", (e) => {
    if (!dragState) return;
    document.querySelectorAll(".connector-dot.is-dragging").forEach((d) => d.classList.remove("is-dragging"));
    document.querySelectorAll(".card.is-drag-target").forEach((c) => c.classList.remove("is-drag-target"));
    document.body.style.cursor = "";

    const overDot = document.elementFromPoint(e.clientX, e.clientY)?.closest(".connector-dot");
    if (overDot) {
      const overCard = overDot.closest(".card");
      if (overCard && overCard.dataset.side !== dragState.fromSide) {
        const targetId = Number(overCard.dataset.nodeId);
        if (dragState.fromSide === "source") {
          bridge.createLink(dragState.fromNodeId, targetId);
        } else {
          bridge.createLink(targetId, dragState.fromNodeId);
        }
      }
    }
    dragState = null;
    drawLinks();
  });

  window.addEventListener("resize", () => requestAnimationFrame(drawLinks));
  sourceList.addEventListener("scroll", () => requestAnimationFrame(drawLinks));
  sinkList.addEventListener("scroll", () => requestAnimationFrame(drawLinks));

  // ---------- add dropdowns ----------

  document.querySelectorAll(".add-dropdown").forEach((dropdown) => {
    const side = dropdown.dataset.side;
    const btn = dropdown.querySelector(".add-btn");
    const menu = dropdown.querySelector(".add-menu");

    btn.addEventListener("click", (e) => {
      e.stopPropagation();
      const opening = menu.hidden;
      closeAllMenus();
      if (opening) openMenu();
    });

    function openMenu() {
      const available = bridge.getAvailableNodes(side);
      menu.innerHTML = "";
      if (available.length === 0) {
        const empty = document.createElement("div");
        empty.className = "add-menu-empty";
        empty.textContent = "Nothing new found.";
        menu.appendChild(empty);
      }
      for (const node of available) {
        const item = document.createElement("button");
        item.className = "add-menu-item";
        item.type = "button";
        item.innerHTML = `<span class="item-icon">${ICONS[node.glyph] || ICONS.wave}</span><span>${node.description || node.name}</span>`;
        item.addEventListener("click", () => {
          bridge.pinNode(node.id);
          menu.hidden = true;
          btn.setAttribute("aria-expanded", "false");
        });
        menu.appendChild(item);
      }
      if (side === "sink") {
        const divider = document.createElement("div");
        divider.className = "add-menu-divider";
        menu.appendChild(divider);
        const newWrap = document.createElement("div");
        newWrap.className = "add-menu-new";
        newWrap.innerHTML = '<input type="text" placeholder="Name for new virtual device…" />';
        const input = newWrap.querySelector("input");
        input.addEventListener("keydown", (e) => {
          if (e.key === "Enter" && input.value.trim()) {
            bridge.createVirtualDevice(input.value.trim());
            menu.hidden = true;
            btn.setAttribute("aria-expanded", "false");
          }
        });
        menu.appendChild(newWrap);
      }
      menu.hidden = false;
      btn.setAttribute("aria-expanded", "true");
    }
  });

  function closeAllMenus() {
    document.querySelectorAll(".add-menu").forEach((m) => (m.hidden = true));
    document.querySelectorAll(".add-btn").forEach((b) => b.setAttribute("aria-expanded", "false"));
  }

  document.addEventListener("click", closeAllMenus);

  // ---------- bridge events ----------

  function init() {
    bridge = window.MixPipeBridge;

    bridge.on("nodeAdded", render);
    bridge.on("nodeRemoved", render);
    bridge.on("linkAdded", () => requestAnimationFrame(drawLinks));
    bridge.on("linkRemoved", () => requestAnimationFrame(drawLinks));
    bridge.on("volumeChanged", ({ nodeId, volume, muted }) => {
      const card = cardEls.get(nodeId);
      if (!card) return;
      if (volume !== undefined) {
        const pct = Math.round(volume * 100);
        const slider = card.querySelector(".volume-slider");
        slider.value = String(pct);
        slider.style.setProperty("--fill", pct + "%");
        card.querySelector(".volume-value").textContent = pct + "%";
      }
      if (muted !== undefined) {
        card.querySelector(".mute-btn").setAttribute("aria-pressed", String(!!muted));
      }
    });

    render();
  }

  // window.MixPipeBridge may already be ready by the time this script runs
  // (mock.js sets it up synchronously) or not yet (qtbridge.js waits on an
  // async QWebChannel handshake) - handle both.
  if (window.MixPipeBridge) {
    init();
  } else {
    window.addEventListener("mixpipe-bridge-ready", init, { once: true });
  }
})();
