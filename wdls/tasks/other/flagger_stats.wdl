version 1.0 

workflow runFlaggerStats{
    call flaggerStats
    output{
        File flaggerStatsTsv = flaggerStats.flaggerStatsTsv
        File flaggerStatsPercOnlyTsv = flaggerStats.flaggerStatsPercOnlyTsv
    }
}
task flaggerStats {
    input {
        File fai
        File flaggerBed
        File difficultBed_1
        String difficultString_1
        File difficultBed_2
        String difficultString_2
        File sexBed
        String sample
        String prefix
        Int minContigSize = 1000000
        # runtime configurations
        Int memSize=4
        Int threadCount=4
        Int diskSize=128
        String dockerImage="mobinasri/bio_base:v0.1"
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

        cat ~{fai} | awk '{print $1"\t0\t"$2}' | bedtools sort -i - > asm.bed
        cat ~{fai} | awk '$2>~{minContigSize}{print $1"\t0\t"$2}' > asm_long.bed
        bedtools subtract -a asm.bed -b ~{sexBed} > autosome.bed
        bedtools subtract -a asm.bed -b ~{difficultBed_1} > easy_1.bed
        bedtools subtract -a asm.bed -b ~{difficultBed_2} > easy_2.bed
        bedtools intersect -a easy_1.bed -b easy_2.bed > easy_all.bed

        bedtools intersect -a autosome.bed -b easy_1.bed > autosome_easy_1.bed
        bedtools intersect -a autosome.bed -b easy_2.bed > autosome_easy_2.bed
        bedtools intersect -a autosome.bed -b easy_all.bed > autosome_easy_all.bed

        bedtools intersect -a autosome_easy_all.bed -b asm_long.bed > autosome_easy_all_long.bed
        
        

        columns="sample\tinfo"
        values="~{sample}\t~{prefix}"

        columns_2="sample\tinfo"
        values_2="~{sample}\t~{prefix}"
        for x in asm.bed,All asm_long.bed,Long ~{sexBed},sex autosome.bed,Autosome ~{difficultBed_1},~{difficultString_1} ~{difficultBed_2},~{difficultString_2} easy_1.bed,Non_~{difficultString_1} easy_2.bed,Non_~{difficultString_2} autosome_easy_1.bed,Autosome_Non_~{difficultString_1} autosome_easy_2.bed,Autosome_Non_~{difficultString_2} autosome_easy_all.bed,Autosome_Non_~{difficultString_1}_Non_~{difficultString_2} autosome_easy_all_long.bed,Autosome_Non_~{difficultString_1}_Non_~{difficultString_2}_Long
        do
            IFS=, read bed name <<< "$x"
            err=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | grep "Err" | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}') || true
            dup=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | grep "Dup" | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}') || true
            hap=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | grep "Hap" | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}') || true
            col=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | grep "Col" | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}') || true
            unk=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | grep "Unk" | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}') || true
            tot=$(bedtools intersect -a ~{flaggerBed} -b ${bed} | awk '{s+=$3-$2}END{printf("%.2f", s/1e6)}')
            values_curr=$(echo ${err} ${dup} ${hap} ${col} ${unk} ${tot} | awk '{printf $1"\\t"$2"\\t"$3"\\t"$4"\\t"$5"\\t"($1+$2+$4+$5)"\\t"($1+$2+$4+$5)/$6 * 100"\\t"$6}')
            values_curr_2=$(echo ${err} ${dup} ${hap} ${col} ${unk} ${tot} | awk '{printf ($1+$2+$4+$5)/$6 * 100}')
            columns_curr="Err_${name}\tDup_${name}\tHap_${name}\tCol_${name}\tUnk_${name}\tUnreliable_${name}\tUnreliable_${name}_Percent\tTotal_${name}"
            columns_curr_2="${name}"
            values="${values}\t${values_curr}"
            columns="${columns}\t${columns_curr}"
            values_2="${values_2}\t${values_curr_2}"
            columns_2="${columns_2}\t${columns_curr_2}"
        done

        printf ${columns}"\n" > ~{sample}.~{prefix}.flagger_stats.tsv
        printf ${values}"\n" >> ~{sample}.~{prefix}.flagger_stats.tsv

        printf ${columns_2}"\n" > ~{sample}.~{prefix}.flagger_stats.perc_only.tsv
        printf ${values_2}"\n" >> ~{sample}.~{prefix}.flagger_stats.perc_only.tsv
 
    >>> 
    runtime {
        docker: dockerImage
        memory: memSize + " GB"
        cpu: threadCount
        disks: "local-disk " + diskSize + " SSD"
        preemptible : preemptible
    }
    output {
        File flaggerStatsTsv = glob("*flagger_stats.tsv")[0]
        File flaggerStatsPercOnlyTsv = glob("*perc_only.tsv")[0]
    }
}

