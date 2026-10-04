"""Funciones compartidas para hablar con archive.org (las usan espejo.py y curar.py)."""
import json, re, time, urllib.parse, urllib.request

UA = {'User-Agent': 'Orbita/1.0 (juego; herramientas de envios)'}
AO_RE = re.compile(r'^https?://(?:www\.)?archive\.org/(?:details|download|embed)/([^/?#]+)(/[^?#]*)?', re.I)
QUIERE = {
    'juegos': (r'\.zip$', r'\.jsdos$'),
    'programas': (r'\.zip$', r'\.jsdos$'),
    'revistas': (r'\.pdf$', r'\.cbz$'),
    'decoracion': (r'\.jpe?g$', r'\.png$'),
    'videos': (r'\.mp4$', r'\.webm$'),
    'musica': (r'\.mp3$', r'\.ogg$'),
}

def pedir(url, intentos=3):
    for i in range(intentos):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=60) as r:
                return r.read()
        except Exception as e:
            if i == intentos - 1:
                raise
            time.sleep(2 + i * 3)

def metadata(ident):
    return json.loads(pedir('https://archive.org/metadata/' + urllib.parse.quote(ident)))

def elegir_archivo(meta, cat):
    """El archivo que sirve para el tipo pedido: el zip más grande, el PDF, etc."""
    files = [f for f in meta.get('files', []) if f.get('name') and not re.search(r'(_meta|_files)\.xml$|__ia_thumb|\.torrent$', f['name'])]
    for pat in QUIERE.get(cat, (r'.',)):
        c = [f for f in files if re.search(pat, f['name'], re.I)]
        if c:
            return sorted(c, key=lambda f: -int(f.get('size') or 0))[0]
    return None

def restringido(meta):
    return bool(meta.get('is_dark')) or str(meta.get('metadata', {}).get('access-restricted-item', '')).lower() == 'true'

def buscar(q, filas=40, orden='downloads desc'):
    url = 'https://archive.org/advancedsearch.php?' + urllib.parse.urlencode(
        [('q', q), ('fl[]', 'identifier'), ('fl[]', 'title'), ('fl[]', 'downloads'), ('sort[]', orden), ('rows', filas), ('page', 1), ('output', 'json')])
    return json.loads(pedir(url)).get('response', {}).get('docs', [])

def clave_titulo(t):
    import unicodedata
    t = unicodedata.normalize('NFD', str(t or '')).encode('ascii', 'ignore').decode().lower()
    t = re.sub(r'\b(19|20)\d\d\b', '', t)
    return re.sub(r'[^a-z0-9]+', '', t)

def leer_enlaces(path):
    out = []
    try:
        lineas = open(path, encoding='utf-8').read().splitlines()
    except FileNotFoundError:
        return out
    for l in lineas:
        l = re.sub(r'#.*$', '', l).strip()
        m = re.match(r'^(\d{4})\s+(\S+)\s+(https?://\S+)\s*(.*)$', l)
        if m:
            out.append({'año': m.group(1), 'tipo': m.group(2).lower(), 'url': m.group(3), 'nombre': m.group(4).strip()})
    return out
