version 1.0

workflow runBamToCoverage{
    call bam2cov
    output{
        File coverageGz = bam2cov.coverageGz
        File biasTableTsv = bam2cov.biasTableTsv
    }
}


task bam2cov{
    input{
        File bam
        File bai
        File fasta
        String? suffix=""
        Int mapqThreshold=20
        Float clipRatioThreshold=0.1
        Float downsampleRate=1.0
        Array[File] annotationBedArray=[]
        Array[String] biasAnnotationNameArray=[]
        String baselineAnnotationName = "WHOLE_GENOME_DEFAULT"
        File? includeContigListText
        Boolean runBiasDetection=false
        String format="all"
        # runtime configurations
        Int memSize=32
        Int threadCount=8
        Int diskSize=ceil(size(bam, "GB"))  + 512
        String dockerImage="mobinasri/flagger:v1.0.0"
        Int preemptible=2
    }
    command <<<
        set -o pipefail
        set -e
        set -u
        set -o xtrace

        # create a bed file covering whole genome after removing "N" base
        FA_PREFIX=$(echo $(basename ~{fasta}) | sed -e 's/\.fa$//' -e 's/\.fa.gz$//' -e 's/\.fasta$//' -e 's/\.fasta.gz$//')

        if [[ "~{fasta}" == *.gz ]]; then
            gunzip -c ~{fasta} > ${FA_PREFIX}.fa
        else
            cp ~{fasta} ${FA_PREFIX}.fa
        fi
        samtools faidx ${FA_PREFIX}.fa

        mkdir -p intermediate
        python3 /home/scripts/get_N_coords.py --inputFasta ${FA_PREFIX}.fa > intermediate/N_coords.bed
        cat ${FA_PREFIX}.fa.fai | \
            awk '{print $1"\t0\t"$2}' | \
            bedtools sort -i - | \
            bedtools subtract -a - -b intermediate/N_coords.bed > WHOLE_GENOME_DEFAULT.bed

        # make a json file pointing to whole_genome.bed and other given annotations
        echo "{" > annotations_path.json
        for bed in ~{sep=" " annotationBedArray}
        do
            echo \"$(basename ${bed%%.bed})\" : \"${bed}\", >> annotations_path.json
        done
        echo \"WHOLE_GENOME_DEFAULT\" : \"${PWD}/WHOLE_GENOME_DEFAULT.bed\" >> annotations_path.json
        echo "}" >> annotations_path.json

        # addittional args for --runBiasDetection 
        ADDITIONAL_ARGS=~{true="--runBiasDetection" false="" runBiasDetection}

        # create a text file containing the list of annotations names for bias detection
        if (( ~{length(biasAnnotationNameArray)} > 0 ));then
            for name in ~{sep=" " biasAnnotationNameArray}
            do
                echo ${name} >> annotations_for_bias.txt
            done
            echo "~{baselineAnnotationName}" >> annotations_for_bias.txt
            ADDITIONAL_ARGS="${ADDITIONAL_ARGS} --restrictBiasAnnotations annotations_for_bias.txt"  
        fi

        # add --includeContigs to additional args variable if a list of contigs is given
        if [ -n "~{includeContigListText}" ]; then
            ADDITIONAL_ARGS="${ADDITIONAL_ARGS} --includeContigs ~{includeContigListText}"
        fi

        BAM_FILENAME=`basename ~{bam}`
        BAM_PREFIX="${BAM_FILENAME%.*}"
        ln -s ~{bam} ${BAM_PREFIX}.bam
        ln -s ~{bai} ${BAM_PREFIX}.bam.bai

        mkdir -p output

        downsampleRateRounded=$(echo ~{downsampleRate} | awk '{printf "%.1f",$1}')

        # if given add suffix to filename
        if [ -n "~{suffix}" ]; then
            OUTPUT="output/${BAM_PREFIX}.downsample_${downsampleRateRounded}.~{suffix}.cov.gz"
            touch output/${BAM_PREFIX}.downsample_${downsampleRateRounded}.~{suffix}.bias_detection_table.tsv
        else
            OUTPUT="output/${BAM_PREFIX}.downsample_${downsampleRateRounded}.cov.gz"
            touch output/${BAM_PREFIX}.downsample_${downsampleRateRounded}.bias_detection_table.tsv    
        fi

        # convert bam to cov
        bam2cov --bam ${BAM_PREFIX}.bam \
                --mapqThreshold ~{mapqThreshold} \
                --clipRatioThreshold ~{clipRatioThreshold} \
                --output ${OUTPUT} \
                --annotationJson annotations_path.json \
                --threads ~{threadCount} \
                --format ~{format} \
                --baselineAnnotation ~{baselineAnnotationName} \
                --downsampleRate ~{downsampleRate} ${ADDITIONAL_ARGS}

    >>>
    runtime {
        docker: dockerImage
        memory: memSize + " GB"
        cpu: threadCount
        disks: "local-disk " + diskSize + " SSD"
        preemptible : preemptible
    }
    output{
        File coverageGz = glob("output/*cov.gz")[0]
        File biasTableTsv = glob("output/*bias_detection_table.tsv")[0]
    }
}




