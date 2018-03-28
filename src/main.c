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
    while ((c = getopt (argc, argv, "hvdo:")) != -1){
        switch (c){
            case 'h':
                printf ("Usage: dmf2esf [options] file\nOptions:\n    -o <file>               Specify output file. Default none.\n");
                return 0;
            case 'v':
                printf ("dmf2esf 2.0.0\n");
                return 0;
            case 'd':
                debug = 1;
                break;
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

    if (outputFile && access(dirname(strdup(outputFile)), W_OK) != 0){
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

    // TO DEBUG
    unsigned char *esf = NULL;
    unsigned char **instruments = NULL;
    size_t esf_length = convertESF(song, &esf, &instruments);
    
    if(outputFile){
        FILE *fp = fopen(outputFile, "wb");
        fwrite(esf, 1, esf_length, fp);
        fclose(fp);

        char *fileStr = calloc(100, 1);
        for (int i = strlen(outputFile); i > 0; --i){
            if (outputFile[i] == 46) memcpy (fileStr, outputFile, i);
        }

        char *filename = calloc(100, 1);
        for (int i = 0; i < song.total_instruments; ++i){

            if (song.instruments[i].mode == MODE_FM){
                sprintf(filename, "%s_inst-%02x.eif", fileStr, i);
                FILE *instFile = fopen(filename, "wb");
                fwrite(instruments[i], 1, 29, instFile);
            }else{
                sprintf(filename, "%s_inst-%02x.eef", fileStr, i);
                FILE *instFile = fopen(filename, "wb");
                int j = 0;
                for (j = 0; instruments[i][j] != 0xFF; ++j){
                    fwrite(&instruments[i][j], 1, 1, instFile);
                }
                fwrite(&instruments[i][j], 1, 1, instFile);
            }
        }
    }

    // Free before exit
    freeDMF(&song);

    return 0;
}
