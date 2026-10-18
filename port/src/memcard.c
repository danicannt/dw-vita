/* libmcrd (memory card) on top of raw memory card image files.
 *
 * PSY-Z only has libmcrd as assembly. This implements the PSY-Q API the game
 * uses on 128 KB raw card images in the standard PS1 layout (the .mcd/.mcr
 * format of DuckStation, PCSX-Redux, ePSXe and memory card tools), so saves
 * can move between the port and emulators.
 *
 * Card layout: 16 blocks of 8 KB. Block 0 holds the header frame ("MC"), 15
 * directory frames (one per data block) and the broken sector list; blocks
 * 1-15 hold file data. Each 128-byte frame ends with an XOR checksum.
 *
 * Asynchronous calls (Exist, Accept, ReadFile, WriteFile, ReadData,
 * WriteData) finish immediately; their result is reported by the next
 * MemCardSync and the callback, as on the PS1. A missing card file is a
 * formatted, empty card; it is written on the first change. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libmcrd.h>

#include "dw1_memcard.h"

#define CARD_SIZE (128 * 1024)
#define BLOCK_SIZE 8192
#define FRAME_SIZE 128
#define NUM_BLOCKS 16

#define STATE_FIRST 0x51
#define STATE_MIDDLE 0x52
#define STATE_LAST 0x53
#define STATE_FREE 0xA0

typedef struct {
    unsigned char data[CARD_SIZE];
    int loaded;
    int accepted;  /* first Accept reports a new card */
} Card;

static Card cards[2];
static char card_dir[512] = "";
static int started;

/* completed asynchronous operation, reported by MemCardSync */
static int pending;
static long pending_cmd, pending_result;
static MemCB callback;

/* open file for MemCardOpen/ReadData/WriteData */
static int open_chan = -1;
static int open_first_block = -1;

void dw1_memcard_set_dir(const char *dir)
{
    snprintf(card_dir, sizeof(card_dir), "%s", dir ? dir : "");
}

static int chan_index(long chan)
{
    if ((chan & 0x0f) != 0) /* only the first slot of each port (no multitap) */
        return -1;
    if (chan == 0x00)
        return 0;
    if (chan == 0x10)
        return 1;
    return -1;
}

static void card_path(int idx, char *out, size_t n)
{
    snprintf(out, n, "%s%scard%d.mcd", card_dir, card_dir[0] ? "/" : "", idx + 1);
}

static unsigned char *frame(Card *c, int block, int f)
{
    return c->data + block * BLOCK_SIZE + f * FRAME_SIZE;
}

static void frame_checksum(unsigned char *fr)
{
    unsigned char x = 0;
    int i;
    for (i = 0; i < FRAME_SIZE - 1; i++)
        x ^= fr[i];
    fr[FRAME_SIZE - 1] = x;
}

static void put16(unsigned char *p, unsigned v) { p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; }
static void put32(unsigned char *p, unsigned long v)
{
    p[0] = v & 0xff; p[1] = (v >> 8) & 0xff; p[2] = (v >> 16) & 0xff; p[3] = (v >> 24) & 0xff;
}
static unsigned get16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned long get32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

static void format_image(Card *c)
{
    int i;
    unsigned char *fr;

    memset(c->data, 0, CARD_SIZE);
    fr = frame(c, 0, 0);
    fr[0] = 'M';
    fr[1] = 'C';
    frame_checksum(fr);
    for (i = 1; i < NUM_BLOCKS; i++) {
        fr = frame(c, 0, i);
        put32(fr, STATE_FREE);
        put16(fr + 8, 0xffff);
        frame_checksum(fr);
    }
    for (i = 16; i < 36; i++) { /* broken sector list: none */
        fr = frame(c, 0, i);
        put32(fr, 0xffffffff);
        put16(fr + 8, 0xffff);
        frame_checksum(fr);
    }
    memcpy(frame(c, 0, 63), frame(c, 0, 0), FRAME_SIZE);
}

