//
// Created by mobin on 9/22/23.
//

#include <getopt.h>
#include "sam.h"
#include "faidx.h"
#include <time.h>
#include "bgzf.h"
#include <regex.h>
#include "sonLib.h"
#include <assert.h>
#include <math.h>
#include <float.h>
#include "cigar_it.h"
#include "common.h"
#include "vcf.h"
#include "ptBlock.h"
#include "ptAlignment.h"
#include <time.h>
#include <string.h>
#include "ptBlock.h"
#include "ptAlignment.h"
#include "tpool.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
#include "cJSON.h"

typedef struct ArgumentsCovExt {
    stHash* coverage_blocks_per_contig;
    stHash* ref_blocks_per_contig_to_parse;
    char* bam_path;
    pthread_mutex_t *mutexPtr;
    int min_mapq;
    double min_clipping_ratio;
} ArgumentsCovExt;

// for one thread of parsing alignments
void update_coverage_blocks_with_alignments(void * arg_){
    // get the arguments
    work_arg_t *arg = arg_;
    ArgumentsCovExt *argsCovExt = arg->data;
    stHash *coverage_blocks_per_contig = argsCovExt->coverage_blocks_per_contig;
    stHash *ref_blocks_per_contig_to_parse = argsCovExt->ref_blocks_per_contig_to_parse;
    char *bam_path = argsCovExt->bam_path;
    pthread_mutex_t *mutexPtr = argsCovExt->mutexPtr;
    int min_mapq = argsCovExt->min_mapq;
    double min_clipping_ratio = argsCovExt->min_clipping_ratio;

    //open bam file
    samFile *fp = sam_open(bam_path, "r");
    sam_hdr_t *sam_hdr = sam_hdr_read(fp);
    bam1_t *b = bam_init1();
    // load the bam index
    hts_idx_t * sam_idx = sam_index_load(fp, bam_path);
    char* ctg_name;

    // iterate over all contigs for this batch
    stHashIterator *it = stHash_getIterator(ref_blocks_per_contig_to_parse);
    while ((ctg_name = stHash_getNext(it)) != NULL) {
        stList* ref_blocks_to_parse = stHash_search(ref_blocks_per_contig_to_parse, ctg_name);
        // get the contig id
        int tid = sam_hdr_name2tid(sam_hdr, ctg_name);
        // iterate over all blocks in this contig
        for(int i = 0; i < stList_length(ref_blocks_to_parse); i++){
            ptBlock * block = stList_get(ref_blocks_to_parse, i);
	    fprintf(stderr, "[%s] Started parsing reads in the block: %s\t%d\t%d\n", get_timestamp(), ctg_name, block->rfs, block->rfe+1);
            // iterate over all alignments overlapping this block
            hts_itr_t * sam_itr = sam_itr_queryi(sam_idx, tid, block->rfs, block->rfe);
            int bytes_read;
            while (sam_itr != NULL) {
                bytes_read = sam_itr_next(fp, sam_itr, b);
                if (bytes_read <= -1)break;
                if (b->core.flag & BAM_FUNMAP) continue; // skip unmapped
                if ((b->core.flag & BAM_FSECONDARY) > 0) continue; // skip secondary alignments
                if (b->core.pos < block->rfs) continue; // make sure the alignment starts after block->rfs
                ptAlignment * alignment = ptAlignment_construct(b, sam_hdr);
                // lock the mutex, add the coverage block and unlock the mutex
                pthread_mutex_lock(mutexPtr);
                bool init_count_data = true;
                ptBlock_add_alignment_as_CoverageInfo(coverage_blocks_per_contig,
                                                      alignment,
                                                      min_mapq,
                                                      min_clipping_ratio);
                pthread_mutex_unlock(mutexPtr);
            }
	    if (sam_itr != NULL) hts_itr_destroy(sam_itr);
        }
    }
    stHash_destructIterator(it);
    hts_idx_destroy(sam_idx);
    sam_hdr_destroy(sam_hdr);
    sam_close(fp);
    bam_destroy1(b);
}

