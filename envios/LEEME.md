# Envíos

Acá va todo lo que llega en los restos, ordenado por año:

```
envios/
  1986/
    juegos/       zips de juegos de DOS (como los de archive.org)
    programas/    zips de programas de DOS
    revistas/     PDF, o CBZ/ZIP con las páginas en imágenes
    videos/       MP4 o WebM
    musica/       una carpeta por disco o casete, con sus temas (MP3, OGG o M4A)
    decoracion/   JPG o PNG: posters (o cuadros si el nombre dice cuadro, marco o foto)
  1987/
    ...
  enlaces.txt     direcciones web, para no tener que bajar nada
  curar.json      qué años y cuántas cosas de cada tipo elige la acción de GitHub
  espejo.json     qué direcciones ya se copiaron al repositorio (se arma solo)
  dosgames.json   el catálogo de dosgames.com (se arma solo)
  envios.json     el índice (se arma solo)
```

- **Tapas:** una imagen con el mismo nombre que el archivo es su tapa. `Karateka.zip` + `Karateka.jpg` llega en su caja con esa tapa.
- **Índice:** después de agregar archivos corré `python3 herramientas/indexar.py`. En GitHub se corre solo al subir (ver el README).
- **Música:** cada disco o casete va en su propia carpeta dentro de `musica/` (por ejemplo `musica/Soda_Stereo_Signos/01.mp3`, `02.mp3`…). Suena entero, tema por tema. Con archive.org pasa lo mismo: se toma el ítem completo.
- **Software:** los juegos y programas los ponés vos acá o en `enlaces.txt`. Desde el juego no se puede cargar software propio: hay que arreglárselas con lo que llega.
- **Nombres:** el nombre del archivo es el que se ve en el juego (`Pole_Position_1986.zip` → POLE POSITION).

## enlaces.txt

Una línea por cosa: año, tipo y dirección, y opcionalmente un nombre. Las líneas que empiezan con `#` se ignoran.

```
1986 videos https://archive.org/details/ComputerChronicles1986 Computer Chronicles
1986 juegos https://archive.org/details/msdos_Karateka_1986 Karateka
1987 revistas https://archive.org/details/byte-magazine-1987-01 Byte enero 87
1986 musica https://archive.org/details/algun-disco
```

Tipos: `juegos`, `programas`, `revistas`, `videos`, `musica`, `decoracion` (también acepta `peliculas`).

Con una dirección de archive.org basta la página del ítem (`/details/...`). Los videos se ven en la pantalla del proyector con el reproductor de archive.org, y las revistas en el lector con su visor. Los juegos, programas y revistas los copia `herramientas/espejo.py` (la acción de GitHub lo corre sola) a `envios/<año>/<tipo>/`, y el juego usa esa copia. También sirven zips de sitios de juegos de DOS, por ejemplo:

```
1983 juegos https://dosgames.com/files/3demon.zip 3-Demon
```

## Elegir solos

`herramientas/dosgames.py` toma de dosgames.com los juegos más vistos de cada año y `herramientas/curar.py` hace lo mismo con archive.org para videos, revistas, música e imágenes. Los dos leen `curar.json`:

```
"desde": 1984, "hasta": 1995       los años
"pais": "Argentina"                prioriza cosas de ese país o idioma
"por_año": { "juegos": 4, ... }    cuántas cosas de cada tipo por año
"juegos_de_archive": false         true suma juegos de archive.org (tienen derechos de autor)
```

Nunca repiten un título que ya esté en `enlaces.txt`.

Desde el juego, con el botón azul debajo del sonar, también se pueden registrar señales de videos, revistas, música y decoración (no de software). Esas quedan guardadas solo en tu navegador.

## Truco

Escribí `iddqd` mientras flotás por el módulo: aparece una cápsula en la esclusa con lo mejor que haya (lo más visto de archive.org para ese año).
