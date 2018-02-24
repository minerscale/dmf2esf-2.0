#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "libdmf.h"
#include "esfConverter.h"

int main(int argc, char **argv){
    int c;

    char *outputFile = NULL;
    char *inputFile = NULL;
    while ((c = getopt (argc, argv, "hvo:")) != -1){
        switch (c){
            case 'h':
                printf ("Usage: dmf2esf [options] file\nOptions:\n    -o <file>               Specify output file. Default none.\n");
                return 0;
            case 'v':
                printf ("dmf2esf 2.0.0\n");
                return 0;
            case 'o':
                outputFile = malloc(strlen(optarg) + 1);
                memcpy(outputFile, optarg, (size_t)strlen(optarg) + 1);
                break;
            case '?':
                return 1;
        }
    }

    if (optind < argc){
        inputFile = argv[optind];
    }else{
        fprintf (stderr, "dmf2esf: fatal error: no input file\n");
        return 1;
    }

    if (inputFile && access(inputFile, R_OK) != 0){
        fprintf (stderr, "dmf2esf: error: %s: No such file or directory\n", inputFile);
        return 1;
    }

    if (outputFile && access(dirname(outputFile), W_OK) != 0){
        fprintf (stderr, "dmf2esf: error: %s: Permission denied\n", outputFile);
        return 1;
    }

    // Now that the options are parsed it's time to parse the dmf and write the esf
    dmf song;
    
    fileToDmfType(inputFile, &song);
    if (song.system != SYSTEM_GENESIS){
        if (song.system == SYSTEM_GENESIS_EXT_CH3){
            printf ("dmf2esf: error: %s: ext. ch3 not supported yet, sorry!", inputFile);
            exit(1);
        }
        printf ("dmf2esf: error: %s: System needs to be Genesis/Megadrive\n", inputFile);
        exit(1);
    }

    void *esf = NULL;
    size_t esf_length = convertESF(song, &esf);
    
    if(outputFile){
        FILE *fp = fopen(outputFile, "wb");

        fwrite(esf, 1, esf_length, fp);
    }
}
