#!/usr/bin/env python3
"""Copia a envios/ los juegos y programas (y opcionalmente revistas) de enlaces.txt.

archive.org no siempre deja que una página web baje sus archivos directo, así que el juego
los lee de esta copia, que vive en tu repositorio junto al juego.

Uso:  python3 herramientas/espejo.py            juegos y programas
      python3 herramientas/espejo.py --revistas  también las revistas en PDF
Después corré herramientas/indexar.py (la acción de GitHub hace las dos cosas sola).
"""
import json, os, re, sys, urllib.parse
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from archivo import AO_RE, elegir_archivo, leer_enlaces, metadata, pedir, restringido

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'envios')
MAX = 95 * 1024 * 1024  # GitHub no acepta archivos de más de 100 MB

def nombre_seguro(s):
    s = re.sub(r'[^\w\-]+', '_', s, flags=re.UNICODE).strip('_')
    return s[:60] or 'archivo'

def main():
    tipos = {'juegos', 'programas'} | ({'revistas'} if '--revistas' in sys.argv else set())
    espejo_path = os.path.join(RAIZ, 'espejo.json')
    espejo = json.load(open(espejo_path, encoding='utf-8')) if os.path.exists(espejo_path) else {}
    ya = {v['origen'] for v in espejo.values()}
    nuevos = fallas = 0
    for e in leer_enlaces(os.path.join(RAIZ, 'enlaces.txt')):
        if e['tipo'] not in tipos or e['url'] in ya:
            continue
        try:
            m = AO_RE.match(e['url'])
            if m:
                meta = metadata(m.group(1))
                if restringido(meta):
                    print('restringido, no se puede copiar:', e['url']); fallas += 1; continue
                f = elegir_archivo(meta, e['tipo'])
                if not f:
                    print('sin archivo útil:', e['url']); fallas += 1; continue
                if int(f.get('size') or 0) > MAX:
                    print('muy grande para GitHub:', e['url']); fallas += 1; continue
                url = 'https://archive.org/download/' + m.group(1) + '/' + urllib.parse.quote(f['name'])
                ext = os.path.splitext(f['name'])[1].lower()
                base = e['nombre'] or meta.get('metadata', {}).get('title') or m.group(1)
            else:
                url, ext, base = e['url'], os.path.splitext(urllib.parse.urlparse(e['url']).path)[1].lower() or '.zip', e['nombre'] or 'archivo'
            datos = pedir(url)
            if len(datos) > MAX:
                print('muy grande para GitHub:', e['url']); fallas += 1; continue
            rel = e['tipo'] + '/' + nombre_seguro(str(base)) + ext
            dest = os.path.join(RAIZ, e['año'], rel)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            with open(dest, 'wb') as out:
                out.write(datos)
            espejo[e['año'] + '/' + rel] = {'origen': e['url'], 'nombre': e['nombre'] or str(base)}
            nuevos += 1
            print('copiado:', rel, f'({len(datos) // 1024} KB)')
        except Exception as ex:
            print('no se pudo copiar', e['url'], '→', ex); fallas += 1
    with open(espejo_path, 'w', encoding='utf-8') as f:
        json.dump(espejo, f, ensure_ascii=False, indent=2)
        f.write('\n')
    print(f'espejo: {nuevos} nuevos, {fallas} sin copiar, {len(espejo)} en total')

if __name__ == '__main__':
    main()