task bamCoverageFast{
    input{
        File bam
        File bai
        Int minMAPQ
        String output_format = "only_total" # it can be "only_high_mapq"
        File assemblyFastaGz
        # runtime configurations
        Int memSize=32
        Int threadCount=16
        Int diskSize=ceil(size(bam, "GB"))  + 512
        String dockerImage="mobinasri/flagger:v0.4.0"
        Int preemptible=2
    }
    command <<<
        # Set the exit code of a pipeline to that of the rightmost command
        # to exit with a non-zero status, or zero if all commands of the pipeline exit
        set -o pipefail
        # cause a bash script to exit immediately when a command fails
        set -e
        # cause the bash shell to treat unset variables as an error and exit immediately
        set -u
        # echo each line of the script to stdout so we can see what is happening
        # to turn off echo do 'set +o xtrace'
        set -o xtrace

        # Extract assembly and index
        FA_FILENAME=`basename ~{assemblyFastaGz}`
        FA_PREFIX="${FA_FILENAME%.*.*}"
        gunzip -c ~{assemblyFastaGz} > ${FA_PREFIX}.fa
        samtools faidx ${FA_PREFIX}.fa

        cat ${FA_PREFIX}.fa.fai | awk '{print $1"\t0\t"$2}' | bedtools sort -i - > asm_wg.bed

        # make a json file pointing to the asm_wg.bed
        echo "{" > bed_file.json
        echo \"asm_wg\" : \"asm_wg.bed\" >> bed_file.json
        echo "}" >> bed_file.json

        BAM_FILENAME=`basename ~{bam}`
        BAM_PREFIX="${BAM_FILENAME%.*}"
        ln -s ~{bam} ${BAM_PREFIX}.bam
        ln -s ~{bai} ${BAM_PREFIX}.bam.bai

        # convert bam to cov
        # -f only_total is for printing the total coverage only
        bam2cov -i ${BAM_PREFIX}.bam -O "c" -o ${BAM_PREFIX}.cov -j bed_file.json -t~{threadCount} -f ~{output_format}

        # Convert cov to counts
        cov2counts -i ${BAM_PREFIX}.cov -o ${BAM_PREFIX}.counts
        # Calculate mean and standard deviation
        python3 ${CALC_MEAN_SD_PY} --countsInput ${BAM_PREFIX}.counts --meanOutput cov_mean.txt --sdOutput cov_sd.txt
        gzip ${BAM_PREFIX}.cov
    >>>
    runtime {
        docker: dockerImage
        memory: memSize + " GB"
        cpu: threadCount
        disks: "local-disk " + diskSize + " SSD"
        preemptible : preemptible
    }
    output{
        File coverageGz = glob("*.cov.gz")[0]
        File counts = glob("*.counts")[0]
        Float coverageMean = read_float("cov_mean.txt")
        Float coverageSD = read_float("cov_sd.txt")
    }
}



task bamCoverage{
    input{
        File bam
        Int minMAPQ
        File assemblyFastaGz
        # runtime configurations
        Int memSize=16
        Int threadCount=4
        Int diskSize=ceil(size(bam, "GB"))  + 512
        String dockerImage="mobinasri/flagger:v0.3.2"
        Int preemptible=2
    }
    command <<<
        # Set the exit code of a pipeline to that of the rightmost command
        # to exit with a non-zero status, or zero if all commands of the pipeline exit
        set -o pipefail
        # cause a bash script to exit immediately when a command fails
        set -e
        # cause the bash shell to treat unset variables as an error and exit immediately
        set -u
        # echo each line of the script to stdout so we can see what is happening
        # to turn off echo do 'set +o xtrace'
        set -o xtrace
        
        # Extract assembly and index
        FA_FILENAME=`basename ~{assemblyFastaGz}`
        FA_PREFIX="${FA_FILENAME%.*.*}"
        gunzip -c ~{assemblyFastaGz} > ${FA_PREFIX}.fa
        samtools faidx ${FA_PREFIX}.fa

        BAM_FILENAME=`basename ~{bam}`
        BAM_PREFIX="${BAM_FILENAME%.*}"
        samtools depth -aa -Q ~{minMAPQ} ~{bam}  > ${BAM_PREFIX}.depth
        
        # Convert the output of samtools depth into a compressed format
        depth2cov -d ${BAM_PREFIX}.depth -f ${FA_PREFIX}.fa.fai -o ${BAM_PREFIX}.cov
        # Convert cov to counts
        cov2counts -i ${BAM_PREFIX}.cov -o ${BAM_PREFIX}.counts
        # Calculate mean and standard deviation
        python3 ${CALC_MEAN_SD_PY} --countsInput ${BAM_PREFIX}.counts --meanOutput cov_mean.txt --sdOutput cov_sd.txt
        gzip ${BAM_PREFIX}.cov
    >>>
    runtime {
        docker: dockerImage
        memory: memSize + " GB"
        cpu: threadCount
        disks: "local-disk " + diskSize + " SSD"
        preemptible : preemptible
    }
    output{
        File coverageGz = glob("*.cov.gz")[0]
        File counts = glob("*.counts")[0]
        Float coverageMean = read_float("cov_mean.txt") 
        Float coverageSD = read_float("cov_sd.txt")
    }
}

