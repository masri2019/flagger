//
// Created by mobin on 5/14/24.
//

#include "summary_table.h"
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>   // stat
#include <stdbool.h>    // bool type
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include "sonLib.h"

SummaryTable *SummaryTable_construct(int numberOfRows, int numberOfColumns) {
    SummaryTable *summaryTable = malloc(sizeof(SummaryTable));
    summaryTable->table = Double_construct2DArray(numberOfRows, numberOfColumns);
    summaryTable->tablePercentage = Double_construct2DArray(numberOfRows, numberOfColumns);
    summaryTable->totalPerRow = Double_construct1DArray(numberOfRows);
    summaryTable->totalPerRowPercentage = Double_construct1DArray(numberOfRows);
    summaryTable->totalSum = 0.0;
    summaryTable->numberOfRows = numberOfRows;
    summaryTable->numberOfColumns = numberOfColumns;
    summaryTable->rowNames = NULL;
    summaryTable->columnNames = NULL;
    return summaryTable;
}

SummaryTable *SummaryTable_constructByNames(stList *rowNames, stList *columnNames) {
    SummaryTable *summaryTable = malloc(sizeof(SummaryTable));
    int numberOfRows = stList_length(rowNames);
    int numberOfColumns = stList_length(columnNames);
    summaryTable->table = Double_construct2DArray(numberOfRows, numberOfColumns);
    summaryTable->tablePercentage = Double_construct2DArray(numberOfRows, numberOfColumns);
    summaryTable->totalPerRow = Double_construct1DArray(numberOfRows);
    summaryTable->totalPerRowPercentage = Double_construct1DArray(numberOfRows);
    summaryTable->totalSum = 0.0;
    summaryTable->numberOfRows = numberOfRows;
    summaryTable->numberOfColumns = numberOfColumns;
    summaryTable->rowNames = stList_copyStringList(rowNames);
    summaryTable->columnNames = stList_copyStringList(columnNames);
    return summaryTable;
}

void SummaryTable_destruct(SummaryTable *summaryTable) {
    if (summaryTable->table != NULL) {
        Double_destruct2DArray(summaryTable->table, summaryTable->numberOfRows);
    }
    if (summaryTable->tablePercentage != NULL) {
        Double_destruct2DArray(summaryTable->tablePercentage, summaryTable->numberOfRows);
    }
    if (summaryTable->totalPerRow != NULL) {
        Double_destruct1DArray(summaryTable->totalPerRow);
    }
    if (summaryTable->totalPerRowPercentage != NULL) {
        Double_destruct1DArray(summaryTable->totalPerRowPercentage);
    }
    if (summaryTable->rowNames != NULL) {
        stList_destruct(summaryTable->rowNames);
    }
    if (summaryTable->columnNames != NULL) {
        stList_destruct(summaryTable->columnNames);
    }
    free(summaryTable);
}

void SummaryTable_increment(SummaryTable *summaryTable, int rowIndex, int columnIndex, double value) {
    if (value < 0) {
        fprintf(stderr, "[%s] Warning: summary table is taking a negative value %.2e\n", get_timestamp(), value);
    }
    summaryTable->table[rowIndex][columnIndex] += value;
    summaryTable->totalPerRow[rowIndex] += value;
    summaryTable->totalSum += value;
    // update tablePercentage
    for (int j = 0; j < summaryTable->numberOfColumns; j++) {
        if (0 < summaryTable->totalPerRow[rowIndex]) {
            summaryTable->tablePercentage[rowIndex][j] =
                    summaryTable->table[rowIndex][j] / summaryTable->totalPerRow[rowIndex] * 100.0;
        }
    }
    // update totalPerRowPercentage
    if (0 < summaryTable->totalSum) {
        for (int i = 0; i < summaryTable->numberOfRows; i++) {
            summaryTable->totalPerRowPercentage[i] = summaryTable->totalPerRow[i] / summaryTable->totalSum * 100.0;
        }
    }
}

