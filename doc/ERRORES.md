# Errores encontrados

Registro de los fallos del port nativo encontrados hasta ahora: qué se veía,
la causa y dónde está el arreglo. Casi todos son diferencias entre PS1 y
nativo (punteros, memoria contigua, orden de evaluación, lecturas de la
dirección 0) que en la consola no hacían daño. Los arreglos del juego van
bajo `#ifdef DW1_NATIVE`.

## Pendientes

| Síntoma | Estado | Pista |
|---|---|---|
| Cabeza o torso de un Digimon desaparece en ataques (cabeza de Agumon en el ataque a distancia) | Arreglo aplicado, **sin confirmar jugando** | `GsMulCoord3` guardaba en la matriz equivocada (ver abajo). Si sigue pasando: `DW1_BONECHECK=1` registra los huesos dibujados lejos de su raíz. Agumon no usa la animación de textura de la cara, así que esto no es el fallo de los ojos |
| Modelo del personaje repetido por la pantalla en el lago al moverse la cámara | Probablemente arreglado, **sin confirmar** | Lo más probable es el desbordamiento de `MAP_CLUTS`, que pisaba `MAP_TILE_X`, el que mueve el anillo de tiles del fondo. El recorrido `walk` por Dragon Eye Lake (mapa 6) ya no lo muestra |
| Filtro "pixel art" (`DW1_FILTER=pixel[:N]`) | **Roto, aparcado para el final** | Probado en ventana (build `build/linux32-gl`): parpadea, mezcla dos imágenes y el escenario sale casi negro. Probablemente muestrea el buffer que se está dibujando en vez del que se presenta, o la textura equivocada. Solo existe en el renderer `sdl3_gl`; el de por defecto (`sdl3_gpu`) lo ignora |
| Bocadillos de estado (hambre, etc.) se ven mal | Arreglo parcial, **falta que confirmes qué ves** | Arreglado: `GsSortSprite` del port dibujaba sin su pivote cuando la escala salía justo 1.0, y el bocadillo saltaba. La animación de hambre (carne → mordida → hueso) sale con sus fotogramas; sin una referencia de PS1 no se sabe qué más falla. Para reproducirlo: `bocadillo 0` en la consola |
| Mapas "busy" en las baterías (el evento no termina solo pulsando X) | Sin investigar | YAKA12/13/24/25, FRZL17, OMOC01, GOMI02, FACT11B, MGEN14-16, MIHA05/06… Pueden ser eventos que esperan otra entrada |
| Evolución al objetivo 62 (wEAG) | Fuera de alcance | No tiene modelo en el disco; tampoco se puede alcanzar en PS1 |

## Arreglados en esta tanda

### El juego iba al triple de velocidad
- **Causa:** el bucle principal se sincroniza con `VSync(3)` (esperar 3
  refrescos: 20 fps, como en PS1). PSY-Z presentaba un frame y esperaba uno
  solo, así que el juego corría a 60 fps. Al grabar o reproducir, el audio
  sincronizado por frame también iba al triple.
- **Arreglo:** `VSync(n)` presenta el frame y espera n refrescos
  (`patches/psyz/0005-vsync-n-vblanks.patch`). Medido con ventana: 20 vueltas
  por segundo. En turbo (`DW1_TURBO`) sigue sin esperar.

### Bloqueo en Drill Tunnel (cuadro "A / Drill Tunnel" que no se cierra)
- **Causa:** `setupMap` guarda un puntero por cada CLUT del `.TFS` del mapa en
  `MAP_CLUTS[3]`, pero MAYO01_2 y TWNA02 tienen 5 y MIST02 tiene 4. Los
  punteros de más pisaban `CURRENT_EXIT`, `MAP_TILE_X` y
  `CAMERA_REACHED_TARGET`, que quedaba con basura (-65). El script del cartel
  esperaba (`SCRIPT_OP_WAIT_FOR_ENTITY`) a que la cámara llegase al tamer y la
  espera no acababa nunca.
- Como lo que se colaba eran punteros, con ASLR el resultado cambiaba en
  cada ejecución, y con gdb (sin ASLR) el replay seguía otro camino.
- **Arreglo:** `MAP_CLUTS[16]` (`game/src/main/map.c`). Además, como
  salvaguarda, `checkCameraMovement` da por terminada una espera de cámara si
  no queda ningún objeto 0xfb1 que la mueva, y lo registra en el log.

### Ojos y caras repetidos o descolocados
- **Causa:** la instrucción 0x3000 de las animaciones (`tickAnimation`,
  `anim.c`) copia trozos de VRAM (ojos, bocas) con
  `MoveImage(&rect, x(**p), y(*(*p)++))`. El orden de evaluación de esos
  argumentos no está definido: el compilador de PS1 los evaluaba de izquierda
  a derecha (comprobado desensamblando `0x800C1E84` de SLUS_010.32) y GCC x86
  al revés, así que la X se leía del halfword siguiente. El tamer pegaba sus
  ojos en x=0 en vez de x=2.
