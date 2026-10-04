#!/usr/bin/env python3
"""Elige juegos de DOS de dosgames.com (freeware y shareware, que se pueden repartir) y los suma
a envios/enlaces.txt. Después espejo.py los copia al repositorio para que el juego los lea.

Por cada año de curar.json toma los más vistos de ese año, sin repetir títulos.
El catálogo del sitio se guarda en envios/dosgames.json y se renueva cada 30 días.
Uso:  python3 herramientas/dosgames.py
"""
import html, json, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from archivo import clave_titulo, leer_enlaces, pedir

SITIO = 'https://dosgames.com'
RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'envios')
CATALOGO = os.path.join(RAIZ, 'dosgames.json')
LICENCIAS = ('Freeware', 'Shareware', 'Full Version', 'Demo')
FICHA = re.compile(r'Views:\s*([\d,.]+)\s*((?:19|20)\d\d)\s*(' + '|'.join(LICENCIAS) + ')', re.I)
ENLACE = re.compile(r'href="(?:https?://(?:www\.)?dosgames\.com)?/game/([\w\-]+)/?"[^>]*>(.*?)</a>', re.I | re.S)

def texto(h):
    return html.unescape(re.sub(r'\s+', ' ', re.sub(r'<[^>]+>', ' ', h))).strip()

def leer_pagina(n):
    h = pedir(f'{SITIO}/listing.php?sort=popular&page={n}').decode('utf-8', 'replace')
    total = re.search(r'Page\s+\d+\s+of\s+(\d+)', texto(h))
    marcas = [m for m in ENLACE.finditer(h) if texto(m.group(2))]
    juegos = []
    for i, m in enumerate(marcas):
        fin = marcas[i + 1].start() if i + 1 < len(marcas) else len(h)
        f = FICHA.search(texto(h[m.end():fin]))
        if f:
            juegos.append({'slug': m.group(1), 'nombre': texto(m.group(2)), 'vistas': int(re.sub(r'\D', '', f.group(1)) or 0),
                           'año': int(f.group(2)), 'licencia': f.group(3).title()})
    return juegos, int(total.group(1)) if total else n

def catalogo():
    if os.path.exists(CATALOGO):
        c = json.load(open(CATALOGO, encoding='utf-8'))
        if time.time() - c.get('fecha', 0) < 30 * 86400 and c.get('juegos'):
            return c['juegos']
    juegos, n, total = {}, 1, 1
    while n <= total and n <= 400:
        try:
            pag, total = leer_pagina(n)
        except Exception as ex:
            print('no se pudo leer la página', n, ex); break
        for j in pag:
            juegos[j['slug']] = j
        n += 1
        time.sleep(0.5)
    lista = sorted(juegos.values(), key=lambda j: (j['año'], -j['vistas']))
    with open(CATALOGO, 'w', encoding='utf-8') as f:
        json.dump({'fecha': int(time.time()), 'juegos': lista}, f, ensure_ascii=False, indent=1)
    print(f'dosgames: catálogo de {len(lista)} juegos')
    return lista

def zip_de(slug):
    h = pedir(f'{SITIO}/game/{slug}/').decode('utf-8', 'replace')
    m = re.search(r'href="((?:https?://(?:www\.)?dosgames\.com)?/files/[^"]+\.zip)"', h, re.I)
    if not m:
        return None
    u = html.unescape(m.group(1))
    return u if u.startswith('http') else SITIO + u

def main():
    cfg = json.load(open(os.path.join(RAIZ, 'curar.json'), encoding='utf-8'))
    cuantos = int(cfg.get('dosgames_por_año', cfg.get('por_año', {}).get('juegos', 0)))
    if not cuantos:
        print('dosgames: desactivado en curar.json'); return
    enlaces_path = os.path.join(RAIZ, 'enlaces.txt')
    previos = leer_enlaces(enlaces_path)
    urls = {e['url'] for e in previos}
    titulos = {clave_titulo(e['nombre']) for e in previos if e['nombre']}
    juegos = catalogo()
    nuevas = []
    for año in range(int(cfg['desde']), int(cfg['hasta']) + 1):
        falta = cuantos - sum(1 for e in previos if e['año'] == str(año) and e['tipo'] == 'juegos')
        for j in sorted((j for j in juegos if j['año'] == año), key=lambda j: -j['vistas']):
            if falta <= 0:
                break
            k = clave_titulo(j['nombre'])
            if k in titulos:
                continue
            try:
                u = zip_de(j['slug'])
            except Exception as ex:
                print('no se pudo abrir', j['slug'], ex); continue
            if not u or u in urls:
                continue
            urls.add(u); titulos.add(k); falta -= 1
            nuevas.append(f"{año} juegos {u} {j['nombre'][:60]}")
            time.sleep(0.5)
    if nuevas:
        with open(enlaces_path, 'a', encoding='utf-8') as f:
            f.write('\n# --- elegido de dosgames.com por herramientas/dosgames.py ---\n' + '\n'.join(nuevas) + '\n')
    print(f'dosgames: {len(nuevas)} juegos nuevos en enlaces.txt')

if __name__ == '__main__':
    main()
