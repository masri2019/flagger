version 1.0

import "../../ext/hpp_production_workflows/QC/wdl/tasks/extract_reads.wdl" as extractReads_t
import "../../ext/hpp_production_workflows/QC/wdl/tasks/arithmetic.wdl" as arithmetic_t
import "../tasks/alignment/merge_bams.wdl" as mergeBams_t
import "../tasks/alignment/read_set_splitter.wdl" as readSetSplitter_t
import "../tasks/alignment/long_read_aligner.wdl" as longReadAligner_t
import "../tasks/alignment/calmd.wdl" as calmd_t

workflow longReadAlignmentScattered {
    input {
        String aligner="winnowmap"
        String preset
        String sampleName
        String sampleSuffix
        Array[File] readFiles
        Int splitNumber = 16
        File assembly
        File? referenceFasta
        String alignerOptions=""
        String fastqOptions=""
        Int kmerSize = 15
        Int preemptible=2
        Int extractReadsDiskSize=512
        String zones="us-west2-a"
    }

    scatter (readFile in readFiles) {
        call extractReads_t.extractReads as extractReads {
            input:
                readFile=readFile,
                referenceFasta=referenceFasta,
                fastqOptions = fastqOptions,
                memSizeGB=4,
                threadCount=4,
                diskSizeGB=extractReadsDiskSize,
                dockerImage="mobinasri/bio_base:dev-v0.1"
        }
    }
    call arithmetic_t.sum as readSize {
        input:
            integers=extractReads.fileSizeGB
    }

    call readSetSplitter_t.readSetSplitter {
        input:
            readFastqs = extractReads.extractedRead,
            splitNumber = splitNumber,
            diskSize = floor(readSize.value * 2.5)
    }
   
    scatter (readFastqAndSize in zip(readSetSplitter.splitReadFastqs, readSetSplitter.readSizes)) {
         ## align reads to the assembly
         call longReadAligner_t.alignmentBam as alignment{
             input:
                 aligner =  aligner,
                 preset = preset,
                 refAssembly=assembly,
                 readFastq_or_queryAssembly = readFastqAndSize.left,
                 diskSize = 8 + floor(readFastqAndSize.right) * 6,
                 preemptible = preemptible,
                 zones = zones,
                 options = alignerOptions,
                 kmerSize = kmerSize
        }
    }
    call arithmetic_t.sum as bamSize {
        input:
            integers=alignment.fileSizeGB
    }

    ## merge the bam files
    call mergeBams_t.merge as mergeBams{
        input:
            sampleName = sampleName,
            sampleSuffix = sampleSuffix,
            sortedBamFiles = alignment.sortedBamFile,
            # runtime configurations
            diskSize = floor(bamSize.value * 2.5) + 32,
            preemptible = preemptible,
            zones = zones
    }
    
    ## add Md tag
    call calmd_t.calmd {
        input:
            bamFile = mergeBams.mergedBam,
            assemblyFastaGz = assembly,
            diskSize = floor(bamSize.value * 2.5) + 32,
            preemptible = preemptible,
            zones = zones
    }
    output {
        File bamFile = calmd.outputBamFile
        File baiFile = calmd.outputBaiFile
    }
}