- **Arreglo:** lecturas explícitas en orden. Es el único caso real que
  encuentra `-Wsequence-point` en todo el juego.

### Modelos que se desmontan por piezas en ataques
- **Causa:** en PSY-Q, `GsMulCoord2` deja `m1*m2` en m2 y `GsMulCoord3` en
  m1. La versión del port de `GsMulCoord3` lo dejaba en m2: al pedir la
  posición de un hueso para apuntar un efecto, se machacaban las matrices
  locales con valores de mundo.
- **Arreglo:** `port/src/libgs/math.c`.

### Crash al caminar por TUNN04 / ROOM20 (`LoadImage` desde NULL)
- **Causa (bug del juego original):** `updateTileRow` comprueba si está vacío
  el tile de la fila de arriba (`tiles[base + i]`) pero sube el de abajo
  (`tiles[base + start + i]`). Si ese está vacío, su imagen es NULL; la PS1
  copiaba lo que hubiera en la dirección 0.
- **Arreglo:** comprobar el tile que de verdad se sube (`map.c`).

### Crash en ICSA08 (`tickScript`, `SCRIPT_POINTER` = 1)
- **Causa:** un NPC habla con la sección 6 del script 100, que no existe.
  `getScriptSection` devuelve NULL y en PS1 se interpretaba RAM baja.
- **Arreglo:** `callScriptSection` no arranca una sección que no existe y lo
  registra en el log (`script_interp.c`).

## Arreglados antes (resumen)

| Fallo | Causa | Dónde |
|---|---|---|
| Crash al abrir el menú, en el gimnasio y en combates | Desreferencias de NULL que en PS1 leían o escribían RAM baja sin daño | `utils2.c`, `btl.c` y otros |
| Divisiones por cero (destello de evolución, HUD, burbujas, peces, partículas) | x86 genera una excepción; el R3000 devuelve -1/1 | `dw1_div`/`dw1_divu` y el manejador de SIGFPE en `crash_handler.c` |
| Texturas pisadas por lecturas de disco | Las lecturas encoladas terminaban después de una síncrona posterior | `dw1_file_queue_drain` en `file.c`/`file_queue.c` |
| Animaciones fuera de tabla (NPC tipo 165 en GCAN11) | Tabla de animaciones más corta | `anim.c` `startAnimation` |
| Evoluciones que crasheaban | `candidates[17]`, el buffer MMD de EVL vía `DW1_PS1_ADDR` | `evl.c` |
| Efectos de combate | Rutas EFEDAT, buffers vía `DW1_PS1_ADDR` | `efe.c`, `dooa.c` |
| Script de MAPHEAD truncado | Buffer de 0x6800 | `script_interp.c` |
| Iluminación rara | Constantes de comandos GTE en PSY-Z | `patches/psyz/0002` |
| Gráficos desplazados | Offset de dibujo con signo | `patches/psyz/0001` |
| Sin sonido / replays que divergían | Fallback de audio y SPU sincronizado por frame | `patches/psyz/0003` |
| Combate que no acababa / Agumon que no atacaba | Timing del SPU en tiempo real | SPU sincronizado por frame |

## Herramientas para encontrar más

- **Consola de debug** (`port/game/debug_console.c`): escribe comandos en la
  terminal donde lanzaste el juego; `ayuda` los lista. Sirve para dar
  objetos, dinero, cambiar de mapa, forzar combates, evoluciones, triggers,
  eventos de script, estados del Digimon (hambre, sueño, enfermo…),
  bocadillos, hora y día, y para hacer capturas. Los mismos comandos pueden
  ir en un fichero, `DW1_DEBUG_SCRIPT=fichero`, con líneas
  `<llamada al mando> <comando>`, para pruebas automáticas.

- **F9 mientras juegas** (`port/src/bug_mark.c`): anota el momento en el log
  (`dw1: MARCA n en llamada C`), guarda una captura y el estado del script. Con
  `grabar.sh nombre` va a `guiones/nombre.txt.marcas` y
  `marcas/nombre/marca_N.ppm`, así que el replay puede ir directo a ese punto.
- **`kill -USR1 <pid>`**: vuelca el estado (mapa, script, sección, instrucción
  en espera, cajas abiertas, objetos) sin parar el juego. Es útil porque gdb
  cambia el timing y el replay diverge.
- **`DW1_SMOKE=walk`**: entra en cada mapa, camina en cuadrado para que la
  cámara se desplace y guarda capturas `walk_<mapa>_<n>.ppm` en
  `DW1_DUMP_DIR`. Es la batería que encontró los crashes de TUNN04, ROOM20 e
  ICSA08.
- Baterías existentes: `DW1_SMOKE=maps|battles|evos` con `tools/smoke.py`.

## Resultado de las baterías (19-10-2026)

- maps: 223 limpios, 18 "busy", 0 crashes.
- walk: 0 crashes tras los arreglos (antes: TUNN04, ICSA08, ROOM20).
- evos: 64/66 (62 se salta).
