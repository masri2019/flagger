#include "track_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

bool testParsingCov(char *covPath) {
    bool correct = true;
    int truthValuesCtg1[7][6] = {{1,  10,  4,  4,  4,  0},
                                 {11, 15,  6,  0,  0,  0},
                                 {16, 20,  6,  0,  0,  1},
                                 {21, 60,  10, 10, 10, 1},
                                 {61, 64,  14, 14, 10, 1},
                                 {65, 70,  14, 14, 10, 0},
                                 {71, 110, 16, 16, 16, 1}};
    int truthValuesCtg2[2][6] = {{1, 2,  4, 4, 4, 1},
                                 {3, 10, 8, 8, 0, 0}};
    int trackIndexCtg1 = -1;
    int trackIndexCtg2 = -1;
    bool zeroBasedCoors = false;
    TrackReader *trackReader = TrackReader_construct(covPath, NULL, zeroBasedCoors);
    fprintf(stderr, "1\n");
    while (0 < TrackReader_next(trackReader)) {
        fprintf(stderr, "2\n");
        fprintf(stderr, "%s:%d-%d\n", trackReader->ctg, trackReader->s, trackReader->e);
        if (strcmp(trackReader->ctg, "ctg1") == 0) {
            trackIndexCtg1 += 1;
            int *truthValues = truthValuesCtg1[trackIndexCtg1];
            if (trackReader->attrbsLen != 5) {
                fprintf(stderr, "Number of parsed attributes per track %d does not match truth (5)\n",
                        trackReader->attrbsLen);
                TrackReader_destruct(trackReader);
                return false;
            }
            correct &= (trackReader->s == truthValues[0]);
            correct &= (trackReader->e == truthValues[1]);
            correct &= (atoi(trackReader->attrbs[0]) == truthValues[2]);
            correct &= (atoi(trackReader->attrbs[1]) == truthValues[3]);
            correct &= (atoi(trackReader->attrbs[2]) == truthValues[4]); // skip annotation column for now
            correct &= (atoi(trackReader->attrbs[4]) == truthValues[5]);
        }
        if (strcmp(trackReader->ctg, "ctg2") == 0) {
            trackIndexCtg2 += 1;
            int *truthValues = truthValuesCtg2[trackIndexCtg2];
            if (trackReader->attrbsLen != 5) {
                fprintf(stderr, "Number of parsed attributes per track %d does not match truth (5)\n",
                        trackReader->attrbsLen);
                TrackReader_destruct(trackReader);
                return false;
            }
            correct &= (trackReader->s == truthValues[0]);
            correct &= (trackReader->e == truthValues[1]);
            correct &= (atoi(trackReader->attrbs[0]) == truthValues[2]);
            correct &= (atoi(trackReader->attrbs[1]) == truthValues[3]);
            correct &= (atoi(trackReader->attrbs[2]) == truthValues[4]);
            correct &= (atoi(trackReader->attrbs[4]) == truthValues[5]);
        }
        fprintf(stderr, "**2\n");
    }
    fprintf(stderr, "***2\n");
    TrackReader_destruct(trackReader);
    return correct;
}


bool test_CoverageHeader_write_and_read_compressed(const char *outputPath) {
    // create header and write into file
    CoverageHeader *header1 = CoverageHeader_construct(NULL);
    stList_append(header1->headerLines, copyString("#annotation:len:2"));
    stList_append(header1->headerLines, copyString("#annotation:0:no_annotation"));
    stList_append(header1->headerLines, copyString("#annotation:1:annotation_1"));

    bool isCompressed = true;
    gzFile fp = gzopen(outputPath, "w6h");
    CoverageHeader_writeIntoFile(header1, (void *) &fp, isCompressed);
    gzclose(fp);

    // read the created file
    CoverageHeader *header2 = CoverageHeader_construct(outputPath);
    if (stList_length(header2->headerLines) != stList_length(header1->headerLines)) return false;
    bool correct = true;
    for (int i = 0; i < 3; i++) {
        correct &= (
                strcmp((char *) stList_get(header1->headerLines, i), (char *) stList_get(header2->headerLines, i)) ==
                0);
    }

    CoverageHeader_destruct(header1);
    CoverageHeader_destruct(header2);
    return correct;
}

bool test_CoverageHeader_write_and_read_uncompressed(const char *outputPath) {
    // create header and write into file
    CoverageHeader *header1 = CoverageHeader_construct(NULL);
    stList_append(header1->headerLines, copyString("#annotation:len:2"));
    stList_append(header1->headerLines, copyString("#annotation:0:no_annotation"));
    stList_append(header1->headerLines, copyString("#annotation:1:annotation_1"));

    bool isCompressed = false;
    FILE *fp = fopen(outputPath, "w");
    ptBlock_print_headers_stList(headerLines, (void *) fp, isCompressed);
    fclose(fp);

    // read the created file
    CoverageHeader *header2 = CoverageHeader_construct(outputPath);
    if (stList_length(header2->headerLines) != stList_length(header1->headerLines)) return false;
    bool correct = true;
    for (int i = 0; i < 3; i++) {
        correct &= (
                strcmp((char *) stList_get(header1->headerLines, i), (char *) stList_get(header2->headerLines, i)) ==
                0);
    }

    CoverageHeader_destruct(header1);
    CoverageHeader_destruct(header2);
    return correct;
}

