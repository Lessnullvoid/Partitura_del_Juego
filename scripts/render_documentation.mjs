// Development utility: Node.js and the marked package are required to render.
// The delivered HTML is self-contained and needs neither dependency.
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import { createRequire } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
const require = createRequire(import.meta.url);
const { marked } = await import(pathToFileURL(require.resolve('marked')).href);
const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dir = path.join(root, 'docs');
const catalog = JSON.parse(fs.readFileSync(path.join(dir, 'DOCUMENTOS_ENTREGA.json')));
const esc = s => s.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const id = name => name.replace(/\.md$/, '').toLowerCase();
const hash = crypto.createHash('sha256');
for (const name of ['DOCUMENTOS_ENTREGA.json', ...catalog.map(d => d.file)]) {
  hash.update(name + '\0'); hash.update(fs.readFileSync(path.join(dir, name)));
}
let group = '';
const nav = catalog.map(d => {
  const label = group === d.group ? '' : `<p class="nav-group">${esc(d.group)}</p>`;
  group = d.group;
  return label + `<a href="#${id(d.file)}">${esc(d.title)}</a>`;
}).join('\n');
const articles = catalog.map(d => {
  let source = fs.readFileSync(path.join(dir, d.file), 'utf8');
  source = source.replace(/```mermaid\n([\s\S]*?)```/g, (_, chart) => {
    const labels = [...chart.matchAll(/[A-Z]\["([^\"]+)"\]/g)].map(m => m[1]);
    return '<ol class="flow">' + labels.map(s => `<li>${esc(s)}</li>`).join('') + '</ol>\n';
  });
  let body = marked.parse(source);
  body = body.replace(/href="([^"#]+\.md)(#[^"]*)?"/g, (all, name) => {
    const entry = catalog.find(d => d.file === name);
    return entry ? `href="#${id(entry.file)}"` : all;
  });
  return `<article id="${id(d.file)}" tabindex="-1"><p class="eyebrow">${esc(d.group)}</p>${body}<footer>Partitura del Juego · Documentación para el museo</footer></article>`;
}).join('\n');
const html = `<!doctype html>
<html lang="es"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="pdj-documentation-sha256" content="${hash.digest('hex')}">
<title>Partitura del Juego · Guías para el museo</title>
<style>
:root{color-scheme:light;--ink:#202723;--muted:#59665f;--paper:#faf9f4;--accent:#315c49;--line:#d9ded6}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:17px/1.7 system-ui,-apple-system,sans-serif}
a{color:var(--accent);text-underline-offset:3px}a:focus-visible,button:focus-visible{outline:3px solid #8b5a24;outline-offset:3px}
.skip{position:absolute;left:10px;top:-80px;background:white;padding:10px}.skip:focus{top:10px}
header{padding:25px 32px;border-bottom:1px solid var(--line);display:flex;justify-content:space-between;gap:20px;align-items:center}
.brand{font-weight:650;letter-spacing:.01em}.subtitle{font-size:13px;color:var(--muted)}button{font:inherit;font-size:14px;border:1px solid var(--line);background:white;border-radius:5px;padding:9px 14px;cursor:pointer}
.layout{display:grid;grid-template-columns:265px minmax(0,1fr);max-width:1440px;margin:auto}nav{padding:25px 22px;position:sticky;top:0;align-self:start;max-height:100vh;overflow:auto}
.nav-group{font-size:11px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);margin:22px 10px 7px}nav a{display:block;text-decoration:none;font-size:14px;line-height:1.5;padding:8px 10px;border-radius:4px}nav a[aria-current=page]{background:#e3ebe3;color:#193f2b;font-weight:650}
main{min-width:0;padding:42px 56px 80px;border-left:1px solid var(--line)}article{max-width:860px;margin:auto;outline:none}article[hidden]{display:none}.eyebrow{font-size:12px;text-transform:uppercase;letter-spacing:.1em;color:var(--muted)}h1{font-family:Georgia,serif;font-size:39px;line-height:1.16;font-weight:500;margin:12px 0 30px}h2{font-size:23px;line-height:1.35;margin:38px 0 15px}h3{font-size:19px;margin:28px 0 12px}p{margin:0 0 18px}li{padding-left:3px;margin:8px 0}table{border-collapse:collapse;width:100%;font-size:14px;line-height:1.55;margin:23px 0 28px}td,th{padding:12px;border:1px solid var(--line);vertical-align:top;text-align:left}th{background:#eaf0e8;font-weight:650}pre{white-space:pre-wrap;overflow-wrap:anywhere;background:#eef0e9;padding:18px;border-radius:5px;font-size:13px;line-height:1.6}code{font-size:.85em;overflow-wrap:anywhere}footer{margin-top:48px;padding-top:20px;border-top:1px solid var(--line);font-size:12px;color:var(--muted)}.flow{display:flex;gap:9px;list-style:none;padding:0;font-size:13px;line-height:1.4}.flow li{flex:1;background:#e8eee5;border-top:3px solid #52785e;padding:12px;margin:0}.flow li+li:before{content:'→ ';font-weight:700}
@media(max-width:800px){header{padding:18px}.layout{display:block}nav{position:static;max-height:none;padding:12px 18px}main{border-left:0;border-top:1px solid var(--line);padding:28px 20px}h1{font-size:31px}.flow{display:block}.flow li{margin:8px 0}table{font-size:12px}td,th{padding:7px}}
@media print{@page{size:A4;margin:18mm}body{background:white;font-size:10.5pt;line-height:1.5}header,nav,.skip{display:none}.layout{display:block}main{padding:0;border:0}h1{font-size:25pt}h2{font-size:15pt;break-after:avoid}h3{break-after:avoid}p,li{orphans:3;widows:3}table{font-size:9pt}tr,pre{break-inside:avoid}a{color:inherit}footer{font-size:8pt}}
</style></head><body><a class="skip" href="#contenido">Ir al contenido</a>
<header><div><div class="brand">Partitura del Juego</div><div class="subtitle">Educación · Mediación · Instalación</div></div><button type="button" id="print">Imprimir esta guía</button></header>
<div class="layout"><nav aria-label="Guías del museo">${nav}</nav><main id="contenido">${articles}</main></div>
<script>
const articles=[...document.querySelectorAll('article')];
function show(focus){const key=location.hash.slice(1);const selected=articles.find(a=>a.id===key)||articles[0];articles.forEach(a=>a.hidden=a!==selected);document.querySelectorAll('nav a').forEach(a=>{if(a.hash==='#'+selected.id)a.setAttribute('aria-current','page');else a.removeAttribute('aria-current')});document.title=selected.querySelector('h1').textContent+' · Guías del museo';if(focus){selected.focus({preventScroll:true});window.scrollTo(0,0)}}
addEventListener('hashchange',()=>show(true));show(false);document.getElementById('print').addEventListener('click',()=>window.print());
</script></body></html>`;
fs.writeFileSync(path.join(dir,'LECTURA.html'),html);
console.log(`Rendered ${catalog.length} guides in docs/LECTURA.html`);