static int is_formatted(Card *c)
{
    return c->data[0] == 'M' && c->data[1] == 'C';
}

static Card *load(int idx)
{
    Card *c = &cards[idx];
    char path[600];
    FILE *f;

    if (c->loaded)
        return c;
    card_path(idx, path, sizeof(path));
    f = fopen(path, "rb");
    if (f != NULL) {
        size_t n = fread(c->data, 1, CARD_SIZE, f);
        fclose(f);
        if (n != CARD_SIZE) {
            fprintf(stderr, "dw1: %s is not a 128 KB memory card image\n", path);
            format_image(c);
        }
    } else {
        format_image(c);
    }
    c->loaded = 1;
    return c;
}

static int save(int idx)
{
    char path[600], tmp[620];
    FILE *f;

    card_path(idx, path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "wb");
    if (f == NULL)
        return -1;
    if (fwrite(cards[idx].data, 1, CARD_SIZE, f) != CARD_SIZE) {
        fclose(f);
        remove(tmp);
        return -1;
    }
    fclose(f);
    remove(path); /* rename does not replace on every platform */
    return rename(tmp, path);
}

/* name match with PS1 wildcards: '*' (rest of name) and '?' (one char) */
static int name_match(const char *pat, const char *name)
{
    for (; *pat; pat++, name++) {
        if (*pat == '*')
            return 1;
        if (*name == '\0')
            return 0;
        if (*pat != '?' && *pat != *name)
            return 0;
    }
    return *name == '\0';
}

static void dir_name(Card *c, int block, char *out)
{
    memcpy(out, frame(c, 0, block) + 10, 20);
    out[20] = '\0';
}

/* first block of a file, or -1 */
static int find_file(Card *c, const char *name)
{
    char n[21];
    int b;

    for (b = 1; b < NUM_BLOCKS; b++) {
        if (frame(c, 0, b)[0] != STATE_FIRST)
            continue;
        dir_name(c, b, n);
        if (strncmp(n, name, 20) == 0)
            return b;
    }
    return -1;
}

static int next_block(Card *c, int block)
{
    unsigned nb = get16(frame(c, 0, block) + 8);
    return nb == 0xffff ? -1 : (int)nb + 1;
}

/* copy between a file and memory; offset and size in bytes */
static int file_io(Card *c, int first, unsigned char *mem, long offset, long bytes, int write)
{
    int block = first;
    long size = (long)get32(frame(c, 0, first) + 4);

    if (offset < 0 || bytes < 0 || offset + bytes > size)
        return -1;
    while (offset >= BLOCK_SIZE && block > 0) {
        block = next_block(c, block);
        offset -= BLOCK_SIZE;
    }
    while (bytes > 0 && block > 0) {
        long n = BLOCK_SIZE - offset;
        unsigned char *p = c->data + block * BLOCK_SIZE + offset;
        if (n > bytes)
            n = bytes;
        if (write)
            memcpy(p, mem, n);
        else
            memcpy(mem, p, n);
        mem += n;
        bytes -= n;
        offset = 0;
        block = next_block(c, block);
    }
    return bytes == 0 ? 0 : -1;
}

static void finish(long cmd, long result)
{
    pending = 1;
    pending_cmd = cmd;
    pending_result = result;
    if (callback != NULL)
        callback((unsigned long)cmd, (unsigned long)result);
}

/* ---- PSY-Q API --------------------------------------------------------------- */

void MemCardInit(long val)
{
    (void)val;
    memset(cards, 0, sizeof(cards));
    pending = 0;
    callback = NULL;
}

void MemCardEnd(void) {}

void MemCardStart(void)
{
    started = 1;
}

void MemCardStop(void)
{
    started = 0;
}

MemCB MemCardCallback(MemCB func)
{
    MemCB old = callback;
    callback = func;
    return old;
}

