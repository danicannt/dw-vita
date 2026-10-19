/* Debug console: type commands in the terminal the game was started from.
 *
 * Commands run at the top of the next gameLoop (a safe point between frames)
 * and answer on stderr. "ayuda" lists them. The same commands can come from
 * a file, DW1_DEBUG_SCRIPT=file, one per line as "<pad call> <command>": each
 * runs once the pad has been read that many times (the clock recordings use),
 * so a test can e.g. warp and force a condition at fixed moments.
 *
 * Built with the game units (port/game is compiled like game/src) so it sees
 * the game's own structures. */
#include <ctype.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dw/clock.h>
#include <dw/entity.h>
#include <dw/item.h>
#include <dw/map.h>
#include <dw/params.h>
#include <dw/partner.h>
#include <dw/script.h>
#include <dw/tamer.h>

extern int16_t EVOLUTION_TARGET;
void setTrigger(uint16_t trigger);
void unsetTrigger(uint16_t trigger);
int32_t isTriggerSet(uint16_t trigger);
int32_t giveItem(uint8_t item, uint8_t amount);
void learnMove(int32_t move);
void unlockMedal(uint16_t medal);
void changeMap(int16_t mapId, int16_t exitId);
unsigned dw1_pad_calls(void);
void dw1_dump_state(void);
void dw1_dump_request(const char *name);
int32_t addConditionBubble(int32_t type, Entity *entity);

#define NUM_ITEMS 128

/* ---- PARTNER_PARA fields by name --------------------------------------- */

static const struct {
    const char *name;
    size_t offset;
    const char *what;
} para_fields[] = {
#define F(n, f, w) { n, offsetof(PartnerPara, f), w }
    F("comida", foodLevel, "hambre: 0 = hambriento"),
    F("energia", energyLevel, "energia"),
    F("cansancio", tiredness, "cansancio (>= 80 cansado)"),
    F("disciplina", discipline, "disciplina 0-100"),
    F("felicidad", happiness, "felicidad -100..100"),
    F("peso", weight, "peso"),
    F("vida", remainingLifetime, "vida restante (horas)"),
    F("edad", age, "edad (dias)"),
    F("errores", careMistakes, "errores de cuidado"),
    F("combates", battles, "combates"),
    F("caca", poopLevel, "nivel de caca"),
    F("virus", virusBar, "barra de virus"),
    F("enfermo", sicknessCounter, "contador de enfermedad"),
    F("evotimer", evoTimer, "temporizador de evolucion"),
#undef F
};

/* PARTNER_PARA.condition bits (partner.c / partner_impl.c) */
static const struct {
    const char *name;
    uint32_t bit;
} cond_bits[] = {
    { "sueno", 0x01 }, { "cansado", 0x02 }, { "hambre", 0x04 }, { "caca", 0x08 },
    { "infeliz", 0x10 }, { "herido", 0x20 }, { "enfermo", 0x40 },
};

extern int8_t TALKED_TO_ENTITY;

/* Start a section of the current map's script the way the game would: a
 * section that belongs to an NPC on the map runs as talking to it
 * (TALKED_TO_ENTITY set, param 1, like checkMapInteraction), so its battles
 * and look-at/unload instructions find that NPC. Returns the NPC's entity
 * slot or 0. Also used by the DW1_SMOKE=events battery. */
int dw1_debug_start_section(int section)
{
    int slot;

    for (slot = 2; slot < 10; slot++) {
        if (ENTITY_TABLE[slot] != NULL && NPC_ENTITIES[slot - 2].scriptId == section) {
            TALKED_TO_ENTITY = (int8_t)slot;
            callScriptSection(CURRENT_SCRIPT_ID, section, 1);
            return slot;
        }
    }
    TALKED_TO_ENTITY = 0;
    callScriptSection(CURRENT_SCRIPT_ID, section, 0);
    return 0;
}

/* Keep the partner's age and remaining life where they were at the first
 * call, so long test runs (DW1_SMOKE=events) do not wander into evolutions
 * or death scenes the battery then cuts short. */