char *SummaryTable_getRowString(SummaryTable *summaryTable, int rowIndex, char delimiter, bool addRowIndex) {
    if (summaryTable->numberOfRows <= rowIndex) {
        fprintf(stderr, "[%s] Error: row index %d cannot be greater than %d.\n",
                get_timestamp(),
                rowIndex,
                summaryTable->numberOfRows - 1);
        exit(EXIT_FAILURE);
    }
    //reset row string
    summaryTable->rowString[0] = '\0';
    // add row name first if row names exist
    if (summaryTable->rowNames != NULL) {
        char *rowName = stList_get(summaryTable->rowNames, rowIndex);
        // copy name + delimiter
        sprintf(summaryTable->rowString, "%s%c", rowName, delimiter);
    } else if (addRowIndex) {
        sprintf(summaryTable->rowString, "%d%c", rowIndex, delimiter);
    }
    char *rowString = String_joinDoubleArrayWithFormat(summaryTable->table[rowIndex],
                                                       summaryTable->numberOfColumns,
                                                       delimiter,
                                                       "%.2f");
    strcpy(summaryTable->rowString + strlen(summaryTable->rowString), rowString);
    free(rowString);
    return summaryTable->rowString;
}

char *SummaryTable_getRowStringPercentage(SummaryTable *summaryTable, int rowIndex, char delimiter, bool addRowIndex) {
    if (summaryTable->numberOfRows <= rowIndex) {
        fprintf(stderr, "[%s] Error: row index %d cannot be greater than %d.\n",
                get_timestamp(),
                rowIndex,
                summaryTable->numberOfRows - 1);
        exit(EXIT_FAILURE);
    }
    //reset row string
    summaryTable->rowString[0] = '\0';
    // add row name first if row names exist
    if (summaryTable->rowNames != NULL) {
        char *rowName = stList_get(summaryTable->rowNames, rowIndex);
        // copy name + delimiter
        sprintf(summaryTable->rowString, "%s%c", rowName, delimiter);
    } else if (addRowIndex) {
        sprintf(summaryTable->rowString, "%d%c", rowIndex, delimiter);
    }
    char *rowString = String_joinDoubleArrayWithFormat(summaryTable->tablePercentage[rowIndex],
                                                       summaryTable->numberOfColumns,
                                                       delimiter,
                                                       "%.2f");
    strcpy(summaryTable->rowString + strlen(summaryTable->rowString), rowString);
    free(rowString);
    return summaryTable->rowString;
}


char *SummaryTable_getTotalPerRowString(SummaryTable *summaryTable, char delimiter, const char *prefixEntry) {
    //reset row string
    summaryTable->rowString[0] = '\0';
    if (prefixEntry != NULL) {
        // copy entry + delimiter
        sprintf(summaryTable->rowString, "%s%c", prefixEntry, delimiter);
    }
    char *rowString = String_joinDoubleArrayWithFormat(summaryTable->totalPerRow,
                                                       summaryTable->numberOfRows,
                                                       delimiter,
                                                       "%.2f");
    strcpy(summaryTable->rowString + strlen(summaryTable->rowString), rowString);
    free(rowString);
    return summaryTable->rowString;
}

char *SummaryTable_getTotalPerRowStringPercentage(SummaryTable *summaryTable, char delimiter, const char *prefixEntry) {
    //reset row string
    summaryTable->rowString[0] = '\0';
    if (prefixEntry != NULL) {
        // copy entry + delimiter
        sprintf(summaryTable->rowString, "%s%c", prefixEntry, delimiter);
    }
    char *rowString = String_joinDoubleArrayWithFormat(summaryTable->totalPerRowPercentage,
                                                       summaryTable->numberOfRows,
                                                       delimiter,
                                                       "%.2f");
    strcpy(summaryTable->rowString + strlen(summaryTable->rowString), rowString);
    free(rowString);
    return summaryTable->rowString;
}

int SummaryTableList_getTableIndex(SummaryTableList *summaryTableList, int catIndex1, int catIndex2) {
    return catIndex1 * summaryTableList->numberOfCategories2 + catIndex2;
}

SummaryTable *SummaryTableList_getTable(SummaryTableList *summaryTableList, int catIndex1, int catIndex2) {
    int tableIndex = SummaryTableList_getTableIndex(summaryTableList, catIndex1, catIndex2);
    return stList_get(summaryTableList->summaryTables, tableIndex);
}

void SummaryTableList_increment(SummaryTableList *summaryTableList,
                                int catIndex1,
                                int catIndex2,
                                int rowIndex,
                                int columnIndex,
                                double value) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    SummaryTable_increment(summaryTable, rowIndex, columnIndex, value);
}

double SummaryTableList_getValue(SummaryTableList *summaryTableList,
                                 int catIndex1,
                                 int catIndex2,
                                 int rowIndex,
                                 int columnIndex) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return summaryTable->table[rowIndex][columnIndex];
}

double SummaryTableList_getValuePercentage(SummaryTableList *summaryTableList,
                                           int catIndex1,
                                           int catIndex2,
                                           int rowIndex,
                                           int columnIndex) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return summaryTable->tablePercentage[rowIndex][columnIndex];
}

