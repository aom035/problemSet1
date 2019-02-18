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
#include <mpi.h>

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

void setInitialConfiguration(int n, char ***configuration, char *configurationFile) {

    FILE *fp = openFile(configurationFile);
    char ignore[32];
    if (fgets(ignore, sizeof(ignore), fp) == NULL) {
        fprintf(stderr, "Bad fgets() @ line:%d.\n", __LINE__);
        exit(EXIT_FAILURE);
    }

    char **temp = calloc((unsigned long)n , sizeof(char *));
    for(int i = 0; i < n; i++)
        temp[i] = calloc((unsigned long)n , sizeof(char));

    if (!temp) {
        fprintf(stderr, "NULL POINTER AT ALLOC:%d.\n", __LINE__);
        exit(EXIT_FAILURE);
    }

    int rowCounter = 0;
    while (rowCounter < n) {
        if (fgets(temp[rowCounter], n + 2, fp) == NULL) {
            fprintf(stderr, "Bad fgets() @ line:%d.\n", __LINE__);
            fprintf(stderr, "Config file should be %d lines long, %d were read .", n, rowCounter);
            exit(EXIT_FAILURE);
        }
        rowCounter++;
    }
    free(*configuration);
    *configuration = temp;
    fclose(fp);
}

void stepConfigurationOnce(int n, int ePP, char **currentBuffer, char **aggregateBuffer, const char transformationFunction[512]) {

    int aggX = n+2;
    int aggY = ePP+2;

    for (int x = 1; x < aggX-1; ++x) {
        for (int y = 1; y < aggY-1; ++y) {
            currentBuffer[x-1][y-1] = transformationFunction[
                    256*(aggregateBuffer[x-1][y-1]-48) + 128*(aggregateBuffer[x-1][y]-48) + 64*(aggregateBuffer[x-1][y+1]-48) +

                    32*(aggregateBuffer[x][y-1]-48) + 16*(aggregateBuffer[x][y]-48) + 8*(aggregateBuffer[x][y+1]-48) +

                    4*(aggregateBuffer[x+1][y-1]-48) + 2*(aggregateBuffer[x+1][y]-48) + (aggregateBuffer[x+1][y+1]-48)
            ];
        }
    }
}

