"use strict";
const $ = (id) => document.getElementById(id);
let paused = false,
  latest = null,
  prior = null,
  rate = 0;
const num = (n) => Number(n || 0).toLocaleString();
function element(tag, text, cls) {
  const e = document.createElement(tag);
  if (text !== undefined) e.textContent = text;
  if (cls) e.className = cls;
  return e;
}
function details(p) {
  $("detail-title").textContent = p.name || p.address || p.kind;
  const root = $("detail-fields");
  root.replaceChildren();
  const field = (label, value) => {
    const d = element("div", undefined, "field");
    d.append(element("b", label), element("pre", String(value)));
    root.append(d);
  };
  field("Packet", `${p.kind} · channel ${p.channel} · CRC verified`);
  if (p.address)
    field("Address", `${p.address} (${p.random ? "random" : "public"})`);
  if (p.scanner) field("Scanner", p.scanner);
  for (const f of p.fields) field(f.label, f.value);
  if (p.note) field("Scope", p.note);
  if (p.malformed_ad)
    field(
      "Advertising data",
      "Malformed AD field length; raw packet CRC is valid.",
    );
  field("Unwhitened PDU", p.raw);
  field("CRC bytes", p.crc);
  $("detail").showModal();
}
function render(s) {
  $("total").textContent = num(s.total);
  $("devices").textContent = num(
    new Set(s.packets.map((p) => p.address).filter(Boolean)).size,
  );
  $("rate").textContent = rate.toFixed(1);
  const cards = $("channels");
  cards.replaceChildren();
  let active = 0,
    lag = 0,
    lost = 0;
  for (const ch of [37, 38, 39]) {
    const c = s.channels[ch];
    active += c.active ? 1 : 0;
    lag = Math.max(lag, c.lag_ms);
    lost += c.dropped;
    const div = element("div", undefined, "channel");
    const top = element("div", undefined, "channel-top");
    top.append(
      element("span", `CHANNEL ${ch}`),
      element(
        "span",
        c.active
          ? c.lag_ms > 100
            ? "● Catching up"
            : "● Listening"
          : "○ Waiting",
        c.active && c.lag_ms <= 100 ? "dot" : "waiting",
      ),
    );
    const h = element("h2", String({ 37: 2402, 38: 2426, 39: 2480 }[ch]));
    h.append(element("small", " MHz"));
    const bottom = element("div", undefined, "channel-bottom");
    bottom.append(
      element("span", `${num(c.packets)} packets`),
      element(
        "span",
        s.decoder === "dsp"
          ? `DSP ${ch - 37}`
          : s.decoder === "cpu"
            ? "Router CPU"
            : "Waiting",
      ),
    );
    div.append(top, h, bottom);
    cards.append(div);
  }
  $("connection").textContent =
    active === 3
      ? "● Three channels connected"
      : s.stopped
        ? "Receiver stopped"
        : "Receiver starting / unavailable";
  const notices = [];
  if (lost)
    notices.push(`${num(lost)} input samples lost across channel readers.`);
  if (lag > 100)
    notices.push(`Processing is ${Math.round(lag)} ms behind reception.`);
  if (s.errors) notices.push(`${num(s.errors)} receiver errors.`);
  $("notice").hidden = !notices.length;
  $("notice").textContent = notices.join(" ");
  $("history").textContent =
    `${num(s.retained)} retained · showing the latest 500 · ${paused ? "display paused" : "updates live"}`;
  if (paused) return;
  const term = $("search").value.toLowerCase(),
    rows = $("rows");
  rows.replaceChildren();
  const packets = s.packets.filter((p) =>
    JSON.stringify(p).toLowerCase().includes(term),
  );
  for (const p of packets) {
    const tr = element("tr");
    const channel = element("td");
    channel.append(element("span", String(p.channel), "badge"));
    const data =
      p.name ||
      p.fields.find((f) => f.type === "0xFF")?.value ||
      p.fields[0]?.value ||
      "—";
    tr.append(
      element("td", new Date(p.time * 1000).toLocaleTimeString()),
      channel,
      element("td", p.address || "Extended header"),
      element("td", data),
      element("td", p.kind, "kind"),
      element("td", "↗"),
    );
    tr.addEventListener("click", () => details(p));
    rows.append(tr);
  }
  $("empty").hidden = packets.length > 0;
  $("empty").textContent = term
    ? "No matching packets."
    : "Listening for CRC-valid advertisements. Packets will appear here as they arrive.";
}
$("pause").onclick = () => {
  paused = !paused;
  $("pause").textContent = paused ? "Resume display" : "Pause display";
  if (latest) render(latest);
};
$("close").onclick = () => $("detail").close();
$("search").oninput = () => {
  if (latest) render(latest);
};
async function poll() {
  try {
    const r = await fetch("/api/state");
    if (!r.ok) throw new Error();
    const s = await r.json();
    if (prior && s.time > prior.time)
      rate = Math.max(0, (s.total - prior.total) / (s.time - prior.time));
    prior = s;
    latest = s;
    render(s);
  } catch {
    $("connection").textContent = "Connection interrupted";
  }
  setTimeout(poll, 1000);
}
poll();
