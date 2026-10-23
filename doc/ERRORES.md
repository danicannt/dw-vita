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
| Batería de eventos: 8 crashes y 7 "HANG" restantes | **A repetir** | Los 8 crashes (DOOA, `reincarnatePartner`, `EVL_applyEvolution`) siguen a un "map N still busy, changing anyway": la batería cambió de mapa en mitad de una evolución o un renacimiento. Una muerte y renacimiento forzados (`para vida 0`) funcionan. Ahora la batería congela edad y vida del compañero. Los HANG de TEND01/02 eran la película del final; `smoke.py` ya cuenta sus fotogramas como progreso. Falta repetir los mapas 19 35 47 75 78 101 109 139 153 213 236 237 254 |
| Mapas "busy" en las baterías (el evento no termina solo pulsando X) | Sin investigar | YAKA12/13/24/25, FRZL17, OMOC01, GOMI02, FACT11B, MGEN14-16, MIHA05/06… Pueden ser eventos que esperan otra entrada |
| Evolución al objetivo 62 (wEAG) | Fuera de alcance | No tiene modelo en el disco; tampoco se puede alcanzar en PS1 |

## Auditoría del código nativo (23-10-2026)

Repaso de todo el juego buscando lo que en PS1 "funcionaba" por lo que
quedaba en los registros y en nativo es basura. Herramientas: los avisos
de GCC que el build normal apaga (`-Wmaybe-uninitialized` a -O1/-O2/-O3/-Og,
`-Wreturn-type`, `-Waggressive-loop-optimizations`), un comprobador de
número de argumentos (prototipos reales con `-aux-info` contra las llamadas
en GIMPLE) y, para cada caso, el código MIPS original sacado del disco con
los mapas de símbolos del decomp. Resultado: 24 llamadas con argumentos de
menos o de más, 112 avisos de `return`, 74 de variables sin valor y 2 bucles
fuera de rango; casi todos son imposibles en la práctica, estos eran reales:

### Felicitar / reñir
- **Llamada sin argumento:** en la versión US `handlePraiseScold()` se llama
  sin `state`, que decide entre felicitar (15) y reñir (4). En PS1 `$a0`
  aún tenía `partnerState`; en nativo era basura, así que una felicitación
  podía contar como riña o al revés (`partner.c`).
- **Riña merecida:** `happinessDelta` no se asignaba. En PS1 estaba en `$s1`,
  que tenía el índice del bucle de `tickObjects` (el hueco del compañero, un
  número pequeño que se sumaba a la felicidad); en nativo GCC lo convertía
  en −10, como una riña injusta. Ahora se suma ese índice
  (`DW1_TICK_SLOT`, `world_object.c`).

### Efectos de combate
- `BTL_/STD_/VS_startEFE` no devolvía nada en el camino normal. En PS1
  `runEFESlotScript` acaba con un salto (no una llamada) a `runEFEScript`,
  así que devolvía su resultado, que se guarda en `efeSubEffect` y luego
  recibe `stopEFESubEffect`. En nativo era basura: probablemente el origen
  del crash de `BTL_stopEFESubEffect` con `EFE_DATA_PTR` NULL.
- `BTL_/STD_/VS_startQueuedMove`: al terminar la carga de un ataque final,
  `tech` se leía sin valor (`MOVE_DATA[tech]`, lectura fuera de rango en
  nativo). En PS1 era `$s3`, el 4.º argumento del que llama (índice del
  luchador o el ataque); ahora se le pasa ese valor.
- `BTL_initializeBuffTrails` escribía 8 elementos en un array de 1: en PS1
  sin efecto visible (el propio decomp lo documenta), en nativo pisaba las
  variables de después. Ahora escribe solo el suyo.

### Menús y texto
- `renderString` con 8 de 9 argumentos (14 llamadas: cartas y tabla de
  evoluciones): faltaba `shadow`. En PS1 esa posición de la pila siempre
  tenía algo distinto de cero, así que el texto llevaba sombra.
- `flipTextboxPage` no devolvía nada al cambiar de página; en PS1 `$v0`
  tenía siempre un puntero (distinto de cero): "ocupado", el menú esperaba
  un fotograma. Ahora devuelve 1.
- `drawString2`: un espacio de ancho completo avanza `adv`, el ancho del
  carácter anterior; al principio del texto no tenía valor. Empieza en 12.
- Tabla de evoluciones: tres bucles recorren 62 casillas de un array de 61.
  La 62 (en PS1, datos del array siguiente: fuera de pantalla y con una
  paleta inválida) se salta, y la búsqueda del cursor sigue acabando en 62
  si no hay coincidencia.

### Otros
- `VS__tickVSInput` (versus) llamaba a `VS___tickVSInput()` sin el jugador;
  en PS1 era el `instanceId` del tick (0 o 1).
- `setActiveAnim(0xc)` con 1 de 2 argumentos (scripts que animan un NPC):
  en PS1 el segundo era distinto de cero y pausa la IA del NPC con script
  12; ahora se pasa el número de animación.
- Curling (`KAR_setOpponentShot`): sin piedra libre escribía en un puntero
  sin asignar (en PS1, el `$s2` del que llama); ahora se salta el tiro.