void dw1_debug_freeze_partner(void)
{
    static int saved;
    static int16_t age, life;

    if (!saved) {
        saved = 1;
        age = PARTNER_PARA.age;
        life = PARTNER_PARA.remainingLifetime > 100 ? PARTNER_PARA.remainingLifetime : 100;
    }
    PARTNER_PARA.age = age;
    PARTNER_PARA.remainingLifetime = life;
}

/* ---- helpers ----------------------------------------------------------- */

static int ci_contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);

    for (; *hay; hay++) {
        size_t i;

        for (i = 0; i < n && hay[i] && tolower((unsigned char)hay[i]) == tolower((unsigned char)needle[i]); i++)
            ;
        if (i == n)
            return 1;
    }
    return n == 0;
}

/* a number, or the first name containing the text (items, digimon, maps) */
static int lookup(const char *arg, int count, const char *(*name)(int))
{
    char *end;
    long v = strtol(arg, &end, 0);
    int i;

    if (*arg != '\0' && *end == '\0')
        return (v >= 0 && v < count) ? (int)v : -1;
    for (i = 0; i < count; i++)
        if (name(i)[0] != '\0' && ci_contains(name(i), arg))
            return i;
    return -1;
}

static const char *item_name(int i) { return ITEM_PARA[i].name; }
static const char *digi_name(int i) { return DIGIMON_DATA[i].name; }
static const char *map_name(int i) { return MAP_ENTRIES[i].filename; }

static void list(const char *what, int count, const char *(*name)(int), const char *filter)
{
    int i, shown = 0;

    for (i = 0; i < count; i++) {
        if (name(i)[0] == '\0' || !ci_contains(name(i), filter ? filter : ""))
            continue;
        fprintf(stderr, "  %3d %-20.20s%s", i, name(i), ++shown % 4 == 0 ? "\n" : "");
    }
    fprintf(stderr, "%s(%d %s)\n", shown % 4 ? "\n" : "", shown, what);
}

static int16_t *stat_field(const char *n)
{
    BaseStats *b = &PARTNER_ENTITY.digimonEntity.stats.base;

    if (!strcmp(n, "hp")) return &b->hp;
    if (!strcmp(n, "mp")) return &b->mp;
    if (!strcmp(n, "off") || !strcmp(n, "ataque")) return &b->off;
    if (!strcmp(n, "def") || !strcmp(n, "defensa")) return &b->def;
    if (!strcmp(n, "vel") || !strcmp(n, "speed")) return &b->speed;
    if (!strcmp(n, "int") || !strcmp(n, "brain")) return &b->brain;
    return NULL;
}

static void help(void)
{
    fprintf(stderr,
        "dw1 debug, comandos:\n"
        "  estado                    mapa, script, tamer, cajas abiertas\n"
        "  objeto <id|nombre> [n]    dar objeto (n veces, por defecto 1)\n"
        "  objetos [texto]           listar objetos\n"
        "  dinero <n>                fijar bits\n"
        "  ir <mapa|nombre> [salida] cambiar de mapa       mapas [texto]: listar\n"
        "  combate [hueco]           luchar con el digimon del hueco 2-9 (el primero)\n"
        "  evo <id|nombre>           evolucionar al companero    digis [texto]: listar\n"
        "  trigger <id> [on|off]     ver / poner / quitar un trigger de la historia\n"
        "  reclutar <id|nombre>      marcar un digimon como reclutado en la ciudad\n"
        "  evento <script> <seccion> lanzar una seccion de script (forzar eventos)\n"
        "  stat <hp|mp|off|def|vel|int> <n>   fijar estadistica base;  stats: ver\n"
        "  curar                     vida y MP al maximo\n"
        "  para [campo] [valor]      ver / cambiar cuidados (comida, cansancio, peso, vida...)\n"
        "  cond [estado] [on|off]    ver / forzar estado: sueno cansado hambre caca\n"
        "                            infeliz herido enfermo\n"
        "  hora <h> [min]   dia <n>   ano <n>\n"
        "  tecnica <id>   medalla <id>   carta <id> <n>\n"
        "  bocadillo <0-7>           mostrar bocadillo: 0 hambre 1 caca 2 enfermo 3 sueno\n"
        "                            4 cansado 5 muriendo 6 herido 7 evolucionando\n"
        "  captura <nombre>          guardar la pantalla en <nombre>.ppm (DW1_DUMP_DIR)\n"
        "  F9 en la ventana          marca (captura + estado), ver grabar.sh\n");
}

