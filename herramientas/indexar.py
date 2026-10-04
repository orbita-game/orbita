#!/usr/bin/env python3
"""Arma envios/envios.json a partir de las carpetas envios/<año>/<tipo>/.

Uso:  python3 herramientas/indexar.py
Tipos: juegos, programas, revistas, videos, musica, decoracion.
Una imagen con el mismo nombre que un archivo (juego.zip + juego.jpg) se usa como tapa.
"""
import json, os, re, sys

RAIZ = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'envios')
TIPOS = {
    'juegos': ('.zip', '.jsdos', '.exe', '.com'),
    'programas': ('.zip', '.jsdos', '.exe', '.com'),
    'revistas': ('.pdf', '.cbz', '.zip'),
    'videos': ('.mp4', '.webm', '.m4v', '.ogv'),
    'musica': ('.mp3', '.ogg', '.m4a', '.wav', '.flac'),
    'decoracion': ('.jpg', '.jpeg', '.png', '.gif', '.webp'),
}
IMG = ('.jpg', '.jpeg', '.png', '.webp')

def main():
    anios = {}
    espejo_path = os.path.join(RAIZ, 'espejo.json')
    espejo = json.load(open(espejo_path, encoding='utf-8')) if os.path.exists(espejo_path) else {}
    if not os.path.isdir(RAIZ):
        sys.exit('No encuentro la carpeta envios/')
    for anio in sorted(os.listdir(RAIZ)):
        if not re.fullmatch(r'\d{4}', anio):
            continue
        cats = {}
        for tipo, exts in TIPOS.items():
            d = os.path.join(RAIZ, anio, tipo)
            if not os.path.isdir(d):
                continue
            nombres = sorted(os.listdir(d))
            bases = {os.path.splitext(n)[0].lower(): n for n in nombres if n.lower().endswith(IMG)}
            lista = []
            # música: cada subcarpeta es un disco o casete completo, con sus temas en orden
            if tipo == 'musica':
                for n in nombres:
                    sub = os.path.join(d, n)
                    if os.path.isdir(sub) and not n.startswith('.'):
                        pistas = sorted(p for p in os.listdir(sub) if p.lower().endswith(exts))
                        if pistas:
                            e = {'archivo': tipo + '/' + n, 'nombre': n.replace('_', ' '), 'pistas': [tipo + '/' + n + '/' + p for p in pistas]}
                            lista.append(e)
            for n in nombres:
                base, ext = os.path.splitext(n)
                if n.startswith('.') or ext.lower() not in exts or os.path.isdir(os.path.join(d, n)):
                    continue
                # en decoración todas las imágenes son cosas; en el resto, una imagen gemela es la tapa
                if tipo != 'decoracion' and ext.lower() in IMG:
                    continue
                e = {'archivo': tipo + '/' + n}
                copia = espejo.get(anio + '/' + tipo + '/' + n)
                if copia:  # copia de algo de enlaces.txt: el juego usa esta en vez de la original
                    e['origen'] = copia['origen']
                    if copia.get('nombre'):
                        e['nombre'] = copia['nombre']
                tapa = bases.get(base.lower())
                if tipo != 'decoracion' and tapa:
                    e['tapa'] = tipo + '/' + tapa
                lista.append(e if len(e) > 1 else e['archivo'])
            if lista:
                cats[tipo] = lista
        if cats:
            anios[anio] = cats
    salida = os.path.join(RAIZ, 'envios.json')
    with open(salida, 'w', encoding='utf-8') as f:
        json.dump({'años': anios}, f, ensure_ascii=False, indent=2)
        f.write('\n')
    total = sum(len(v) for c in anios.values() for v in c.values())
    print(f'envios.json: {total} cosas en {len(anios)} años')

if __name__ == '__main__':
    main()