SummaryTableList *SummaryTableList_construct(stList *categoryNames1,
                                             stList *categoryNames2,
                                             int numberOfRows,
                                             int numberOfColumns) {
    SummaryTableList *summaryTableList = malloc(sizeof(SummaryTableList));
    int numberOfCategories1 = stList_length(categoryNames1);
    int numberOfCategories2 = stList_length(categoryNames2);
    int totalNumberOfTables = numberOfCategories1 * numberOfCategories2;
    summaryTableList->numberOfCategories1 = numberOfCategories1;
    summaryTableList->numberOfCategories2 = numberOfCategories2;
    summaryTableList->totalNumberOfTables = totalNumberOfTables;
    summaryTableList->numberOfRows = numberOfRows;
    summaryTableList->numberOfColumns = numberOfColumns;
    // construct tables
    summaryTableList->summaryTables = stList_construct3(summaryTableList->totalNumberOfTables, SummaryTable_destruct);
    for (int c1 = 0; c1 < numberOfCategories1; c1++) {
        for (int c2 = 0; c2 < numberOfCategories2; c2++) {
            int tableIndex = SummaryTableList_getTableIndex(summaryTableList, c1, c2);
            SummaryTable *summaryTable = SummaryTable_construct(numberOfRows, numberOfColumns);
            stList_set(summaryTableList->summaryTables, tableIndex, summaryTable);
        }
    }
    // copy category names 1
    summaryTableList->categoryNames1 = stList_copyStringList(categoryNames1);
    // copy category names 2
    summaryTableList->categoryNames2 = stList_copyStringList(categoryNames2);
    return summaryTableList;
}

SummaryTableList *SummaryTableList_constructByNames(stList *categoryNames1,
                                                    stList *categoryNames2,
                                                    stList *rowNames,
                                                    stList *columnNames) {
    SummaryTableList *summaryTableList = malloc(sizeof(SummaryTableList));
    int numberOfRows = stList_length(rowNames);
    int numberOfColumns = stList_length(columnNames);
    int numberOfCategories1 = stList_length(categoryNames1);
    int numberOfCategories2 = stList_length(categoryNames2);
    int totalNumberOfTables = numberOfCategories1 * numberOfCategories2;
    summaryTableList->numberOfCategories1 = numberOfCategories1;
    summaryTableList->numberOfCategories2 = numberOfCategories2;
    summaryTableList->totalNumberOfTables = totalNumberOfTables;
    summaryTableList->numberOfRows = numberOfRows;
    summaryTableList->numberOfColumns = numberOfColumns;
    // construct tables
    summaryTableList->summaryTables = stList_construct3(summaryTableList->totalNumberOfTables, SummaryTable_destruct);
    for (int c1 = 0; c1 < numberOfCategories1; c1++) {
        for (int c2 = 0; c2 < numberOfCategories2; c2++) {
            int tableIndex = SummaryTableList_getTableIndex(summaryTableList, c1, c2);
            SummaryTable *summaryTable = SummaryTable_constructByNames(rowNames, columnNames);
            stList_set(summaryTableList->summaryTables, tableIndex, summaryTable);
        }
    }
    // copy category names 1
    summaryTableList->categoryNames1 = stList_copyStringList(categoryNames1);
    // copy category names 2
    summaryTableList->categoryNames2 = stList_copyStringList(categoryNames2);
    return summaryTableList;
}

char *SummaryTableList_getRowString(SummaryTableList *summaryTableList,
                                    int catIndex1,
                                    int catIndex2,
                                    int rowIndex,
                                    char delimiter,
                                    bool addRowIndex) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return SummaryTable_getRowString(summaryTable, rowIndex, delimiter, addRowIndex);
}

char *SummaryTableList_getRowStringPercentage(SummaryTableList *summaryTableList,
                                              int catIndex1,
                                              int catIndex2,
                                              int rowIndex,
                                              char delimiter,
                                              bool addRowIndex) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return SummaryTable_getRowStringPercentage(summaryTable, rowIndex, delimiter, addRowIndex);
}

char *SummaryTableList_getTotalPerRowString(SummaryTableList *summaryTableList,
                                            int catIndex1,
                                            int catIndex2,
                                            char delimiter,
                                            const char *prefixEntry) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return SummaryTable_getTotalPerRowString(summaryTable, delimiter, prefixEntry);
}

