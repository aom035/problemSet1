/*
Copyright (c) 2019 Andreas Ommundsen

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>

void checkInput(int argc) {
    if (argc != 4) {
        fprintf(stderr, "Bad input. Expecting: {functionDefinition.txt} {initialConfiguration.txt} {t = time/turns}");
        exit(EXIT_FAILURE);
    }
}

int bitStrToInt(const char *s) {
    return (int) strtol(s, NULL, 2);
}

FILE *openFile(char *fileName) {
    FILE *fp = fopen(fileName, "r");
    if (fp == NULL) {
        fprintf(stderr, "Could not open %s.\n", fileName);
        exit(EXIT_FAILURE);
    } else return fp;
}

void setRange(char *function, char *fileName) {

    FILE *fp = openFile(fileName);

    char temp[20];
    while (fgets(temp, 20, fp) != NULL) {
        char index[8];
        char value;
        sscanf(temp, "%s %c", index, &value);
        function[bitStrToInt(index)] = value;
    }
    fclose(fp);
}

int mod(int val, int divisor) {
    return (val % divisor + divisor) % divisor;
//    for (int i = 0; i < n; ++i)
//        printf("%2d %2d %2d\n", mod(i-1, n), mod(i, n), mod(i+1, n));
}

int readConfigLength(char *fileName) {

    FILE *fp = openFile(fileName);
    int toRet;
    fscanf(fp, "%d", &toRet);
    fclose(fp);
    return toRet;
}

void readConfigState(char *fileName, int len, char *config) {

    FILE *fp = openFile(fileName);
    char temp[len + 1];
    fgets(temp, len + 1, fp);
    fgets(temp, len + 1, fp);
    for (int i = 0; i < len; ++i)
        config[i] = temp[i];
    fclose(fp);
}

void *stepConfig(int n, char **config, const char *transFunc) {

    char *current = *config;
    char *next = malloc(n * sizeof(char));
    for (int x = 0; x < n; ++x) {
        next[x] =
                transFunc[
                        4 * (current[mod(x - 1, n)] - 48) +
                        2 * (current[mod(x, n)] - 48) +
                        (current[mod(x + 1, n)] - 48)
                ];
    }
    free(*config);
    *config = next;
}

void drawConfig(int n, const char *config) {
    for (int x = 0; x < n; ++x)
        (config[x] - 48) ? printf(" ") : printf("█");
    printf("\n");
}

int main(int argc, char **argv) {

    printf("Start\n");
    checkInput(argc);
    char *funcFile = argv[1];
    char *confFile = argv[2];
    int t = atoi(argv[3]);

    char transFunc[8];
    setRange(transFunc, funcFile);

    int n = readConfigLength(confFile);
    char *config = malloc(n * sizeof(char));
    readConfigState(confFile, n, config);

    drawConfig(n, config);
    for (int i = 0; i < t; ++i) {
        stepConfig(n, &config, transFunc);
        drawConfig(n, config);
    }
    printf("\nEnd\n");
}