long MemCardSync(long mode, long *cmds, long *result)
{
    (void)mode; /* nothing is ever in progress: both modes behave alike */
    if (!pending)
        return -1;
    pending = 0;
    if (cmds != NULL)
        *cmds = pending_cmd;
    if (result != NULL)
        *result = pending_result;
    return 1;
}

long MemCardExist(long chan)
{
    int idx = chan_index(chan);

    if (pending)
        return 0;
    if (idx < 0 || !started) {
        finish(McFuncExist, McErrCardNotExist);
        return 1;
    }
    load(idx);
    finish(McFuncExist, cards[idx].accepted ? McErrNone : McErrNewCard);
    return 1;
}

long MemCardAccept(long chan)
{
    int idx = chan_index(chan);
    Card *c;

    if (pending)
        return 0;
    if (idx < 0 || !started) {
        finish(McFuncAccept, McErrCardNotExist);
        return 1;
    }
    c = load(idx);
    if (!is_formatted(c)) {
        finish(McFuncAccept, McErrNotFormat);
        return 1;
    }
    finish(McFuncAccept, c->accepted ? McErrNone : McErrNewCard);
    c->accepted = 1;
    return 1;
}

long MemCardFormat(long chan)
{
    int idx = chan_index(chan);

    if (idx < 0)
        return McErrCardNotExist;
    format_image(load(idx));
    return save(idx) == 0 ? McErrNone : McErrCardInvalid;
}

long MemCardUnformat(long chan)
{
    int idx = chan_index(chan);

    if (idx < 0)
        return McErrCardNotExist;
    memset(load(idx)->data, 0, CARD_SIZE);
    return save(idx) == 0 ? McErrNone : McErrCardInvalid;
}

long MemCardCreateFile(long chan, char *file, long blocks)
{
    int idx = chan_index(chan);
    Card *c;
    int list[NUM_BLOCKS];
    int n = 0, b, i;

    if (idx < 0)
        return McErrCardNotExist;
    c = load(idx);
    if (!is_formatted(c))
        return McErrNotFormat;
    if (find_file(c, file) >= 0)
        return McErrAlreadyExist;
    for (b = 1; b < NUM_BLOCKS && n < blocks; b++)
        if (frame(c, 0, b)[0] == STATE_FREE || (frame(c, 0, b)[0] & 0xf0) == 0xA0)
            list[n++] = b;
    if (blocks <= 0 || n < blocks)
        return McErrBlockFull;
    for (i = 0; i < n; i++) {
        unsigned char *fr = frame(c, 0, list[i]);
        memset(fr, 0, FRAME_SIZE);
        fr[0] = n == 1 ? STATE_FIRST : i == 0 ? STATE_FIRST : i == n - 1 ? STATE_LAST : STATE_MIDDLE;
        if (i == 0) {
            put32(fr + 4, (unsigned long)n * BLOCK_SIZE);
            strncpy((char *)fr + 10, file, 20);
        }
        put16(fr + 8, i == n - 1 ? 0xffff : (unsigned)(list[i + 1] - 1));
        frame_checksum(fr);
        memset(c->data + list[i] * BLOCK_SIZE, 0, BLOCK_SIZE);
    }
    return save(idx) == 0 ? McErrNone : McErrCardInvalid;
}

long MemCardDeleteFile(long chan, char *file)
{
    int idx = chan_index(chan);
    Card *c;
    int b, next;

    if (idx < 0)
        return McErrCardNotExist;
    c = load(idx);
    b = find_file(c, file);
    if (b < 0)
        return McErrFileNotExist;
    while (b > 0) {
        unsigned char *fr = frame(c, 0, b);
        next = next_block(c, b);
        memset(fr, 0, FRAME_SIZE);
        put32(fr, STATE_FREE);
        put16(fr + 8, 0xffff);
        frame_checksum(fr);
        b = next;
    }
    return save(idx) == 0 ? McErrNone : McErrCardInvalid;
}

