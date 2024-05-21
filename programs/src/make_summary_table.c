#include <getopt.h>
#include <time.h>
#include "bgzf.h"
#include "sonLib.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include "common.h"
#include <time.h>
#include <string.h>
#include "ptBlock.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include "stdlib.h"
#include "sonLib.h"
#include "chunk.h"
#include "track_reader.h"
#include "summary_table.h"


static struct option long_options[] =
        {
                {"input",                 required_argument, NULL, 'i'},
                {"binArrayFile",          required_argument, NULL, 'b'},
                {"output",                required_argument, NULL, 'o'},
                {"threads",               required_argument, NULL, 't'},
                {"overlapRatioThreshold", required_argument, NULL, 'v'},
                {NULL,                    0,                 NULL, 0}
        };


int main(int argc, char *argv[]) {
    int c;
    double overlapRatioThreshold = 0.4;
    char *inputPath = NULL;
    char *outputPath = NULL;
    char *binArrayFilePath = NULL;
    int threads = 4;
    char *program;
    (program = strrchr(argv[0], '/')) ? ++program : (program = argv[0]);
    while (~(c = getopt_long(argc, argv, "i:o:b:t:v:h", long_options, NULL))) {
        switch (c) {
            case 'i':
                inputPath = optarg;
                break;
            case 'o':
                outputPath = optarg;
                break;
            case 'b':
                binArrayFilePath = optarg;
                break;
            case 't':
                threads = atoi(optarg);
                break;
            case 'v':
                overlapRatioThreshold = atof(optarg);
                break;
            default:
                if (c != 'h') fprintf(stderr, "[E::%s] undefined option %c\n", __func__, c);
            help:
                fprintf(stderr, "\nUsage: %s  -i <INPUT_FILE> -b <BIN_ARRAY_TSV> -o <OUTPUT_FILE> \n", program);
                fprintf(stderr, "Options:\n");
                fprintf(stderr,
                        "         -i,--input         input path (can have formats '.cov', '.cov.gz', '.bed', '.bed.gz' or '.bin')\n");
                fprintf(stderr,
                        "         -b,--binArrayFile         a tsv file (tab-delimited) that contains bin arrays for stratifying results by event size. "
                        "It should contain three columns. 1st column is the closed start of the bin and the 2nd column is the open end."
                        "The 3rd column has a name for each bin. For example one row can be '0\t100\t[0-100). If no file is passed it will"
                        "consider one large bin as the default value. (Default = [0,1e9) with the name 'ALL')\n");
                fprintf(stderr,
                        "         -o,--output         output path for saving summary table (should have format '.tsv')\n");
                fprintf(stderr,
                        "         -v, --overlapRatioThreshold                        Minimum overlap ratio in calculating overlap-based metrics for considering a hit between a ref label "
                        "(for example truth label for recall) and query label (for example prediction label for recall) [default: 0.4]\n");
                fprintf(stderr,
                        "         -t, --threads                        Number of threads for parallelizing creating table for each annotation/region [default: 4]\n");
                return 1;
        }
    }

    double realtimeStart = System_getRealTimePoint();

    if (inputPath == NULL || outputPath == NULL) {
        fprintf(stderr, "[%s] Error: Input/Output path cannot be NULL.\n", get_timestamp());
        exit(EXIT_FAILURE);
    }

    // check input/output file extensions
    char *inputExtension = extractFileExtension(inputPath);
    char *outputExtension = extractFileExtension(outputPath);
    if (strcmp(outputExtension, "tsv") != 0) {
        fprintf(stderr, "[%s] Error: output file should have tsv extension  !\n", get_timestamp());
        free(inputExtension);
        free(outputExtension);
        exit(EXIT_FAILURE);
    }
    if (strcmp(inputExtension, "cov") != 0 &&
        strcmp(inputExtension, "cov.gz") != 0 &&
        strcmp(inputExtension, "bed") != 0 &&
        strcmp(inputExtension, "bed.gz") != 0 &&
        strcmp(inputExtension, "bin") != 0) {
        fprintf(stderr,
                "[%s] Error: input file should either cov/cov.gz/bed/bed.gz or a binary file made with create_bin_chunks.\n",
                get_timestamp());
        free(inputExtension);
        free(outputExtension);
        exit(EXIT_FAILURE);
    }

    // define object and functions for iterating blocks
    void *iterator;
    CoverageHeader *header = NULL;
    ChunksCreator *chunksCreator = NULL;
    stHash *blocksPerContig = NULL;
    BlockIteratorType blockIteratorType =
            strcmp(inputExtension, "bin") == 0 ? ITERATOR_BY_CHUNK : ITERATOR_BY_COV_BLOCK;

    // create block iterator
    if (blockIteratorType == ITERATOR_BY_CHUNK) { // for binary file
        chunksCreator = ChunksCreator_constructEmpty();
        ChunkCreator_parseChunksFromBinaryFile(chunksCreator, inputPath);
        iterator = (void *) ChunkIterator_construct(chunksCreator);
        header = chunksCreator->header;
    } else if (blockIteratorType == ITERATOR_BY_COV_BLOCK) { // for cov or bed file
        blocksPerContig = ptBlock_parse_coverage_info_blocks(inputPath);
        iterator = (void *) ptBlockItrPerContig_construct(blocksPerContig);
        header = CoverageHeader_construct(inputPath);
    }

    // parse bin intervals
    IntBinArray *binArray = IntBinArray_constructFromFile(binArrayFilePath);

    // open file for writing summary tables
    FILE *fout = fopen(outputPath, "w");
    if (fout == NULL) {
        fprintf(stderr, "[%s] Error: %s cannot be opened.\n", get_timestamp(), outputPath);
        exit(EXIT_FAILURE);
    }


    char linePrefix[1000];
    int numberOfLabels = header->numberOfLabels;
    double overlapRatioThreshold = 0.4;
    if (header->isTruthAvailable && header->isPredictionAvailable) {
        // get a summary table for base-level precision across all annotation and size bins
        SummaryTableList *summaryTableListAnnotationBaseLevelPrecision =
                SummaryTableList_constructAndFillByIterator(iterator,
                                                            blockIteratorType,
                                                            header->annotationNames,
                                                            CATEGORY_ANNOTATION,
                                                            binArray,
                                                            METRIC_BASE_LEVEL,
                                                            overlapRatioThreshold,
                                                            numberOfLabels,
                                                            COMPARISON_PREDICTION_VS_TRUTH,
                                                            threads);
        sprintf(linePrefix, "precision\tbase-level\tannotation");
        SummaryTableList_writeIntoFile(summaryTableListAnnotationBaseLevelPrecision, fout, linePrefix);
        SummaryTableList_destruct(summaryTableListAnnotationBaseLevelPrecision);
/*
        // get a summary table for base-level recall across all annotation and size bins
        SummaryTableList *summaryTableListAnnotationBaseLevelRecall =
                SummaryTableList_constructAndFillByIterator(iterator,
                                                            blockIteratorType,
                                                            header->annotationNames,
                                                            CATEGORY_ANNOTATION,
                                                            binArray,
                                                            METRIC_BASE_LEVEL,
                                                            overlapRatioThreshold,
                                                            numberOfLabels,
                                                            COMPARISON_TRUTH_VS_PREDICTION,
                                                            threads);*/
    }

    // close file
    fclose(fout);

    // free memory
    free(outputExtension);
    free(inputExtension);
    IntBinArray_destruct(binArray);

    if (blockIteratorType == ITERATOR_BY_CHUNK) {
        // free iterator
        ChunkIterator_destruct((ChunkIterator *) iterator);
        // free chunks
        ChunksCreator_destruct(chunksCreator);
    }else{
        // free iterator
        ptBlockItrPerContig_destruct((ptBlockItrPerContig *) iterator);
        // free blocks
        stHash_destruct(blocksPerContig);
        // free header
        CoverageHeader_destruct(header);
    }
    fprintf(stderr, "[%s] Done!\n", get_timestamp());

    // log used time/resources
    double realtime = System_getRealTimePoint() - realtimeStart;
    double cputime = System_getCpuTime();
    double rssgb = System_getPeakRSSInGB();
    double usage = System_getCpuUsage(cputime, realtime);
    // copied from https://github.com/chhylp123/hifiasm/blob/70fd9a0b1fea45e442eb5f331922ea91ef4f71ae/main.cpp#L73
    fprintf(stderr, "Real time: %.3f sec; CPU: %.3f sec; Peak RSS: %.3f GB; CPU usage: %.1f\%\n", realtime, cputime,
            rssgb, usage * 100.0);
}
