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
#include <math.h>
#include <unistd.h>
#include <time.h>

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

void setFunctionRange(char *fileName, char *function) {

    FILE *fp = openFile(fileName);

    char buffer[64];
    while (fgets(buffer, 64, fp) != NULL) {
        char functionArgument[16];
        char functionValue;
        sscanf(buffer, "%s %c", functionArgument, &functionValue);
        function[bitStrToInt(functionArgument)] = functionValue;
    }
    fclose(fp);
}

int mod(int val, int divisor) {
    return (val % divisor + divisor) % divisor;
}

int read_n(char *fileName) {

    FILE *fp = openFile(fileName);
    char buffer[8];
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        fprintf(stderr, "Bad fgets() @ line:%d.\n", __LINE__);
        fprintf(stderr, "Was looking for n.");
        exit(EXIT_FAILURE);
    }
    int n = (int) strtol(buffer, NULL, 10);
    fclose(fp);
    return n;
}

void setInitialConfiguration(int n, char configuration[n][n], char *configurationFile) {

    FILE *fp = openFile(configurationFile);
    char ignore[32];
    if (fgets(ignore, sizeof(ignore), fp) == NULL) {
        fprintf(stderr, "Bad fgets() @ line:%d.\n", __LINE__);
        exit(EXIT_FAILURE);
    }

    int rowCounter = 0;
    while (rowCounter < n) {
        if (fgets(configuration[rowCounter], n + 2, fp) == NULL) {
            fprintf(stderr, "Bad fgets() @ line:%d.\n", __LINE__);
            fprintf(stderr, "Config file should be %d lines long, %d were read .", n, rowCounter);
            exit(EXIT_FAILURE);
        }
        rowCounter++;
    }
    fclose(fp);
}

void copy2D(int n, char first[n][n], char second[n][n]) {
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            second[i][j] = first[i][j];
}

void stepConfigurationOnce(int n, char currentConfiguration[n][n], char oldBuffer[n][n], const char transformationFunction[512]) {

    copy2D(n, currentConfiguration, oldBuffer);
    int endPoint = n-1;

    for (int x = 0; x < n; ++x) {
        for (int y = 0; y < n; ++y) {
            if((y == 0 || y == endPoint) || (x == 0 || x == endPoint)){ // Lazy evaluation?
                currentConfiguration[x][y] = transformationFunction[
                        256*(oldBuffer[mod(x-1, n)][mod(y-1, n)]-48) + 128*(oldBuffer[mod(x-1, n)][mod(y, n)]-48) + 64*(oldBuffer[mod(x-1, n)][mod(y+1, n)]-48) +

                        32*(oldBuffer[mod(x, n)][mod(y-1, n)]-48) + 16*(oldBuffer[mod(x, n)][mod(y, n)]-48) + 8*(oldBuffer[mod(x, n)][mod(y+1, n)]-48) +

                        4*(oldBuffer[mod(x+1, n)][mod(y-1, n)]-48) + 2*(oldBuffer[mod(x+1, n)][mod(y, n)]-48) + (oldBuffer[mod(x+1, n)][mod(y+1, n)]-48)
                ];
            } else {
                currentConfiguration[x][y] = transformationFunction[
                        256*(oldBuffer[x-1][y-1]-48) + 128*(oldBuffer[x-1][y]-48) + 64*(oldBuffer[x-1][y+1]-48) +

                        32*(oldBuffer[x][y-1]-48) + 16*(oldBuffer[x][y]-48) + 8*(oldBuffer[x][y+1]-48) +

                        4*(oldBuffer[x+1][y-1]-48) + 2*(oldBuffer[x+1][y]-48) + (oldBuffer[x+1][y+1]-48)
                ];
            }
        }
    }
}

void drawConfiguration(int n, char configuration[n][n]) {

    for (int x = 0; x < n; ++x) {
        if ( x != 0 )
            printf("\n");
        for (int y = 0; y < n; ++y)
            (configuration[x][y] - 48) ? printf("*") : printf(" ");
    }
    printf("\n");
    printf("\033[2J");   // Clean the screen
    printf("\033[1;1H"); // Set the cursor to 1:1 position
}

int main(int argc, char **argv) {

    checkInput(argc);

    char *functionFile = argv[1];
    char *configurationFile = argv[2];
    int t = (int) strtol(argv[3], NULL, 10);
    int n = read_n(configurationFile);

    char transformationFunction[512];
    setFunctionRange(functionFile, transformationFunction);

    char configuration[n][n];
    char previousConfiguration[n][n];
    setInitialConfiguration(n, configuration, configurationFile);


//    clock_t start = clock(), diff;
    for (int i = 0; i < t; ++i) {
        stepConfigurationOnce(n, configuration, previousConfiguration, transformationFunction);
        drawConfiguration(n, configuration);
        usleep(100000);
    }
//    diff = clock() - start;

//    long msec = diff * 1000 / CLOCKS_PER_SEC;
//    printf("Time taken %ld seconds %ld milliseconds", msec/1000, msec%1000);

}