long MemCardGetDirentry(long chan, char *name, struct DIRENTRY *dir, long *files, long ofs,
                        long max)
{
    int idx = chan_index(chan);
    Card *c;
    int b;
    long found = 0, n = 0;
    char fn[21];

    if (files != NULL)
        *files = 0;
    if (idx < 0)
        return McErrCardNotExist;
    c = load(idx);
    if (!is_formatted(c))
        return McErrNotFormat;
    for (b = 1; b < NUM_BLOCKS; b++) {
        if (frame(c, 0, b)[0] != STATE_FIRST)
            continue;
        dir_name(c, b, fn);
        if (!name_match(name, fn))
            continue;
        if (found++ < ofs)
            continue;
        if (n >= max)
            break;
        memset(&dir[n], 0, sizeof(dir[n]));
        memcpy(dir[n].name, fn, 20);
        dir[n].attr = 0;
        dir[n].size = (int)get32(frame(c, 0, b) + 4);
        n++;
    }
    if (files != NULL)
        *files = n;
    return McErrNone;
}

long MemCardReadFile(long chan, char *file, unsigned long *adrs, long offset, long bytes)
{
    int idx = chan_index(chan);
    Card *c;
    int b;

    if (pending)
        return 0;
    if (idx < 0) {
        finish(McFuncReadFile, McErrCardNotExist);
        return 1;
    }
    c = load(idx);
    b = find_file(c, file);
    if (b < 0) {
        finish(McFuncReadFile, McErrFileNotExist);
        return 1;
    }
    finish(McFuncReadFile,
           file_io(c, b, (unsigned char *)adrs, offset, bytes, 0) == 0 ? McErrNone : McErrCardInvalid);
    return 1;
}

long MemCardWriteFile(long chan, char *file, unsigned long *adrs, long offset, long bytes)
{
    int idx = chan_index(chan);
    Card *c;
    int b;

    if (pending)
        return 0;
    if (idx < 0) {
        finish(McFuncWriteFile, McErrCardNotExist);
        return 1;
    }
    c = load(idx);
    b = find_file(c, file);
    if (b < 0) {
        finish(McFuncWriteFile, McErrFileNotExist);
        return 1;
    }
    if (file_io(c, b, (unsigned char *)adrs, offset, bytes, 1) != 0 || save(idx) != 0) {
        finish(McFuncWriteFile, McErrCardInvalid);
        return 1;
    }
    finish(McFuncWriteFile, McErrNone);
    return 1;
}

long MemCardOpen(long chan, char *file, long flag)
{
    int idx = chan_index(chan);
    int b;

    (void)flag;
    if (idx < 0)
        return McErrCardNotExist;
    b = find_file(load(idx), file);
    if (b < 0)
        return McErrFileNotExist;
    open_chan = idx;
    open_first_block = b;
    return McErrNone;
}

void MemCardClose(void)
{
    open_chan = -1;
    open_first_block = -1;
}

long MemCardReadData(unsigned long *adrs, long offset, long bytes)
{
    if (pending)
        return 0;
    if (open_chan < 0) {
        finish(McFuncReadData, McErrFileNotExist);
        return 1;
    }
    finish(McFuncReadData,
           file_io(&cards[open_chan], open_first_block, (unsigned char *)adrs, offset, bytes, 0) == 0
               ? McErrNone
               : McErrCardInvalid);
    return 1;
}

long MemCardWriteData(unsigned long *adrs, long offset, long bytes)
{
    if (pending)
        return 0;
    if (open_chan < 0) {
        finish(McFuncWriteData, McErrFileNotExist);
        return 1;
    }
    if (file_io(&cards[open_chan], open_first_block, (unsigned char *)adrs, offset, bytes, 1) != 0 ||
        save(open_chan) != 0) {
        finish(McFuncWriteData, McErrCardInvalid);
        return 1;
    }
    finish(McFuncWriteData, McErrNone);
    return 1;
}