char *SummaryTableList_getTotalPerRowStringPercentage(SummaryTableList *summaryTableList,
                                                      int catIndex1,
                                                      int catIndex2,
                                                      char delimiter,
                                                      const char *prefixEntry) {
    SummaryTable *summaryTable = SummaryTableList_getTable(summaryTableList, catIndex1, catIndex2);
    return SummaryTable_getTotalPerRowStringPercentage(summaryTable, delimiter, prefixEntry);
}

void SummaryTableList_writeIntoFile(SummaryTableList *summaryTableList, FILE *fp, const char *linePrefix) {
    for (int c1 = 0; c1 < summaryTableList->numberOfCategories1; c1++) {
        char *c1Name = stList_get(summaryTableList->categoryNames1, c1);
        for (int c2 = 0; c2 < summaryTableList->numberOfCategories2; c2++) {
            char *c2Name = stList_get(summaryTableList->categoryNames2, c2);
            for (int rowIndex = 0; rowIndex < summaryTableList->numberOfRows; rowIndex++) {
                char *tableRowString = SummaryTableList_getRowString(summaryTableList,
                                                                     c1,
                                                                     c2,
                                                                     rowIndex,
                                                                     '\t',
                                                                     true);
                fprintf(fp, "%s\t%s\t%s\t%s\n", linePrefix, c1Name, c2Name, tableRowString);
            }
        }
    }
}

void SummaryTableList_writePercentageIntoFile(SummaryTableList *summaryTableList, FILE *fp, const char *linePrefix) {
    for (int c1 = 0; c1 < summaryTableList->numberOfCategories1; c1++) {
        char *c1Name = stList_get(summaryTableList->categoryNames1, c1);
        for (int c2 = 0; c2 < summaryTableList->numberOfCategories2; c2++) {
            char *c2Name = stList_get(summaryTableList->categoryNames2, c2);
            for (int rowIndex = 0; rowIndex < summaryTableList->numberOfRows; rowIndex++) {
                char *tableRowString = SummaryTableList_getRowStringPercentage(summaryTableList,
                                                                               c1,
                                                                               c2,
                                                                               rowIndex,
                                                                               '\t',
                                                                               true);
                fprintf(fp, "%s\t%s\t%s\t%s\n", linePrefix, c1Name, c2Name, tableRowString);
            }
        }
    }
}

void SummaryTableList_writeTotalPerRowIntoFile(SummaryTableList *summaryTableList, FILE *fp, const char *linePrefix) {
    for (int c1 = 0; c1 < summaryTableList->numberOfCategories1; c1++) {
        char *c1Name = stList_get(summaryTableList->categoryNames1, c1);
        for (int c2 = 0; c2 < summaryTableList->numberOfCategories2; c2++) {
            char *c2Name = stList_get(summaryTableList->categoryNames2, c2);
            char *tableRowString = SummaryTableList_getTotalPerRowString(summaryTableList,
                                                                         c1,
                                                                         c2,
                                                                         '\t',
                                                                         "ALL");
            fprintf(fp, "%s\t%s\t%s\t%s\n", linePrefix, c1Name, c2Name, tableRowString);
        }
    }
}

void SummaryTableList_writeTotalPerRowPercentageIntoFile(SummaryTableList *summaryTableList, FILE *fp,
                                                         const char *linePrefix) {
    for (int c1 = 0; c1 < summaryTableList->numberOfCategories1; c1++) {
        char *c1Name = stList_get(summaryTableList->categoryNames1, c1);
        for (int c2 = 0; c2 < summaryTableList->numberOfCategories2; c2++) {
            char *c2Name = stList_get(summaryTableList->categoryNames2, c2);
            char *tableRowString = SummaryTableList_getTotalPerRowStringPercentage(summaryTableList,
                                                                                   c1,
                                                                                   c2,
                                                                                   '\t',
                                                                                   "ALL");
            fprintf(fp, "%s\t%s\t%s\t%s\n", linePrefix, c1Name, c2Name, tableRowString);
        }
    }
}

void SummaryTableList_destruct(SummaryTableList *summaryTableList) {
    if (summaryTableList->summaryTables != NULL) stList_destruct(summaryTableList->summaryTables);
    if (summaryTableList->categoryNames1 != NULL) stList_destruct(summaryTableList->categoryNames1);
    if (summaryTableList->categoryNames2 != NULL) stList_destruct(summaryTableList->categoryNames2);
    free(summaryTableList);
}


