/* Tests for port/src/memcard.c: the libmcrd API on raw card images.
 * Run from a scratch directory (it writes card1.mcd there). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libmcrd.h>

#include "dw1_memcard.h"

static int failures;

#define CHECK(cond)                                                            \
    do {                                                                       \
        if (!(cond)) {                                                         \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__,   \
                    #cond);                                                    \
            failures++;                                                        \
        }                                                                      \
    } while (0)

static long sync_result(long expect_cmd)
{
    long cmd = -1, result = -1;
    CHECK(MemCardSync(0, &cmd, &result) == 1);
    CHECK(cmd == expect_cmd);
    return result;
}

static unsigned char xor_frame(const unsigned char *fr)
{
    unsigned char x = 0;
    int i;
    for (i = 0; i < 127; i++)
        x ^= fr[i];
    return x;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    static unsigned char save[8192], back[8192];
    struct DIRENTRY dir_entries[15];
    long files;
    char path[600];
    FILE *f;
    unsigned char img[131072];
    int i;

    snprintf(path, sizeof(path), "%s/card1.mcd", dir);
    remove(path);
    dw1_memcard_set_dir(dir);

    MemCardInit(1);
    MemCardStart();
    CHECK(MemCardSync(1, NULL, NULL) == -1); /* nothing pending */

    /* a missing card is a formatted, empty card; first access reports it new */
    CHECK(MemCardExist(0x00) == 1);
    CHECK(sync_result(McFuncExist) == McErrNewCard);
    CHECK(MemCardAccept(0x00) == 1);
    CHECK(sync_result(McFuncAccept) == McErrNewCard);
    CHECK(MemCardAccept(0x00) == 1);
    CHECK(sync_result(McFuncAccept) == McErrNone);

    CHECK(MemCardGetDirentry(0x00, "*", dir_entries, &files, 0, 15) == McErrNone);
    CHECK(files == 0);

    /* a file that does not exist */
    CHECK(MemCardReadFile(0x00, "BASLUS-01032DMW", (unsigned long *)back, 0, 128) == 1);
    CHECK(sync_result(McFuncReadFile) == McErrFileNotExist);

    /* create, write, read back */
    CHECK(MemCardCreateFile(0x00, "BASLUS-01032DMW", 1) == McErrNone);
    CHECK(MemCardCreateFile(0x00, "BASLUS-01032DMW", 1) == McErrAlreadyExist);
    for (i = 0; i < (int)sizeof(save); i++)
        save[i] = (unsigned char)(i * 7 + 3);
    CHECK(MemCardWriteFile(0x00, "BASLUS-01032DMW", (unsigned long *)save, 0, 8192) == 1);
    CHECK(sync_result(McFuncWriteFile) == McErrNone);
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0x00, "BASLUS-01032DMW", (unsigned long *)back, 0, 8192) == 1);
    CHECK(sync_result(McFuncReadFile) == McErrNone);
    CHECK(memcmp(save, back, sizeof(save)) == 0);

    /* partial read at an offset */
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0x00, "BASLUS-01032DMW", (unsigned long *)back, 256, 128) == 1);
    CHECK(sync_result(McFuncReadFile) == McErrNone);
    CHECK(memcmp(save + 256, back, 128) == 0);

    /* reading past the end fails */
    CHECK(MemCardReadFile(0x00, "BASLUS-01032DMW", (unsigned long *)back, 8192, 128) == 1);
    CHECK(sync_result(McFuncReadFile) != McErrNone);

    /* directory with wildcards */
    CHECK(MemCardCreateFile(0x00, "BASLUS-01032DMW2", 2) == McErrNone);
    CHECK(MemCardGetDirentry(0x00, "BASLUS-01032*", dir_entries, &files, 0, 15) == McErrNone);
    CHECK(files == 2);
    CHECK(strcmp(dir_entries[0].name, "BASLUS-01032DMW") == 0);
    CHECK(dir_entries[0].size == 8192);
    CHECK(dir_entries[1].size == 16384);
    CHECK(MemCardGetDirentry(0x00, "BASLUS-01032DM?", dir_entries, &files, 0, 15) == McErrNone);
    CHECK(files == 1);

    /* the image on disk is a valid raw card: header, checksums, saved data */
    f = fopen(path, "rb");
    CHECK(f != NULL);
    if (f != NULL) {
        CHECK(fread(img, 1, sizeof(img), f) == sizeof(img));
        fclose(f);
        CHECK(img[0] == 'M' && img[1] == 'C');
        for (i = 0; i < 16; i++)
            CHECK(img[i * 128 + 127] == xor_frame(img + i * 128));
        CHECK(img[128] == 0x51);                       /* first file, first block */
        CHECK(memcmp(img + 128 + 10, "BASLUS-01032DMW", 15) == 0);
        CHECK(memcmp(img + 8192, save, sizeof(save)) == 0); /* block 1 */
    }

    /* the file survives a restart */
    MemCardInit(1);
    MemCardStart();
    memset(back, 0, sizeof(back));
    CHECK(MemCardReadFile(0x00, "BASLUS-01032DMW", (unsigned long *)back, 0, 8192) == 1);
    CHECK(sync_result(McFuncReadFile) == McErrNone);
    CHECK(memcmp(save, back, sizeof(save)) == 0);

    /* delete frees the blocks */
    CHECK(MemCardDeleteFile(0x00, "BASLUS-01032DMW") == McErrNone);
    CHECK(MemCardDeleteFile(0x00, "BASLUS-01032DMW") == McErrFileNotExist);
    CHECK(MemCardGetDirentry(0x00, "*", dir_entries, &files, 0, 15) == McErrNone);
    CHECK(files == 1);
    CHECK(MemCardCreateFile(0x00, "BIG", 14) == McErrBlockFull); /* DMW2 still uses 2 of 15 */
    CHECK(MemCardCreateFile(0x00, "BIG", 13) == McErrNone);
    CHECK(MemCardCreateFile(0x00, "ONE MORE", 1) == McErrBlockFull);

    /* port 2 is a separate card; multitap slots do not exist */
    CHECK(MemCardAccept(0x10) == 1);
    CHECK(sync_result(McFuncAccept) == McErrNewCard);
    CHECK(MemCardAccept(0x01) == 1);
    CHECK(sync_result(McFuncAccept) == McErrCardNotExist);

    snprintf(path, sizeof(path), "%s/card1.mcd", dir);
    remove(path);
    snprintf(path, sizeof(path), "%s/card2.mcd", dir);
    remove(path);

    if (failures) {
        fprintf(stderr, "memcard_test: %d failure(s)\n", failures);
        return 1;
    }
    printf("memcard_test: ok\n");
    return 0;
}