/* ---- commands ---------------------------------------------------------- */

static void run(char *line)
{
    char *argv[8];
    int argc = 0, v;
    char *tok, *save;

    for (tok = strtok_r(line, " \t\r\n", &save); tok && argc < 8; tok = strtok_r(NULL, " \t\r\n", &save))
        argv[argc++] = tok;
    if (argc == 0)
        return;
#define IS(s) (!strcmp(argv[0], s))
#define ARG(i) (argc > (i) ? argv[i] : NULL)
#define NUM(i, d) (argc > (i) ? (int)strtol(argv[i], NULL, 0) : (d))

    if (IS("ayuda") || IS("help") || IS("?")) {
        help();
    } else if (IS("estado") || IS("state")) {
        dw1_dump_state();
        fprintf(stderr, "dw1: %02d:%02d dia %d ano %d, bits %d\n", HOUR, MINUTE, DAY, YEAR, MONEY);
    } else if (IS("objeto") || IS("item")) {
        if (!ARG(1) || (v = lookup(argv[1], NUM_ITEMS, item_name)) < 0) {
            fprintf(stderr, "dw1: objeto desconocido (prueba: objetos <texto>)\n");
            return;
        }
        {
            int n = NUM(2, 1), given = 0;

            while (n-- > 0 && giveItem((uint8_t)v, 1))
                given++;
            fprintf(stderr, "dw1: %d x %s (%d)%s\n", given, ITEM_PARA[v].name, v,
                    given ? "" : " - inventario lleno o al maximo");
        }
    } else if (IS("objetos") || IS("items")) {
        list("objetos", NUM_ITEMS, item_name, ARG(1));
    } else if (IS("digis")) {
        list("digimon", NUM_DIGIMON, digi_name, ARG(1));
    } else if (IS("mapas") || IS("maps")) {
        list("mapas", 255, map_name, ARG(1));
    } else if (IS("dinero") || IS("money")) {
        MONEY = NUM(1, MONEY);
        fprintf(stderr, "dw1: bits %d\n", MONEY);
    } else if (IS("ir") || IS("warp")) {
        if (!ARG(1) || (v = lookup(argv[1], 255, map_name)) < 0) {
            fprintf(stderr, "dw1: mapa desconocido (prueba: mapas <texto>)\n");
            return;
        }
        if (tamerGetState() != 0)
            fprintf(stderr, "dw1: aviso: el tamer esta ocupado (estado %d)\n", tamerGetState());
        fprintf(stderr, "dw1: ir a %d %.10s salida %d\n", v, MAP_ENTRIES[v].filename, NUM(2, 0));
        changeMap((int16_t)v, (int16_t)NUM(2, 0));
    } else if (IS("combate") || IS("battle")) {
        int slot = NUM(1, 0);

        if (slot == 0)
            for (slot = 2; slot < 10 && ENTITY_TABLE[slot] == NULL; slot++)
                ;
        if (slot < 2 || slot >= 10 || ENTITY_TABLE[slot] == NULL) {
            fprintf(stderr, "dw1: no hay digimon en ese hueco\n");
            return;
        }
        fprintf(stderr, "dw1: combate contra el hueco %d (%s)\n", slot, DIGIMON_DATA[ENTITY_TABLE[slot]->type].name);
        startBattle((int16_t)slot);
    } else if (IS("evo")) {
        if (!ARG(1) || (v = lookup(argv[1], NUM_DIGIMON, digi_name)) < 0) {
            fprintf(stderr, "dw1: digimon desconocido (prueba: digis <texto>)\n");
            return;
        }
        fprintf(stderr, "dw1: evolucion a %s (%d)\n", DIGIMON_DATA[v].name, v);
        EVOLUTION_TARGET = (int16_t)v;
        tamerSetState(6);
        partnerSetState(13);
    } else if (IS("trigger")) {
        v = NUM(1, -1);
        if (v < 0) {
            fprintf(stderr, "dw1: trigger <id> [on|off]\n");
            return;
        }
        if (ARG(2) && !strcmp(argv[2], "on"))
            setTrigger((uint16_t)v);
        else if (ARG(2) && !strcmp(argv[2], "off"))
            unsetTrigger((uint16_t)v);
        fprintf(stderr, "dw1: trigger %d = %d\n", v, isTriggerSet((uint16_t)v));
    } else if (IS("reclutar")) {
        if (!ARG(1) || (v = lookup(argv[1], NUM_DIGIMON, digi_name)) < 0) {
            fprintf(stderr, "dw1: digimon desconocido\n");
            return;
        }
        setTrigger((uint16_t)(0xc8 + v));
        fprintf(stderr, "dw1: %s reclutado (trigger %d)\n", DIGIMON_DATA[v].name, 0xc8 + v);
    } else if (IS("evento") || IS("script")) {
        if (argc < 3) {
            fprintf(stderr, "dw1: evento <script> <seccion> (el script del mapa actual es %d)\n", CURRENT_SCRIPT_ID);
            return;
        }
        if (IS_SCRIPT_PAUSED != 1)
            fprintf(stderr, "dw1: aviso: ya hay un script en marcha, se reemplaza\n");
        if (NUM(1, 0) == CURRENT_SCRIPT_ID) {
            int npc = dw1_debug_start_section(NUM(2, 0));

            fprintf(stderr, "dw1: evento script %d seccion %d%s\n", NUM(1, 0), NUM(2, 0),
                    npc ? " (hablando con su NPC)" : "");
        } else {
            callScriptSection(NUM(1, 0), NUM(2, 0), 0);
            fprintf(stderr, "dw1: evento script %d seccion %d\n", NUM(1, 0), NUM(2, 0));
        }
    } else if (IS("stat")) {
        int16_t *f = ARG(1) ? stat_field(argv[1]) : NULL;

        if (f == NULL || argc < 3) {
            fprintf(stderr, "dw1: stat <hp|mp|off|def|vel|int> <n>\n");
            return;
        }
        *f = (int16_t)NUM(2, 0);
        fprintf(stderr, "dw1: %s = %d\n", argv[1], *f);
    } else if (IS("stats")) {
        BaseStats *b = &PARTNER_ENTITY.digimonEntity.stats.base;

        fprintf(stderr, "dw1: %s hp %d/%d mp %d/%d off %d def %d vel %d int %d\n",
                DIGIMON_DATA[PARTNER_ENTITY.digimonEntity.entity.type].name,
                PARTNER_ENTITY.digimonEntity.stats.current.currentHP, b->hp,
                PARTNER_ENTITY.digimonEntity.stats.current.currentMP, b->mp,
                b->off, b->def, b->speed, b->brain);
    } else if (IS("curar") || IS("heal")) {
        PARTNER_ENTITY.digimonEntity.stats.current.currentHP = PARTNER_ENTITY.digimonEntity.stats.base.hp;
        PARTNER_ENTITY.digimonEntity.stats.current.currentMP = PARTNER_ENTITY.digimonEntity.stats.base.mp;
        fprintf(stderr, "dw1: curado\n");
    } else if (IS("para")) {
        size_t i;

        for (i = 0; i < sizeof(para_fields) / sizeof(para_fields[0]); i++) {
            int16_t *f = (int16_t *)((char *)&PARTNER_PARA + para_fields[i].offset);

            if (ARG(1) && strcmp(argv[1], para_fields[i].name))
                continue;
            if (ARG(2))
                *f = (int16_t)NUM(2, 0);
            fprintf(stderr, "  %-11s %6d  %s\n", para_fields[i].name, *f, para_fields[i].what);
        }
    } else if (IS("cond")) {
        size_t i;

        for (i = 0; ARG(1) && i < sizeof(cond_bits) / sizeof(cond_bits[0]); i++) {
            if (strcmp(argv[1], cond_bits[i].name))
                continue;
            if (ARG(2) && !strcmp(argv[2], "off"))
                PARTNER_PARA.condition &= ~cond_bits[i].bit;
            else
                PARTNER_PARA.condition |= cond_bits[i].bit;
        }
        fprintf(stderr, "dw1: estado %#x:", PARTNER_PARA.condition);
        for (i = 0; i < sizeof(cond_bits) / sizeof(cond_bits[0]); i++)
            if (PARTNER_PARA.condition & cond_bits[i].bit)
                fprintf(stderr, " %s", cond_bits[i].name);
        fprintf(stderr, "\n");
    } else if (IS("hora")) {
        HOUR = (int16_t)NUM(1, HOUR);
        MINUTE = (int16_t)NUM(2, 0);
        fprintf(stderr, "dw1: %02d:%02d\n", HOUR, MINUTE);
    } else if (IS("dia")) {
        DAY = (int16_t)NUM(1, DAY);
        fprintf(stderr, "dw1: dia %d\n", DAY);
    } else if (IS("ano")) {
        YEAR = (uint8_t)NUM(1, YEAR);
        fprintf(stderr, "dw1: ano %d\n", YEAR);
    } else if (IS("captura")) {
        dw1_dump_request(ARG(1) ? argv[1] : "captura");
    } else if (IS("bocadillo")) {
        v = NUM(1, 0);
        if (v < 0 || v > 7 || ENTITY_TABLE[1] == NULL) {
            fprintf(stderr, "dw1: bocadillo <0-7>\n");
            return;
        }
        fprintf(stderr, "dw1: bocadillo %d -> instancia %d\n", v, addConditionBubble(v, ENTITY_TABLE[1]));
    } else if (IS("tecnica")) {
        learnMove(NUM(1, 0));
        fprintf(stderr, "dw1: tecnica %d aprendida\n", NUM(1, 0));
    } else if (IS("medalla")) {
        unlockMedal((uint16_t)NUM(1, 0));
        fprintf(stderr, "dw1: medalla %d\n", NUM(1, 0));
    } else if (IS("carta")) {
        setCardAmount(NUM(1, 0), NUM(2, 1));
        fprintf(stderr, "dw1: carta %d = %d\n", NUM(1, 0), NUM(2, 1));
    } else {
        fprintf(stderr, "dw1: comando desconocido '%s' (ayuda)\n", argv[0]);
    }
#undef IS
#undef ARG
#undef NUM
}