- Renacimiento (`DOOA_renderRebirth`): `rand() % range` con `range` que a
  veces sale 0. El `div` de MIPS no salta y deja el dividendo como resto
  (en PS1 daba `rand()`); en PC era SIGFPE. Lo encontró la batería de
  eventos en GIAS01. En la Vita (ARM) la división por cero da 0 sin cerrar
  el juego, pero el resultado tampoco era el de PS1.

Baterías después de la auditoría: battles 0-60, 60/60 limpios; events 0-80,
los mismos crashes de renacimiento que antes (OGRE02, GIAS06A) y GIAS09 pasa
de crash a HANG.

Descartados tras mirarlos (todos los caminos devuelven o asignan, o solo
fallarían con un script mal formado): `_sin`, `getCardAmount`,
`calculateElementBonus`, `getEntityIndex`, `getItemOptionColor`, las
operaciones `*Variable` de la VM de efectos, `selectRandomTargetEntity`,
`getHighest/LowestScoredMove`, `EVL_tickEvoSequence`, `matrixToEuler`,
`partnerTickCollision`, `handleRestore`, `scriptCompareValue`, `playBGM`…

## Divisiones por cero (23-10-2026)

El `div`/`divu` del R3000 no salta con divisor 0: el cociente es -1 (1 si el
dividendo es negativo; 0xFFFFFFFF sin signo) y el resto es el dividendo. En
el ejecutable y los overlays solo llevan la comprobación `break 7` las
librerías PsyQ (libsnd, libspu, libgs, `ratan2`); todo el código del juego
divide sin comprobar. En nativo, x86 genera SIGFPE (el manejador de
`crash_handler.c` lo emula y registra el sitio) y la libgcc de la Vita
devuelve INT_MAX/INT_MIN (0 para 0/0) como cociente y 0 como resto, así que
allí el resultado era distinto aunque no se cerrara el juego. Se repasaron
las ~310 divisiones y restos con divisor no constante de `game/src`; los
sitios donde el divisor puede ser 0 usan ahora `dw1_div`/`dw1_divu` y los
nuevos `dw1_mod`/`dw1_modu` (`port/include/dw1_native.h`) bajo
`#ifdef DW1_NATIVE`:

- **Proyecciones detrás de la cámara:** el GTE (y PSY-Z) satura SZ3 a 0
  cuando el punto queda detrás, y estas funciones dividen por la
  profundidad antes (o en vez) de comprobarla: `renderSprite` y `addFXPrim`
  (`effect_common.c`; nubes, partículas de entidad, `EFERotateVector`),
  `renderParticleFX` (`efe.c`), `renderOverworldItem` (`item.c`),
  `renderFleeBubble` (`btl.c`), `*_renderPoisonBubble` y
  `*_renderPlayerMarker` (combate, torneo y versus). `*_renderProjectedSprite`
  comprueba el flag de error del GTE, pero no cubre SZ3 = 1..3 (otz 0).
- **VM de efectos:** las operaciones `*_divide/moduloInt{8,16,32}Variable`,
  `*_divideVectorByScalar` y `*_divideVectors` dividen por lo que pase el
  script. La batería de eventos lo encontró en STIC02
  (`BTL_divideInt32Variable`). Las de 16 bits dividen por el registro
  truncado a 16 bits, como el MIPS original. `*_centerTransformOnEntities`
  divide por el número de entidades en el mapa, que puede ser 0.
- **Contraataque en combate** (`BTL/STD_tickAttackHits`, `VS_resolveAttack`):
  `speed / DIGIMON_DATA[type].level`, y el nivel es 0 en los tipos de NPC
  (Jijimon, Cherrymon, Hagurumon…). Si uno de ellos recibe un golpe con
  un contraataque aprendido, en PS1 da -1 y no contraataca nunca; en la Vita
  daba INT_MAX y contraatacaba siempre.
- **Movimientos de script:** `tickEntityMoveTo`, `tickEntityMoveToAxis` y
  `tickMoveObjectTo` dividen la distancia por un byte del script, y
  `renderFadeOut` divide por `frames + 1` (`fadeToWhite` con 0xFFFF).
- **Distancias 0:** `getClosestTileOffScreen` e `isLinearPathBlocked`
  (`map.c`) con los dos extremos en la misma casilla (el bucle no se ejecuta,
  solo cambiaba la excepción en x86); `*_setCameraOrbit` y
  `*_getFighterDistance` (torneo y versus) con los dos luchadores en el
  mismo punto.
- **Selección del versus:** `listPage %= listPageCount` con una tarjeta sin
  Digimon.