// It will modify confusion row in place
void convertBaseLevelToOverlapBased(int *refLabelConfusionRow,
                                    int columnSize,
                                    int refLabelBlockLength,
                                    double overlapThreshold) {
    bool atLeastOneHit = false;
    for (int i = 0; i < columnSize; i++) {
        double overlapRatio = (double) refLabelConfusionRow[i] / refLabelBlockLength;
        if (overlapThreshold < overlapRatio) atLeastOneHit = true;
        refLabelConfusionRow[i] = overlapThreshold < overlapRatio ? 1 : 0;
    }
    // if no hit was found set the last column to 1
    // last column is reserved for not defined labels
    // it should rarely happen that no hit is found
    if (atLeastOneHit == false) {
        refLabelConfusionRow[columnSize - 1] = 1;
    }
}

SummaryTableUpdaterArgs *SummaryTableUpdaterArgs_construct(void *blockIterator,
                                                           void *(*copyBlockIterator)(void *),
                                                           void *(*resetBlockIterator)(void *),
                                                           void (*destructBlockIterator)(void *),
                                                           ptBlock *(*getNextBlock)(void *, char *),
                                                           SummaryTableList *summaryTableList,
                                                           IntBinArray *sizeBinArray,
                                                           int (*overlapFuncCategoryIndex1)(CoverageInfo *, int),
                                                           int categoryIndex1,
                                                           int8_t (*getRefLabelFunction)(Inference *),
                                                           int8_t (*getQueryLabelFunction)(Inference *),
                                                           bool isMetricOverlapBased,
                                                           double overlapThreshold) {
    SummaryTableUpdaterArgs *args = (SummaryTableUpdaterArgs *) malloc(1 * sizeof(SummaryTableUpdaterArgs));
    args->blockIterator = blockIterator;
    args->copyBlockIterator = copyBlockIterator;
    args->resetBlockIterator = resetBlockIterator;
    args->destructBlockIterator = destructBlockIterator;
    args->getNextBlock = getNextBlock;
    args->summaryTableList = summaryTableList;
    args->sizeBinArray = sizeBinArray;
    args->overlapFuncCategoryIndex1 = overlapFuncCategoryIndex1;
    args->categoryIndex1 = categoryIndex1;
    args->getRefLabelFunction = getRefLabelFunction;
    args->getQueryLabelFunction = getQueryLabelFunction;
    args->isMetricOverlapBased = isMetricOverlapBased;
    args->overlapThreshold = overlapThreshold;
    return args;
}

SummaryTableUpdaterArgs *SummaryTableUpdaterArgs_copy(SummaryTableUpdaterArgs *src) {
    SummaryTableUpdaterArgs *dest = (SummaryTableUpdaterArgs *) malloc(1 * sizeof(SummaryTableUpdaterArgs));
    dest->blockIterator = src->blockIterator;
    dest->copyBlockIterator = src->copyBlockIterator;
    dest->resetBlockIterator = src->resetBlockIterator;
    dest->destructBlockIterator = src->destructBlockIterator;
    dest->getNextBlock = src->getNextBlock;
    dest->summaryTableList = src->summaryTableList;
    dest->sizeBinArray = src->sizeBinArray;
    dest->overlapFuncCategoryIndex1 = src->overlapFuncCategoryIndex1;
    dest->categoryIndex1 = src->categoryIndex1;
    dest->getRefLabelFunction = src->getRefLabelFunction;
    dest->getQueryLabelFunction = src->getQueryLabelFunction;
    dest->isMetricOverlapBased = src->isMetricOverlapBased;
    dest->overlapThreshold = src->overlapThreshold;
    return dest;
}

void SummaryTableUpdaterArgs_destruct(SummaryTableUpdaterArgs *args) {
    free(args);
}


void
SummaryTableList_updateForAllCategory1(SummaryTableUpdaterArgs *argsTemplate, int sizeOfCategory1, int threads) {

    // create a thread pool
    tpool_t *tm = tpool_create(threads);
    for (int categoryIndex1 = 0; categoryIndex1 < sizeOfCategory1; categoryIndex1++) {
        // make a copy of args
        SummaryTableUpdaterArgs *argsToRun = SummaryTableUpdaterArgs_copy(argsTemplate);
        // set the category index 1 whose related summary table is going to be updated
        argsToRun->categoryIndex1 = categoryIndex1;
        // create arg struct for tpool
        work_arg_t *argWork = malloc(sizeof(work_arg_t));
        argWork->data = (void *) argsToRun;
        // Add a new job to the thread pool
        tpool_add_work(tm,
                       SummaryTableList_updateByUpdaterArgsForThreadPool,
                       (void *) argWork);
        fprintf(stderr, "[%s] Created thread for updating summary table for category1 index %d (out of range [0-%d])\n",
                get_timestamp(), categoryIndex1, sizeOfCategory1 - 1);
    }
    tpool_wait(tm);
    tpool_destroy(tm);

    fprintf(stderr, "[%s] Summary tables for all category 1 indices are updated.\n", get_timestamp());
}

