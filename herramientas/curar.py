#!/usr/bin/env python3
"""Elige en archive.org cosas representativas de cada año y las suma a envios/enlaces.txt.

"Representativo" = lo más visto de ese año dentro de colecciones curadas (juegos y programas
de DOS, películas y TV, revistas, discos completos, posters y fotos), sin repetir títulos.
La configuración está en envios/curar.json. Uso:  python3 herramientas/curar.py
"""
import json, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from archivo import AO_RE, buscar, clave_titulo, leer_enlaces, metadata

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'envios')
CONSULTAS = {
    'juegos': 'collection:softwarelibrary_msdos_games',
    'programas': 'collection:softwarelibrary_msdos',
    'videos': 'mediatype:movies AND (collection:prelinger OR collection:classic_tv OR collection:feature_films OR collection:classic_tv_commercials OR collection:television OR collection:moviesandfilms OR collection:computerchronicles)',
    'revistas': 'mediatype:texts AND (collection:magazine_rack OR collection:computermagazines OR collection:magazines)',
    'musica': 'mediatype:audio AND (collection:netlabels OR collection:audio_music OR collection:album_recordings OR subject:album OR subject:cassette)',
    'decoracion': 'mediatype:image AND (subject:poster OR subject:postcard OR subject:photograph OR collection:flickrcommons)',
}
IDIOMAS = {'Argentina': 'spa OR Spanish', 'Uruguay': 'spa OR Spanish', 'Chile': 'spa OR Spanish', 'México': 'spa OR Spanish', 'España': 'spa OR Spanish',
           'Brasil': 'por OR Portuguese', 'Estados Unidos': 'eng OR English', 'Reino Unido': 'eng OR English', 'Francia': 'fre OR French', 'Italia': 'ita OR Italian', 'Alemania': 'ger OR German', 'Japón': 'jpn OR Japanese'}

def es_disco(ident):
    try:
        n = sum(1 for f in metadata(ident).get('files', []) if str(f.get('name', '')).lower().endswith(('.mp3', '.ogg')) and 'mp3' in str(f.get('format', '')).lower() + f['name'].lower())
        return 4 <= n <= 40
    except Exception:
        return False

def main():
    cfg = json.load(open(os.path.join(RAIZ, 'curar.json'), encoding='utf-8'))
    enlaces_path = os.path.join(RAIZ, 'enlaces.txt')
    previos = leer_enlaces(enlaces_path)
    urls = {e['url'] for e in previos}
    titulos = {clave_titulo(e['nombre']) for e in previos if e['nombre']}
    pais = cfg.get('pais') or ''
    nuevas = []
    for año in range(int(cfg['desde']), int(cfg['hasta']) + 1):
        for tipo, cuantos in cfg.get('por_año', {}).items():
            if tipo not in CONSULTAS or not cuantos:
                continue
            if tipo == 'juegos' and not cfg.get('juegos_de_archive'):
                continue  # los juegos salen de dosgames.py (freeware y shareware)
            ya = sum(1 for e in previos if e['año'] == str(año) and e['tipo'] == tipo)
            falta = int(cuantos) - ya
            if falta <= 0:
                continue
            base = CONSULTAS[tipo] + f' AND year:{año}'
            filtros = [base]
            if pais and tipo not in ('juegos', 'programas'):
                filtros.insert(0, base + f' AND (language:({IDIOMAS.get(pais, "")}) OR country:"{pais}" OR subject:"{pais}")' if pais in IDIOMAS else base + f' AND (country:"{pais}" OR subject:"{pais}")')
            for q in filtros:
                if falta <= 0:
                    break
                try:
                    docs = buscar(q)
                except Exception as ex:
                    print('búsqueda fallida', año, tipo, ex); continue
                for d in docs:
                    if falta <= 0:
                        break
                    url = 'https://archive.org/details/' + d['identifier']
                    t = d.get('title') or d['identifier']
                    t = t[0] if isinstance(t, list) else t
                    k = clave_titulo(t)
                    if url in urls or k in titulos:
                        continue
                    if tipo == 'musica' and not es_disco(d['identifier']):
                        continue
                    urls.add(url); titulos.add(k); falta -= 1
                    nuevas.append(f'{año} {tipo} {url} {str(t).strip()[:60]}')
    if nuevas:
        with open(enlaces_path, 'a', encoding='utf-8') as f:
            f.write('\n# --- elegido por herramientas/curar.py ---\n' + '\n'.join(nuevas) + '\n')
    print(f'curar: {len(nuevas)} cosas nuevas en enlaces.txt')

if __name__ == '__main__':
    main()