void drawConfiguration(int n, char **configuration) {

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

void mergeAggregate(int n, int ePP, char **localBuffer, const char *left, const char *right, char **aggregate){

    int aggX = n+2;
    int aggY = ePP+2;

    for (int sidesX = 1; sidesX < aggX-1; ++sidesX) {
        aggregate[sidesX][0] = left[sidesX-1];
        aggregate[sidesX][aggY-1] = right[sidesX-1];
    }

    for (int coreX = 1; coreX < aggX-1; ++coreX) {
        for (int coreY = 1; coreY < aggY - 1; ++coreY) {
            aggregate[coreX][coreY] = localBuffer[coreX-1][coreY-1];
        }
    }

    for (int topsY = 0; topsY < aggY; ++topsY) {
        aggregate[0][topsY] = aggregate[aggX-2][topsY];
        aggregate[aggX-1][topsY] = aggregate[1][topsY];
    }
}

void compute(int n, int t, int ePP, char **rootConfiguration, int myRank, int commSize, const char *transformationFunction) {

    char sendLeft[n], sendRight[n];
    char recvLeft[n], recvRight[n];

    MPI_Request req;

    char **currentBuffer = malloc((unsigned long) n * sizeof(char *));
    for (int i = 0; i < n; ++i) {
        currentBuffer[i] = malloc((unsigned long) n * sizeof(char));
    }

    char **aggregateBuffer = malloc((unsigned long) (n+2) * sizeof(char *));
    for (int i = 0; i < n+2; ++i)
        aggregateBuffer[i] = malloc((unsigned long) (ePP+2) * sizeof(char));

    for (int k = 0; k < n; ++k)
        MPI_Scatter(rootConfiguration[k], ePP, MPI_CHAR, currentBuffer[k], ePP, MPI_CHAR, 0, MPI_COMM_WORLD);

    for (int i = 0; i < t; ++i) {

//        if ( myRank == 0 )
//            drawConfiguration(n, rootConfiguration);
//        for (int k = 0; k < n; ++k)
//            MPI_Scatter(rootConfiguration[k], ePP, MPI_CHAR, currentBuffer[k], ePP, MPI_CHAR, 0, MPI_COMM_WORLD);

        for (int li = 0; li < n; ++li)
            sendLeft[li] = currentBuffer[li][0];

        for (int ri = 0; ri < n; ++ri)
            sendRight[ri] = currentBuffer[ri][ePP-1];

        MPI_Isend(&sendLeft, n, MPI_CHAR, mod(myRank - 1, commSize), 1, MPI_COMM_WORLD, &req);
        MPI_Isend(&sendRight, n, MPI_CHAR, mod(myRank + 1, commSize), 2, MPI_COMM_WORLD, &req);

        MPI_Recv(&recvRight, n, MPI_CHAR, mod((myRank + 1), commSize), 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(&recvLeft, n, MPI_CHAR, mod((myRank - 1), commSize), 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        mergeAggregate(n, ePP, currentBuffer, recvLeft, recvRight, aggregateBuffer);

        stepConfigurationOnce(n, ePP, currentBuffer, aggregateBuffer, transformationFunction);

//        for (int k = 0; k < n; ++k)
//            MPI_Gather(currentBuffer[k], ePP, MPI_CHAR, rootConfiguration[k], ePP, MPI_CHAR, 0, MPI_COMM_WORLD);
//        usleep(100000);
    }

    for (int k = 0; k < n; ++k)
        MPI_Gather(currentBuffer[k], ePP, MPI_CHAR, rootConfiguration[k], ePP, MPI_CHAR, 0, MPI_COMM_WORLD);

    for (int l = 0; l < n + 2; ++l)
        free(aggregateBuffer[l]);
    free(aggregateBuffer);

    for (int j = 0; j < n; ++j)
        free(currentBuffer[j]);
    free(currentBuffer);
}

int main(int argc, char **argv) {

    MPI_Init(&argc, &argv);
    int myRank;
    int commSize;
    MPI_Comm_rank(MPI_COMM_WORLD, &myRank);
    MPI_Comm_size(MPI_COMM_WORLD, &commSize);

    char *functionFile = argv[1];
    char *configurationFile = argv[2];
    int t = (int) strtol(argv[3], NULL, 10);
    int n = read_n(configurationFile);
    char transformationFunction[512];
    setFunctionRange(functionFile, transformationFunction);
    char **rootConfiguration = NULL;
    int ePP = n / commSize; // elementsPerProcess

    if ( myRank == 0 ) {
        checkInput(argc);
        setInitialConfiguration(n, &rootConfiguration, configurationFile);
    } else {
        rootConfiguration = malloc(sizeof(char *));
    }

//    MPI_Barrier(MPI_COMM_WORLD); /* IMPORTANT */
//    double start = MPI_Wtime();

    compute(n, t, ePP, rootConfiguration, myRank, commSize, transformationFunction);

//    MPI_Barrier(MPI_COMM_WORLD); /* IMPORTANT */
//    double end = MPI_Wtime();


    if ( myRank == 0 )
        for (int j = 0; j < n; ++j)
            free(rootConfiguration[j]);

    free(rootConfiguration);

    MPI_Finalize();

//    if (myRank == 0) {
//
//        int inputSize;
//        sscanf(configurationFile, "%d", &inputSize);
//        char *fileName = malloc(64 * sizeof(char));
//        For logging on lyng using script.
//        sprintf(fileName, "/Home/siv24/aom035/ps1log2/p%d_k%d.log", commSize, inputSize);
//        FILE *pF = fopen(fileName, "a");
//        fprintf(pF, "%f\n", end - start);
//        fclose(pF);
//    }
    return 0;
}