void SummaryTableList_updateByUpdaterArgsForThreadPool(void *argWork_) {
    // get the arguments
    work_arg_t *argWork = argWork_;
    SummaryTableUpdaterArgs *args = argWork->data;
    SummaryTableList_updateByUpdaterArgs(args);
    SummaryTableUpdaterArgs_destruct(args);
}

// blockIterator can be created by either
// ChunkIterator_construct or ptBlockItrPerContig_construct
void SummaryTableList_updateByUpdaterArgs(SummaryTableUpdaterArgs *args) {
    // fetch the arguments
    void *(*copyBlockIterator)(void *) = args->copyBlockIterator;
    void (*resetBlockIterator)(void *) = args->resetBlockIterator;
    void (*destructBlockIterator)(void *) = args->destructBlockIterator;
    ptBlock *(*getNextBlock)(void *, char *) = args->getNextBlock;
    SummaryTableList *summaryTableList = args->summaryTableList;
    IntBinArray *sizeBinArray = args->sizeBinArray;
    int (*overlapFuncCategoryIndex1)(CoverageInfo *, int) = args->overlapFuncCategoryIndex1;
    int categoryIndex1 = args->categoryIndex1;
    int8_t(*getRefLabelFunction)(Inference * ) = args->getRefLabelFunction;
    int8_t(*getQueryLabelFunction)(Inference * ) = args->getQueryLabelFunction;
    bool isMetricOverlapBased = args->isMetricOverlapBased;
    double overlapThreshold = args->overlapThreshold;

    // make a copy of iterator and reset it
    void *blockIterator = copyBlockIterator(args->blockIterator);
    resetBlockIterator(blockIterator);

    if (summaryTableList->numberOfCategories1 <= categoryIndex1) {
        fprintf(stderr,
                "[%s] Error: annotation index (%d) for updating category 1 is greater than or equal to the size of the category (%d).",
                get_timestamp(),
                categoryIndex1,
                summaryTableList->numberOfCategories1);
        exit(EXIT_FAILURE);
    }

    // +1 for undefined query labels
    int *refLabelConfusionRow = Int_construct1DArray(summaryTableList->numberOfColumns);
    Int_fill1DArray(refLabelConfusionRow, summaryTableList->numberOfColumns, 0);

    int refLabelStart = -1;
    int preRefLabel = -1;
    int preBlockEnd = -1;
    CoverageInfo *preCoverageInfo = NULL;

    char ctg[200];
    char preCtg[200];
    preCtg[0] = '\0';

    ptBlock *block = NULL;
    while ((block = getNextBlock(blockIterator, ctg)) != NULL) {

        // get coverage info for this block
        CoverageInfo *coverageInfo = (CoverageInfo *) block->data;
        int start = block->rfs;
        int end = block->rfe;

        // get labels
        if (coverageInfo->data == NULL) {
            fprintf(stderr, "[%s] Warning: inference data does not exist for updating summary tables.\n",
                    get_timestamp());
        }
        Inference *inference = coverageInfo->data;
        int refLabel = getRefLabelFunction(inference);
        // if refLabel is -1 change it to numberOfRows - 1 since last row is for "Unk"
        refLabel = refLabel == -1 ? summaryTableList->numberOfRows - 1 : refLabel;
        int queryLabel = getQueryLabelFunction(inference);
        // if queryLabel is -1 change it to numberOfColumns - 1 since last column is for "Unk"
        queryLabel = queryLabel == -1 ? summaryTableList->numberOfColumns - 1 : queryLabel;

        // set event flags
        bool contigChanged = (preCtg[0] != '\0') && (strcmp(preCtg, ctg) != 0);
        bool refLabelChanged = refLabel != preRefLabel;

        bool annotationInCurrent = overlapFuncCategoryIndex1(coverageInfo, categoryIndex1);
        bool annotationInPrevious = overlapFuncCategoryIndex1(preCoverageInfo, categoryIndex1);
        bool annotationContinued = annotationInCurrent && annotationInPrevious;
        bool annotationStarted = annotationInCurrent && !annotationInPrevious;
        bool annotationEnded = !annotationInCurrent && annotationInPrevious;

        bool preRefLabelIsValid = preRefLabel != -1;
        /*
        fprintf(stderr, "%s %d-%d annot = %d \n", ctg, start, end, categoryIndex1);
        fprintf(stderr, "\t rlabel = %d\n", refLabel);
        fprintf(stderr, "\t qlabel = %d\n", queryLabel);
        fprintf(stderr, "\t contigChanged = %d\n", contigChanged);
        fprintf(stderr, "\t refLabelChanged = %d\n", refLabelChanged);
        fprintf(stderr, "\t annotationInCurrent = %d\n", annotationInCurrent);
        fprintf(stderr, "\t annotationInPrevious = %d\n", annotationInPrevious);
        fprintf(stderr, "\t annotationContinued = %d\n", annotationContinued);
        fprintf(stderr, "\t annotationStarted = %d\n", annotationStarted);
        fprintf(stderr, "\t annotationEnded = %d\n", annotationEnded);
        fprintf(stderr, "\t preRefLabelIsValid = %d\n", preRefLabelIsValid);
         */

        // one annotation-ref-label block has ended
        // update summary table
        if (preRefLabelIsValid && (
                (annotationContinued && refLabelChanged) ||
                (annotationInPrevious && contigChanged) ||
                annotationEnded)
                ) {
            int refLabelBlockLen = preBlockEnd - refLabelStart + 1;
            int binIndex = IntBinArray_getBinIndex(sizeBinArray, refLabelBlockLen);
            if (isMetricOverlapBased) {
                convertBaseLevelToOverlapBased(refLabelConfusionRow,
                                               summaryTableList->numberOfColumns,
                                               refLabelBlockLen,
                                               overlapThreshold);
            }
            // iterating over query labels
            for (int q = 0; q < summaryTableList->numberOfColumns; q++) {
                int catIndex1 = categoryIndex1;
                int catIndex2 = binIndex;
                int rowIndex = preRefLabel;
                int columnIndex = q;
                //fprintf(stderr, "block len = %d, [%d][%d] [%d][%d] += %d\n", blockLen, catIndex1, catIndex2, rowIndex, columnIndex, refLabelConfusionRow[q]);
                SummaryTableList_increment(summaryTableList,
                                           catIndex1,
                                           catIndex2,
                                           rowIndex,
                                           columnIndex,
                                           refLabelConfusionRow[q]);
            }
        }

        // reset confusion row and update start location
        if ((!annotationInCurrent && contigChanged) ||
            annotationEnded) {
            refLabelStart = -1;
            Int_fill1DArray(refLabelConfusionRow, summaryTableList->numberOfColumns, 0);
        }

        // reset confusion row and update start location
        if ((annotationContinued && refLabelChanged) ||
            (annotationInCurrent && contigChanged) ||
            annotationStarted) {
            refLabelStart = start;
            Int_fill1DArray(refLabelConfusionRow, summaryTableList->numberOfColumns, 0);
        }

        // update confusion row
        if (annotationInCurrent) {
            refLabelConfusionRow[queryLabel] += end - start + 1;
        }


        preCoverageInfo = coverageInfo;
        preRefLabel = refLabel;
        strcpy(preCtg, ctg);
        preBlockEnd = end;
    }

    bool preRefLabelIsValid = preRefLabel != -1;
    // check last window and update summary tables if it had overlap with annotation
    bool annotationInLastWindow = overlapFuncCategoryIndex1(preCoverageInfo, categoryIndex1);
    if (annotationInLastWindow && preRefLabelIsValid) {
        int refLabelBlockLen = preBlockEnd - refLabelStart + 1;
        int binIndex = IntBinArray_getBinIndex(sizeBinArray, refLabelBlockLen);
        if (isMetricOverlapBased) {
            convertBaseLevelToOverlapBased(refLabelConfusionRow,
                                           summaryTableList->numberOfColumns,
                                           refLabelBlockLen,
                                           overlapThreshold);
        }
        // iterating over query labels
        for (int q = 0; q < summaryTableList->numberOfColumns; q++) {
            int catIndex1 = categoryIndex1;
            int catIndex2 = binIndex;
            int rowIndex = preRefLabel;
            int columnIndex = q;
            //fprintf(stderr, "[%d][%d] [%d][%d] += %d\n", catIndex1, catIndex2, rowIndex, columnIndex, refLabelConfusionRow[q]);
            SummaryTableList_increment(summaryTableList,
                                       catIndex1,
                                       catIndex2,
                                       rowIndex,
                                       columnIndex,
                                       refLabelConfusionRow[q]);
        }
    }

    Int_destruct1DArray(refLabelConfusionRow);
    destructBlockIterator(blockIterator);
}