bool test_CoverageHeader_createByAttributes(const char *outputPath) {
    stList *annotationNames = stList_construct3(0, free);
    stList_append(annotationNames, copyString("no_annotation"));
    stList_append(annotationNames, copyString("annotation_1"));

    int numberOfRegions = 3;
    int regionCoverages[3];
    regionCoverages[0] = 5;
    regionCoverages[1] = 10;
    regionCoverages[2] = 25;

    bool isCompressed = false;
    bool isTruthAvailable = false;
    bool isPredictionAvailable = false;
    int numberOfLabels = 0;
    FILE *fp = fopen(outputPath, "w");
    CoverageHeader *header1 = CoverageHeader_constructByAttributes(annotationNames,
                                                                   regionCoverages,
                                                                   numberOfRegions,
                                                                   numberOfLabels,
                                                                   isTruthAvailable,
                                                                   isPredictionAvailable);
    CoverageHeader_writeIntoFile(header1, (void *) fp, isCompressed);
    fclose(fp);
    CoverageHeader_destruct(header1);


    CoverageHeader *header2 = CoverageHeader_construct(outputPath);
    if (stList_length(header2->annotationNames) != stList_length(annotationNames)) return false;
    if (header2->numberOfAnnotations != stList_length(annotationNames)) return false;
    bool correct = true;
    for (int i = 0; i < stList_length(header2->annotationNames); i++) {
        correct &= (strcmp((char *) stList_get(header2->annotationNames, i), (char *) stList_get(annotationNames, i)) ==
                    0);
    }

    if (header2->numberOfRegions != numberOfRegions) return false;
    for (int i = 0; i < numberOfRegions; i++) {
        correct &= header2->regionCoverages[i] == regionCoverages[i];
    }

    correct &= header2->numberOfLabels != numberOfLabels;
    correct &= header2->isTruthAvailable != isTruthAvailable;
    correct &= header2->isPredictionAvailable != isPredictionAvailable;

    stList_destruct(annotationNames);
    CoverageHeader_destruct(header1);
    CoverageHeader_destruct(header2);
    return correct;
}

int main(int argc, char *argv[]) {

    bool all_tests_passed = true;


    // test 1
    bool testParsingCov_passed = testParsingCov("tests/test_files/test_1.cov");
    all_tests_passed &= testParsingCov_passed;
    printf("Test parsing cov with TrackReader for uncompressed file:");
    printf(testParsingCov_passed ? "\x1B[32m OK \x1B[0m\n" : "\x1B[31m FAIL \x1B[0m\n");

    // test 2
    bool testParsingCovGz_passed = testParsingCov("tests/test_files/test_1.cov.gz");
    all_tests_passed &= testParsingCovGz_passed;
    printf("Test parsing cov with TrackReader for compressed file:");
    printf(testParsingCovGz_passed ? "\x1B[32m OK \x1B[0m\n" : "\x1B[31m FAIL \x1B[0m\n");


    // test 3
    bool test_CoverageHeader_write_and_read_compressed_passed = test_CoverageHeader_write_and_read_compressed(
            "tests/test_files/test_header_1.cov.gz");
    all_tests_passed &= test_CoverageHeader_write_and_read_compressed_passed;
    printf("Test test_CoverageHeader_write_and_read_compressed:");
    printf(test_CoverageHeader_write_and_read_compressed_passed ? "\x1B[32m OK \x1B[0m\n" : "\x1B[31m FAIL \x1B[0m\n");

    // test 4
    bool test_CoverageHeader_write_and_read_uncompressed_passed = test_CoverageHeader_write_and_read_uncompressed(
            "tests/test_files/test_header_1.cov");
    all_tests_passed &= test_CoverageHeader_write_and_read_uncompressed_passed;
    printf("Test test_CoverageHeader_write_and_read_uncompressed:");
    printf(test_CoverageHeader_write_and_read_uncompressed_passed ? "\x1B[32m OK \x1B[0m\n"
                                                                  : "\x1B[31m FAIL \x1B[0m\n");

    // test 5
    bool test_CoverageHeader_createByAttributes_passed = test_CoverageHeader_createByAttributes(
            "tests/test_files/test_header_2.cov");
    all_tests_passed &= test_CoverageHeader_createByAttributes_passed;
    printf("Test test_CoverageHeader_createByAttributes:");
    printf(test_CoverageHeader_createByAttributes_passed ? "\x1B[32m OK \x1B[0m\n"
                                                         : "\x1B[31m FAIL \x1B[0m\n");

    if (all_tests_passed)
        return 0;
    else
        return 1;

}