// make a block table that covers the whole reference sequences
stHash* ptBlock_get_whole_genome_blocks_per_contig(char* bam_path){
    stHash *blocks_per_contig = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, free,
                                                        (void (*)(void *)) stList_destruct);
    samFile *fp = sam_open(bam_path, "r");
    sam_hdr_t *sam_hdr = sam_hdr_read(fp);
    int64_t total_len = 0;
    for(int i = 0; i < sam_hdr->n_targets; i++){
        char* ctg_name = sam_hdr->target_name[i];
        int ctg_len = sam_hdr->target_len[i];
        ptBlock * block = ptBlock_construct(0, ctg_len-1,
                                            -1, -1,
                                            -1, -1);
        stList* block_list = stList_construct3(0, ptBlock_destruct);
        stList_append(block_list, block);
        stHash_insert(blocks_per_contig, copyString(ctg_name), block_list);
        total_len += ctg_len;
    }
    fprintf("[%s] Size of the whole genome = %ld (n=%d)\n", get_timestamp(), total_len, sam_hdr->n_targets);
    sam_hdr_destroy(sam_hdr);
    sam_close(fp);
    return blocks_per_contig;
}

stHash* ptBlock_multi_threaded_coverage_extraction(char* bam_path,
                                                   int threads,
                                                   int min_mapq,
                                                   double min_clipping_ratio){
    stHash* whole_genome_blocks_per_contig = ptBlock_get_whole_genome_blocks_per_contig(bam_path);
    stList* block_batches = ptBlock_split_into_batches(whole_genome_blocks_per_contig, threads);
    stHash *coverage_blocks_per_contig = stHash_construct3(stHash_stringKey, stHash_stringEqualKey, NULL,
                                                           (void (*)(void *)) stList_destruct);
    // create a thread pool
    tpool_t *tm = tpool_create(threads);
    pthread_mutex_t *mutexPtr = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(mutexPtr, NULL);
    for(int i=0; i < stList_length(block_batches); i++) {
        stHash* batch = stList_get(block_batches, i);
        // make the args struct to pass to the function that
        // is going to be run in each thread
        ArgumentsCovExt *argsCovExt = malloc(sizeof(ArgumentsCovExt));
        argsCovExt->coverage_blocks_per_contig = coverage_blocks_per_contig;
        argsCovExt->ref_blocks_per_contig_to_parse = batch;
        argsCovExt->bam_path = bam_path;
        argsCovExt->mutexPtr = mutexPtr;
        argsCovExt->min_mapq = min_mapq;
        argsCovExt->min_clipping_ratio = min_clipping_ratio;
        work_arg_t *arg = malloc(sizeof(work_arg_t));
        arg->data = (void*) argsCovExt;
        // Add a new job to the thread pool
        tpool_add_work(tm,
                       update_coverage_blocks_with_alignments,
                       (void*) arg);
        fprintf(stderr, "[%s] Created thread for parsing batch %d (length=%ld)\n",get_timestamp(), i, ptBlock_get_total_length_by_rf(batch));
    }
    tpool_wait(tm);
    tpool_destroy(tm);

    fprintf(stderr, "[%s] All batches are parsed.\n", get_timestamp());

    pthread_mutex_destroy(mutexPtr);


    stHash_destruct(whole_genome_blocks_per_contig);
    stList_destruct(block_batches);

    fprintf(stderr, "[%s] Started sorting and merging blocks.\n", get_timestamp());
    //sort 
    ptBlock_sort_stHash_by_rfs(coverage_blocks_per_contig);
    //merge
    stHash * coverage_blocks_per_contig_merged = ptBlock_merge_blocks_per_contig_by_rf_v2(coverage_blocks_per_contig);
    fprintf(stderr, "[%s] Merging coverage blocks is done.\n", get_timestamp());

    // free unmerged blocks
    stHash_destruct(coverage_blocks_per_contig);

    return coverage_blocks_per_contig_merged;
}


stList* parse_all_annotations_and_save_in_stList(char* json_path){
    int buffer_size = 0;
    char* json_buffer = read_whole_file(json_path, &buffer_size, "r");
    fwrite(json_buffer,1,buffer_size,stderr);
    cJSON *annotation_json = cJSON_ParseWithLength(json_buffer, buffer_size);
    if (annotation_json == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        return NULL;
    }

    int annotation_count = cJSON_GetArraySize(annotation_json);
    stList* block_table_list = stList_construct3(annotation_count, stHash_destruct);

    // iterate over key-values in json
    // each key is an index
    // each value is a path to a bed file
    cJSON *element = NULL;
    cJSON_ArrayForEach(element, annotation_json)
    {
	if (cJSON_IsString(element)){
            char* bed_path = cJSON_GetStringValue(element);
            stHash *annotation_block_table = ptBlock_parse_bed(bed_path);
	    fprintf(stderr, "[%s] Parsed  annotation %s:%s\n", get_timestamp(), element->string, cJSON_GetStringValue(element));
            int index = atoi(element->string) - 1; // given index is 1-based
            stList_set(block_table_list, index, annotation_block_table);
        }
    }
    cJSON_Delete(annotation_json);
    fprintf(stderr, "[%s] Number of parsed annotations = %d\n", get_timestamp(), stList_length(block_table_list));
    ptBlock_print_blocks_stHash(stList_get(block_table_list, 0), false, stderr, false);
    return block_table_list;

}