SummaryTableList *SummaryTableList_constructAndFillByIterator(void *blockIterator,
                                                              BlockIteratorType blockIteratorType,
                                                              stList *categoryNames,
                                                              CategoryType categoryType,
                                                              IntBinArray *sizeBinArray,
                                                              MetricType metricType,
                                                              double overlapRatioThreshold,
                                                              int numberOfLabelsWithUnknown,
                                                              stList *labelNamesWithUnknown,
                                                              ComparisonType comparisonType,
                                                              int numberOfThreads) {

    ptBlock *(*getNextBlock)(void *, char *);
    void *(*copyIterator)(void *);
    void (*destructIterator)(void *);
    void (*resetIterator)(void *);
    if (blockIteratorType == ITERATOR_BY_CHUNK) {
        copyIterator = ChunkIterator_copy;
        destructIterator = ChunkIterator_destruct;
        resetIterator = ChunkIterator_reset;
        getNextBlock = ChunkIterator_getNextPtBlock;
    } else if (blockIteratorType == ITERATOR_BY_COV_BLOCK) {
        copyIterator = ptBlockItrPerContig_copy;
        destructIterator = ptBlockItrPerContig_destruct;
        resetIterator = ptBlockItrPerContig_reset;
        getNextBlock = ptBlockItrPerContig_next;
    } else {
        fprintf(stderr, "[%s] block iterator type is not valid.\n", get_timestamp());
        exit(EXIT_FAILURE);
    }


    bool (*overlapFuncCategoryIndex1)(CoverageInfo *, int);
    if (categoryType == CATEGORY_ANNOTATION) {
        overlapFuncCategoryIndex1 = CoverageInfo_overlapAnnotationIndex;
    } else if (categoryType == CATEGORY_REGION) {
        overlapFuncCategoryIndex1 = CoverageInfo_overlapRegionIndex;
    } else {
        fprintf(stderr, "[%s] category type is not valid.\n", get_timestamp());
        exit(EXIT_FAILURE);
    }

    int8_t(*getRefLabelFunction)(Inference * );
    if (comparisonType == COMPARISON_TRUTH_VS_PREDICTION || comparisonType == COMPARISON_TRUTH_VS_TRUTH) {
        getRefLabelFunction = get_inference_truth_label;
    } else {
        getRefLabelFunction = get_inference_prediction_label;
    }


    int8_t(*getQueryLabelFunction)(Inference * );
    if (comparisonType == COMPARISON_TRUTH_VS_PREDICTION || comparisonType == COMPARISON_PREDICTION_VS_PREDICTION) {
        getQueryLabelFunction = get_inference_prediction_label;
    } else {
        getQueryLabelFunction = get_inference_truth_label;
    }

    // create summary table list
    stList *categoryNames1 = categoryNames;
    stList *categoryNames2 = sizeBinArray->names;

    int sizeOfCategory1 = stList_length(categoryNames1);
    int numberOfRows = numberOfLabelsWithUnknown;
    int numberOfColumns = numberOfLabelsWithUnknown;
    SummaryTableList *summaryTableList;
    if (labelNamesWithUnknown == NULL) {
        summaryTableList = SummaryTableList_construct(categoryNames1,
                                                      categoryNames2,
                                                      numberOfRows,
                                                      numberOfColumns);
    } else {
        summaryTableList = SummaryTableList_constructByNames(categoryNames1,
                                                             categoryNames2,
                                                             labelNamesWithUnknown,
                                                             labelNamesWithUnknown);
    }

    // create a template of update args with category 1 index set to -1
    bool isMetricOverlapBased = metricType == METRIC_OVERLAP_BASED;
    SummaryTableUpdaterArgs *argsTemplate = SummaryTableUpdaterArgs_construct((void *) blockIterator,
                                                                              copyIterator,
                                                                              resetIterator,
                                                                              destructIterator,
                                                                              getNextBlock,
                                                                              summaryTableList,
                                                                              sizeBinArray,
                                                                              overlapFuncCategoryIndex1,
                                                                              -1,
                                                                              getRefLabelFunction,
                                                                              getQueryLabelFunction,
                                                                              isMetricOverlapBased,
                                                                              overlapRatioThreshold);
    // update all tables with multi-threading
    SummaryTableList_updateForAllCategory1(argsTemplate, sizeOfCategory1, numberOfThreads);

    SummaryTableUpdaterArgs_destruct(argsTemplate);

    return summaryTableList;
}


