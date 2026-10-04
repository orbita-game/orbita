# Órbita

Un juego contemplativo en primera persona. Flotás en un módulo en órbita alrededor de la Tierra, usás computadoras de verdad (un DOS completo y una ROM BASIC), escuchás casetes, mirás películas en una pantalla y rescatás restos de satélites que explotaron hace décadas, con lo que hacían los terrestres ese año. No hay forma de perder.

Corre entero en el navegador: no hay que instalar nada.

## Publicarlo en GitHub Pages

1. Creá un repositorio nuevo en GitHub (por ejemplo `orbita`).
2. Subí todo el contenido de esta carpeta a la raíz del repositorio: `index.html`, `emu/`, `tex/`, `lib/`, `envios/`, `herramientas/`, `.github/`. Desde la web sirve "Add file → Upload files". La carpeta `.github` empieza con punto: si tu sistema la oculta, activá "mostrar archivos ocultos".
3. En el repositorio entrá a **Settings → Pages**. En "Build and deployment" elegí **Deploy from a branch**, rama `main`, carpeta `/ (root)`, y guardá.
4. Al minuto el juego queda en `https://<tu-usuario>.github.io/orbita/`. Ese enlace se puede compartir.
5. En **Settings → Actions → General → Workflow permissions** elegí "Read and write permissions": así GitHub rearma el índice de envíos solo cada vez que subís algo.
6. Para llenar los años solos: en la pestaña **Actions** elegí "Envíos" → **Run workflow**. GitHub busca juegos de DOS en dosgames.com y videos, revistas, música e imágenes en archive.org para los años de `envios/curar.json`, y copia los juegos, programas y revistas al repositorio. Tarda unos minutos.

Para probarlo en tu computadora sin subirlo, abrí una terminal en esta carpeta y corré `python3 -m http.server`; después entrá a `http://localhost:8000`. Abrir `index.html` con doble clic no alcanza, porque el navegador bloquea el emulador y las texturas desde archivos locales.

## Agregar contenido

Todo va en `envios/`, ordenado por año. Mirá `envios/LEEME.md`. Hay tres formas, y se pueden mezclar:

- **Archivos:** poné zips de juegos, PDF de revistas, videos, música o imágenes en `envios/<año>/<tipo>/`.
- **Direcciones web:** escribilas en `envios/enlaces.txt` (ítems de archive.org, zips de dosgames.com o cualquier archivo en la web). Los videos y revistas de archive.org se ven con el reproductor de archive.org, sin bajar nada. Los juegos, programas y revistas en PDF los copia la acción de GitHub al repositorio, porque archive.org y la mayoría de los sitios no dejan que otra página web los lea directo.
- **Elegidos solos:** la acción "Envíos" (paso 6) arma `enlaces.txt` con lo más visto de cada año. Los juegos salen de [dosgames.com](https://dosgames.com), que tiene freeware y shareware que se pueden repartir.
- **Al azar:** si en la pantalla de inicio está marcada la opción "Restos al azar de archive.org", cada cápsula se completa con cosas de ese año sacadas de archive.org (y la decoración, de Wikimedia Commons), priorizando la región que elegiste. Toda cápsula trae al menos algo para decorar: si no hay conexión, llega una postal del satélite.

Las cápsulas llegan de a tandas: 10 seguidas del mismo año, siempre entre 1983 y 1999. Cada una trae solo una parte de lo que hay para ese año. El sonar, junto a la escotilla del fondo, captura una por día; la red pasiva junta hasta tres cada dos días. Para probar todo rápido, agregá `?rapido` al final de la dirección: un día dura un minuto.

## Créditos y licencias

- Imágenes de la Tierra: NASA Visible Earth / Blue Marble (dominio público), tomadas del paquete `three-globe`.
- Emulador DOS: DOSBox-X compilado por js-dos (`emulators` 8.5.2), licencia GPL-2.0. Código fuente: https://github.com/js-dos/emulators y https://github.com/joncampbell123/dosbox-x
- three.js r128 (MIT), cargado desde cdnjs.
- pdf.js 3.11.174 (Apache-2.0), en `lib/`.
- Los juegos, revistas, películas y música no vienen con el juego: los aporta cada jugador, se leen desde archive.org o los copia la acción. Si tu repositorio es público, lo copiado queda publicado: los juegos de dosgames.com son freeware o shareware, pero los de archive.org (`msdos_*`) y muchas revistas tienen derechos de autor. Por eso `curar.json` trae `"juegos_de_archive": false`.
- three.js CSS3DRenderer (MIT), en `lib/`, para mostrar el reproductor de archive.org en la pantalla del proyector.