Descartados tras mirarlos (divisor constante, con guarda o imposible con los
datos del disco): los colores del modo alambre, `lerp`, las interpolaciones
`*_interpolate*` (comprueban `lo == hi`), `_atan`, `matrixToEuler*`,
`getDistance`, los fundidos de CLUT y las piezas de DOO2/EVL/ENDI (siempre
con constantes), `boneCount - 1` (mínimo 3 huesos en `DIGIMON_DATA`),
`radius >> 1` (radio mínimo 90), las proporciones de radio y altura de EVL,
`finisherGoal` (3000 - velocidad), los pesos del curling, `DOOA_updateShards`
(`axisDistance` ya se fuerza a 1), la curva de `DOOA_tickDissolve`,
`DOOA_tickRebirth` (`level` >= 10), `getRookie/ChampionEvolutionTarget`
(`statCount` solo sería 0 con un objetivo sin requisitos, y en las rutas de
evolución solo los hay de bebé a pequeño, que usan otra función), la
barra de desplazamiento de `renderUIBoxStatic` (solo se dibuja con más
filas que las visibles), `MONEY / SHOP_ITEM_PRICE` (solo si el coste supera
el dinero), `steps` del cambio de hora (>= 2), las proyecciones con guarda
(`murd`, `dooa`, `fish`, `butterfly`, `bubble`), las funciones `__garbage__`
(no se llaman) y los valores de los ficheros de efectos que no han dado 0
en las baterías (`numFrames`, `startOffset`, `renderParallelLines`).

Batería de eventos (0-254, 240 mapas), antes y después: desaparecen las 3
divisiones por cero (`BTL_divideInt32Variable` en TROP02, KODA08 y STIC02).
Siguen los crashes de antes, de renacimiento y DOOA (`DOOA_updateShards` y
`GsMapModelingData` en `reincarnatePartner`): 8 antes, 7 después (esta vez
GIAS09 sale como HANG). Hay 12 HANG en las dos pasadas (ahora pasa
GCAN04_2 y se añade GIAS09).

## Arreglados en esta tanda

### Bocadillo de caca con textura basura
- **Causa:** `GsSortSprite` del port dibuja un sprite escalado como polígono
  con el borde derecho en `u + w`. El último fotograma de la caca está en
  `u = 0xF0` con 16 de ancho: `0x100` se convierte en 0 en un byte y el
  polígono recorría la página de textura al revés.
- **Arreglo:** el borde se limita a 255 (`port/src/libgs/system.c`). Antes,
  en el mismo sitio: el atajo sin escala ignoraba el pivote `(mx, my)` y el
  bocadillo saltaba.

### Crashes de la batería de eventos (`DW1_SMOKE=events`)
- **Diálogo de un NPC que no está:** `getSpeakerName` leía
  `ENTITY_TABLE[0xff]` (`script_value.c`).
- **NPC ausente en giros y descargas:** `getEntityFromScriptId` no tenía
  `return` si no encontraba el NPC (en PS1 devolvía lo que quedase en `v0`);
  ahora devuelve NULL, y `tickLookAtEntity`, `tickEntitySetRotation` y
  `scriptUnloadEntity` lo comprueban.
- **Combate contra el hueco 0 (el tamer):** la instrucción de combate pelea
  contra quien inició el diálogo; una sección alcanzada de otra forma cargaba
  datos de combate para un tipo sin ellos. Ahora se salta y cuenta como
  victoria (`script_interp.c`).
- **Minijuego KAR (curling), versión US:** su segunda cadena de menú es NULL
  también en el disco original; `drawString(NULL)` ya no dibuja nada (en PS1
  la RAM baja empieza con ceros) (`font.c`).
- **Fin de combate sin efectos cargados:** `BTL_stopEFESubEffect` con
  `EFE_DATA_PTR` a NULL (`battle_effect.c`).

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
- **`DW1_SMOKE=events`**: entra en cada mapa y lanza una a una todas las
  secciones de su script (diálogos, tiles de evento, escenas), como si
  hablaras con el NPC dueño de cada una, pulsando X para avanzar. Los
  "event N busy" suelen ser escenas sin su contexto de historia; los crashes
  indican la sección.
- **`DW1_VRAMWATCH=x,y,w,h`**: registra quién escribe en ese rectángulo de
  VRAM (`LoadImage`/`MoveImage`/`ClearImage`), para encontrar texturas
  pisadas.
- **`DW1_SMOKE=walk`**: entra en cada mapa, camina en cuadrado para que la
  cámara se desplace y guarda capturas `walk_<mapa>_<n>.ppm` en
  `DW1_DUMP_DIR`. Es la batería que encontró los crashes de TUNN04, ROOM20 e
  ICSA08.
- Baterías existentes: `DW1_SMOKE=maps|battles|evos` con `tools/smoke.py`.
- **`tools/audit/`** (la auditoría de arriba):
  - `isoextract.py IMAGEN.bin DESTINO`: saca los ficheros del disco
    (`SLUS_010.32`, `*_REL.BIN`).
  - `psxdis.py external/dw_decomp DESTINO función...`: desensambla la función
    original (necesita `pip install capstone`). Sirve para saber qué valor
    tenía en PS1 una variable sin asignar o un argumento que falta.
  - `arity.py`: compara el número de argumentos de cada llamada con la
    definición, a partir de `-aux-info` y `-fdump-tree-gimple` de cada unidad.

## Resultado de las baterías (19-10-2026)

- maps: 223 limpios, 18 "busy", 0 crashes.
- walk: 0 crashes tras los arreglos (antes: TUNN04, ICSA08, ROOM20).
- evos: 64/66 (62 se salta).
