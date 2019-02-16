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
#include <mpi.h>
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
}

int readConfigLength(char *fileName) {

    FILE *fp = openFile(fileName);
    int toRet;
    if(fscanf(fp, "%d", &toRet) != 1) {
        fprintf(stderr, "Bad configuration file, could not parse n.");
        exit(EXIT_FAILURE);
    }
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

void stepConfig(int n, char **config, char left, char right, const char *transFunc) {

    char *current = *config;
    char *next = malloc((unsigned int) n * sizeof(char));
    for (int x = 0; x < n; ++x) {
        if (x == 0) {
            next[x] =
                    transFunc[
                            4 * (left - 48) +
                            2 * (current[x] - 48) +
                            (current[x+1] - 48)
                    ];
        } else if (x == n - 1) {
            next[x] =
                    transFunc[
                            4 * (current[x-1] - 48) +
                            2 * (current[x] - 48) +
                            (right - 48)
                    ];
        } else {
            next[x] =
                    transFunc[
                            4 * (current[x - 1] - 48) +
                            2 * (current[x] - 48) +
                            (current[x + 1] - 48)
                    ];
        }
    }
    free(*config);
    *config = next;
}

void drawConfig(int n, const char *config) {
    for (int x = 0; x < n; ++x)
        (config[x] - 48) ? printf(" ") : printf("█");
    printf("\n");
}

void computeAndDraw(int t, int n, int ePP, char *rootConf, int myRank, int commSize, char *transFunc) {

    char *localConf = malloc((unsigned int) ePP * sizeof(char));
    char sendLeft, sendRight;
    char recvLeft, recvRight;
    MPI_Scatter(rootConf, ePP, MPI_CHAR, localConf, ePP, MPI_CHAR, 0, MPI_COMM_WORLD);

    for (int i = 0; i < t; ++i) {

        sendLeft = localConf[0];
        sendRight = localConf[ePP - 1];

        MPI_Send(&sendLeft, 1, MPI_CHAR, mod(myRank - 1, commSize), 0, MPI_COMM_WORLD);
        MPI_Send(&sendRight, 1, MPI_CHAR, mod(myRank + 1, commSize), 0, MPI_COMM_WORLD);

        MPI_Recv(&recvRight, 1, MPI_CHAR, mod((myRank + 1), commSize), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&recvLeft, 1, MPI_CHAR, mod((myRank - 1), commSize), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        stepConfig(ePP, &localConf, recvLeft, recvRight, transFunc);

        MPI_Gather(localConf, ePP, MPI_CHAR, rootConf, ePP, MPI_CHAR, 0, MPI_COMM_WORLD); // REVERSE of MPI_Scatter.
        if (myRank == 0)
            drawConfig(n, rootConf);
    }
}

void compute(int t, int ePP, char *rootConf, int myRank, int commSize, char *transFunc) {

    char *localConf = malloc((unsigned int) ePP * sizeof(char));
    char sendLeft, sendRight;
    char recvLeft, recvRight;

    MPI_Scatter(rootConf, ePP, MPI_CHAR, localConf, ePP, MPI_CHAR, 0, MPI_COMM_WORLD);
    for (int i = 0; i < t; ++i) {

        sendLeft = localConf[0];
        sendRight = localConf[ePP - 1];

        MPI_Send(&sendLeft, 1, MPI_CHAR, mod(myRank - 1, commSize), 0, MPI_COMM_WORLD);
        MPI_Send(&sendRight, 1, MPI_CHAR, mod(myRank + 1, commSize), 0, MPI_COMM_WORLD);

        MPI_Recv(&recvRight, 1, MPI_CHAR, mod((myRank + 1), commSize), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&recvLeft, 1, MPI_CHAR, mod((myRank - 1), commSize), 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        stepConfig(ePP, &localConf, recvLeft, recvRight, transFunc);
    }
    MPI_Gather(localConf, ePP, MPI_CHAR, rootConf, ePP, MPI_CHAR, 0, MPI_COMM_WORLD); // REVERSE of MPI_Scatter.
}

int main(int argc, char **argv) {

    checkInput(argc);
    char *funcFile = argv[1];
    char *confFile = argv[2];
    int t = atoi(argv[3]);
    int n = readConfigLength(confFile);

    char transFunc[8];
    setRange(transFunc, funcFile);

    MPI_Init(&argc, &argv);
    int myRank;
    int commSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    int ePP = n / commSize; // elementsPerProcess
    char *rootConf = malloc((unsigned int) n * sizeof(char));

    if (myRank == 0)
        readConfigState(confFile, n, rootConf);

//    MPI_Barrier(MPI_COMM_WORLD); /* IMPORTANT */
//    double start = MPI_Wtime();
    compute(t, ePP, rootConf, myRank, commSize, transFunc);
//    computeAndDraw(t, n, ePP, rootConf, myRank, commSize, transFunc);
//    MPI_Barrier(MPI_COMM_WORLD); /* IMPORTANT */
//    double end = MPI_Wtime();

    MPI_Finalize();


//    if (myRank == 0) {
//
//        int inputSize;
//        sscanf(confFile, "%d", &inputSize);
//        char *fileName = malloc(64 * sizeof(char));
//        For logging on lyng using script.
//        sprintf(fileName, "/Home/siv24/aom035/ps1log1/p%d_k%d.log", commSize, inputSize);
//        FILE *pF = fopen(fileName, "a");
//        fprintf(pF, "%f\n", end - start);
//        fclose(pF);
//    }
    return 0;
}