// annotation block tables need to have a CoverageInfo object as their "data"
// this CoverageInfo will have zero values for the coverage related attributes
// the annotation flag is set based on the order of the annotation blocks in the 
// given stList as the input
// The data augmentation is happening in place
void add_coverage_info_to_all_annotation_block_tables(stList *block_table_list){
    for(int i=0; i < stList_length(block_table_list); i++){
        stHash * block_per_contig = stList_get(block_table_list, i);
        // Each annotation has an associated flag represented by a bit-vector
        // the size of the bit-vector is 32, so it can be saved in an int32_t variable
        // for example for i=0 -> flag = 1 and for i=6 -> flag= 64
        int32_t annotation_flag = 1 << i;
        CoverageInfo * cov_info = CoverageInfo_construct(annotation_flag, 0, 0, 0);
        // The cov_info object created above will be copied and added as "data" to all blocks
        // for the current annotation. This process is happening in place
        ptBlock_add_data_to_all_blocks_stHash(block_per_contig,
                                              cov_info,
                                              destruct_cov_info_data,
                                              copy_cov_info_data,
                                              extend_cov_info_data);
    }
}

int main(int argc, char *argv[]) {
    int c;
    int min_mapq = 20;
    double min_clipping_ratio = 0.1;
    int threads=4;
    char *bam_path;
    char *out_path;
    char *json_path;
    char *out_type;
    char *program;
    (program = strrchr(argv[0], '/')) ? ++program : (program = argv[0]);
    while (~(c = getopt(argc, argv, "i:t:j:m:r:O:o:h"))) {
        switch (c) {
            case 'i':
                bam_path = optarg;
                break;
            case 't':
                threads = atoi(optarg);
                break;
            case 'o':
                out_path = optarg;
                break;
            case 'j':
                json_path = optarg;
                break;
            case 'm':
                min_mapq = atoi(optarg);
                break;
            case 'r':
                min_clipping_ratio = atof(optarg);
                break;
            case 'O':
                out_type = optarg;
                break;
            default:
                if (c != 'h') fprintf(stderr, "[E::%s] undefined option %c\n", __func__, c);
            help:
                fprintf(stderr, "\nUsage: %s  -i <BAM_FILE> -t <THREADS> -o <OUT_BED_FILE> \n", program);
                fprintf(stderr, "Options:\n");
                fprintf(stderr, "         -i         input bam file (should be indexed)\n");
                fprintf(stderr, "         -j         JSON file for the annotation bed files [maximum 32 files can be given and the keys can be any number between 1-32 for example {\"1\":\"/path/to/1.bed\", \"2\":\"/path/to/2.bed\"}]\n");
                fprintf(stderr, "         -m         minimum mapq for the measuring the coverage of the alignments with high mapq [Default = 20]\n");
                fprintf(stderr, "         -r         minimum clipping ratio for the measuring the coverage of the highly clipped alignments [Default = 0.1]\n");
                fprintf(stderr, "         -t         number of threads [default: 4]\n");
                fprintf(stderr, "         -O         output type [\"b\" for bed, \"bz\" for gzipped bed, \"c\" for cov, \"cz\" for gzipped cov]\n");
                fprintf(stderr, "         -o         output path \n");
                return 1;
        }
    }

    // parse annotation bed files
    stList* annotation_block_table_list = parse_all_annotations_and_save_in_stList(json_path);

    // add coverage info objects as data to all annotation blocks
    // each coverage info will contain only the related annotation flag with 0 coverage
    add_coverage_info_to_all_annotation_block_tables(annotation_block_table_list);

    // parse alignments and make a block table that contains the necessary coverage values per block
    // the coverage values will be related to the total alignments, alignments with high mapq and
    // each block in the output table is a maximal contiguous block with no change in the depth of coverage
    stHash* coverage_block_table = ptBlock_multi_threaded_coverage_extraction(bam_path,
                                                                              threads,
                                                                              min_mapq,
                                                                              min_clipping_ratio);
    // print len/number stats for the coverage block table
    fprintf(stderr, "[%s] Created block table with coverage data : tot_len=%ld, number=%ld\n", 
		    ptBlock_get_total_length_by_rf(coverage_block_table), 
		    ptBlock_get_total_number(coverage_block_table));

    // cover the whole genome with blocks that have zero coverage
    // this is useful to save the blocks with no coverage
    stHash* whole_genome_block_table = ptBlock_get_whole_genome_blocks_per_contig(bam_path);
    // print len/number stats for the whole genome block table
    fprintf(stderr, "[%s] Created block table for whole genome  : tot_len=%ld, number=%ld\n", 
		    ptBlock_get_total_length_by_rf(whole_genome_block_table), 
		    ptBlock_get_total_number(whole_genome_block_table));

    CoverageInfo * cov_info = CoverageInfo_construct(0, 0, 0, 0);
    // The cov_info object created above will be copied and added as "data" to all whole genome blocks
    // This process is happening in place.
    ptBlock_add_data_to_all_blocks_stHash(whole_genome_block_table,
                                          cov_info,
                                          destruct_cov_info_data,
                                          copy_cov_info_data,
                                          extend_cov_info_data);

    // print len/number stats for the whole genome block table
    fprintf(stderr, "[%s] Created block table for whole genome  : tot_len=%ld, number=%ld\n", 
		    ptBlock_get_total_length_by_rf(whole_genome_block_table), 
		    ptBlock_get_total_number(whole_genome_block_table));
    // add annotation and coverage blocks to the whole genome blocks in place
    ptBlock_extend_block_tables(whole_genome_block_table, coverage_block_table);
    fprintf(stderr, "[%s]  Added 0-coverage whole genome blocks to coverage block tables : tot_len=%ld, number=%ld\n", 
		    ptBlock_get_total_length_by_rf(whole_genome_block_table), 
		    ptBlock_get_total_number(whole_genome_block_table));
    for(int i=0; i < stList_length(annotation_block_table_list); i++){
        ptBlock_extend_block_tables(whole_genome_block_table, stList_get(annotation_block_table_list, i));
    }
    fprintf(stderr, "[%s] Added annotation blocks to coverage block tables: \n", get_timestamp(), 
		    ptBlock_get_total_length_by_rf(whole_genome_block_table),
		    ptBlock_get_total_number(whole_genome_block_table));

    fprintf(stderr, "[%s] Started sorting and merging blocks\n", get_timestamp());
    //sort
    ptBlock_sort_stHash_by_rfs(whole_genome_block_table);
    fprintf(stderr, "[%s] Merged blocks : tot_len=%ld, number=%ld\n", get_timestamp(),
		    ptBlock_get_total_length_by_rf(whole_genome_block_table), 
		    ptBlock_get_total_number(whole_genome_block_table));

    //merge and create the final block table
    stHash * final_block_table = ptBlock_merge_blocks_per_contig_by_rf_v2(whole_genome_block_table);

    fprintf(stderr, "[%s] Created final block table : tot_len=%ld, number=%ld\n", get_timestamp(),
		    ptBlock_get_total_length_by_rf(final_block_table),
		    ptBlock_get_total_number(final_block_table));


    // free unmerged blocks
    stHash_destruct(coverage_block_table);
    stHash_destruct(whole_genome_block_table);
    stList_destruct(annotation_block_table_list);

    fprintf(stderr, "[%s] Started writing to %s.\n", get_timestamp(), out_path);
    if ((strcmp(out_type, "c") == 0) || (strcmp(out_type, "cz") == 0)){
	    fprintf(stderr, "cov and gzipped cov are not supported for now");
	    return -1;
    }else if (strcmp(out_type, "b") == 0){ // uncompressed BED
	fprintf(stderr, "%ld %ld\n", ptBlock_get_total_length_by_rf(final_block_table), ptBlock_get_total_number(final_block_table));
        FILE* fp = fopen(out_path, "w");
        if (fp == NULL) {
            fprintf(stderr, "[%s] Error: Failed to open file %s.\n", get_timestamp(), out_path);
        }
        ptBlock_print_blocks_stHash(final_block_table,
                                    get_string_cov_info_data_format_1,
                                    fp,
                                    false);
        fclose(fp);
    }else if (strcmp(out_type, "bz") == 0){ // gzip-compressed BED
        gzFile fp = gzopen(out_path, "w6h");
        if (fp == NULL) {
            fprintf(stderr, "[%s] Error: Failed to open file %s.\n", get_timestamp(), out_path);
        }
        ptBlock_print_blocks_stHash(final_block_table,
                                    get_string_cov_info_data_format_1,
                                    fp,
                                    true);
        gzclose(fp);
    }
    stHash_destruct(final_block_table);
    fprintf(stderr, "[%s] Done.\n", get_timestamp());
}
