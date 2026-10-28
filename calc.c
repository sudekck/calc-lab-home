// Name Lastname StudentID: Sude Kucukoglu 251ADB136
// Repository: https://github.com/sudekck/Programming_Languages_C_2026_Fall_Labs
// Compile with: gcc -O2 -Wall -Wextra -std=c17 -o calc calc.c -lm
// Resources used: OpenAI Codex generated the implementation 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

/* Replace these three strings; use lowercase filename-safe spelling. */
#define NAME "sude"
#define LASTNAME "kucukoglu"
#define STUDENT_ID "studentid"


/* The index is measured from the start of the whole file, so newlines
   and skipped comments still count toward error positions. */
typedef struct {
    const char *text;
    size_t at;
    size_t error;
} Parser;

/* Keep newlines because each line must be evaluated separately. */
static void spaces(Parser *p)
{
    while (p->text[p->at] == ' ' || p->text[p->at] == '\t' ||
           p->text[p->at] == '\r')
        p->at++;
}

static void fail(Parser *p, size_t at)
{
    /* Record only the first error; convert the index to a 1-based position. */
    if (p->error == 0)
        p->error = at + 1;
}

static long long primary(Parser *p)
{
    long long value = 0;
    spaces(p);
    if (!isdigit((unsigned char)p->text[p->at])) {
        fail(p, p->at);
        return 0;
    }
    
    /* Shift the existing digits left by one decimal place, then add
       the new digit. Subtracting '0' converts its character to a number. */
    while (isdigit((unsigned char)p->text[p->at])) {
        value = value * 10 + p->text[p->at] - '0';
        p->at++;
    }
    return value;
}

static long long expression(Parser *p)
{
    long long left = primary(p);
    /* Only + and - are supported. A loop applies them from left to right,
       so 10 - 3 + 1 is evaluated as (10 - 3) + 1. */
    while (!p->error) {
        char op;
        long long right;
        spaces(p);
        op = p->text[p->at];
        if (op != '+' && op != '-')
            break;
        p->at++;
        right = primary(p);
        if (!p->error)
            left = op == '+' ? left + right : left - right;
    }
    return left;
}


static char *read_file(FILE *file)
{
    size_t used = 0, capacity = 4096;
    char *data = malloc(capacity);
    int ch;
    if (!data)
        return NULL;
    while ((ch = fgetc(file)) != EOF) {
        if (used + 1 == capacity) {
            char *bigger;
            /* Doubling reduces reallocations for large inputs. */
            capacity *= 2;
            bigger = realloc(data, capacity);
            if (!bigger) {
                free(data);
                return NULL;
            }
            data = bigger;
        }
        data[used++] = (char)ch;
    }
    if (ferror(file)) {
        free(data);
        return NULL;
    }
    /* Reserve a final null character so the buffer can be read as a C string. */
    data[used] = '\0';
    return data;
}

/* Remove the directory prefix and .txt extension for the output filename. */
static char *base_name(const char *path)
{
    size_t length = strlen(path), start, n;
    char *base;
    while (length > 0 && path[length - 1] == '/')
        length--;
    start = length;
    while (start > 0 && path[start - 1] != '/')
        start--;
    n = length - start;
    if (n >= 4 && strncmp(path + length - 4, ".txt", 4) == 0)
        n -= 4;
    base = malloc(n + 1);
    if (base) {
        memcpy(base, path + start, n);
        base[n] = '\0';
    }
    return base;
}

static int process_file(const char *input, const char *outdir)
{
    FILE *source = fopen(input, "rb"), *results = NULL, *output = NULL;
    char *data = NULL, *base = NULL, *path = NULL;
    int ok = 0, ch;
    Parser p;
    if (!source)
        goto cleanup;
    data = read_file(source);
    fclose(source);
    source = NULL;
    if (!data)
        goto cleanup;
    /* Delay copying results to the output: a later error must replace
       all results, including those from earlier valid lines. */
    results = tmpfile();
    if (!results)
        goto cleanup;
    p = (Parser){data, 0, 0};
    while (data[p.at] && !p.error) {
        long long value;
        spaces(&p);
        /* A comment is accepted only before an expression begins. */
        if (data[p.at] == '#') {
            while (data[p.at] && data[p.at] != '\n')
                p.at++;
        } else if (data[p.at] && data[p.at] != '\n') {
            value = expression(&p);
            spaces(&p);
            /* Anything left after the expression is an unexpected token. */
            if (!p.error && data[p.at] && data[p.at] != '\n')
                fail(&p, p.at);
            if (!p.error && fprintf(results, "%lld\n", value) < 0)
                goto cleanup;
        }
        if (data[p.at] == '\n')
            p.at++;
    }
    base = base_name(input);
    if (!base)
        goto cleanup;
    /* The extra bytes cover /, three underscores, .txt and the null terminator. */
    path = malloc(strlen(outdir) + strlen(base) + strlen(NAME) +
                  strlen(LASTNAME) + strlen(STUDENT_ID) + 9);
    if (!path)
        goto cleanup;
    snprintf(path, strlen(outdir) + strlen(base) + strlen(NAME) +
             strlen(LASTNAME) + strlen(STUDENT_ID) + 9,
             "%s/%s_%s_%s_%s.txt", outdir, base,
            NAME, LASTNAME, STUDENT_ID);
    output = fopen(path, "wb");
    if (!output)
        goto cleanup;
    if (p.error) {
        if (fprintf(output, "ERROR:%zu\n", p.error) < 0)
            goto cleanup;
    } else {
        /* Flush pending writes and return to the start before copying results. */
        if (fflush(results) != 0 || fseek(results, 0, SEEK_SET) != 0)
            goto cleanup;
        while ((ch = fgetc(results)) != EOF)
            if (fputc(ch, output) == EOF)
                goto cleanup;
        if (ferror(results))
            goto cleanup;
    }
    ok = 1;
/* Share cleanup between success and failure paths to release resources. */
cleanup:
    if (source)
        fclose(source);
    if (results && fclose(results) != 0)
        ok = 0;
    if (output && fclose(output) != 0)
        ok = 0;
    free(data);
    free(base);
    free(path);
    if (!ok)
        fprintf(stderr, "Cannot read input or write output: %s\n", input);
    return ok;
}

int main(int argc, char **argv)
{
    struct stat info;
   
    /* Expected arguments: program name, -o, output directory, input file. */
    if (argc != 4 || strcmp(argv[1], "-o") != 0) {
        fprintf(stderr, "Usage: calc -o OUTDIR input.txt\n");
        return 1;
    }
    if (stat(argv[3], &info) != 0 || !S_ISREG(info.st_mode)) {
        fprintf(stderr, "Input file does not exist: %s\n", argv[3]);
        return 1;
    }
    /* Create the output directory if absent; otherwise require a directory. */
    if (stat(argv[2], &info) != 0) {
        if (mkdir(argv[2], 0777) != 0) {
            fprintf(stderr, "Cannot create output directory: %s\n", argv[2]);
            return 1;
        }
    } else if (!S_ISDIR(info.st_mode)) {
        fprintf(stderr, "Output path is not a directory: %s\n", argv[2]);
        return 1;
    }

    /* A written ERROR result still counts as successful file processing.
       Only input/output failures produce a nonzero exit code here. */
    return process_file(argv[3], argv[2]) ? 0 : 1;
}