/* ---- input ------------------------------------------------------------- */

static FILE *script_file;
static unsigned long script_call;
static char script_cmd[256];
static int script_pending;

static void next_script_line(void)
{
    char line[300];

    script_pending = 0;
    while (script_file && fgets(line, sizeof(line), script_file)) {
        char *p = line;

        if (*p == '#' || *p == '\n')
            continue;
        script_call = strtoul(p, &p, 10);
        while (*p == ' ' || *p == '\t')
            p++;
        snprintf(script_cmd, sizeof(script_cmd), "%s", p);
        script_pending = 1;
        return;
    }
}

void dw1_debug_frame(void)
{
    static int init;
    static int use_stdin;
    static char buf[512];
    static size_t len;

    if (!init) {
        const char *s = getenv("DW1_DEBUG_SCRIPT");

        init = 1;
        use_stdin = isatty(0);
        if (s && (script_file = fopen(s, "r")) == NULL)
            fprintf(stderr, "dw1: no puedo abrir DW1_DEBUG_SCRIPT '%s'\n", s);
        next_script_line();
        if (use_stdin)
            fprintf(stderr, "dw1: consola de debug: escribe 'ayuda' en esta terminal\n");
    }

    while (script_pending && dw1_pad_calls() >= script_call) {
        fprintf(stderr, "dw1: > %s", script_cmd);
        run(script_cmd);
        next_script_line();
    }

    if (use_stdin) {
        struct pollfd pfd = { 0, POLLIN, 0 };
        char *nl;

        while (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(0, buf + len, sizeof(buf) - 1 - len);

            if (n <= 0) {
                use_stdin = 0; /* EOF: terminal closed */
                break;
            }
            len += (size_t)n;
            buf[len] = '\0';
            while ((nl = strchr(buf, '\n')) != NULL) {
                *nl = '\0';
                run(buf);
                memmove(buf, nl + 1, len - (size_t)(nl + 1 - buf) + 1);
                len = strlen(buf);
            }
            if (len == sizeof(buf) - 1)
                len = 0; /* overlong line: drop it */
        }
    }
}
