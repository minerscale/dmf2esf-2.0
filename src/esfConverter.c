#include <stdio.h>
#include <string.h>

#include "libdmf.h"
#include "esfConverter.h"

typedef int u8;

// Current state
int noiseMode = 1;

int current_instrument[10] = {-1};

void *parseRow(void *esfPointer, dmf song, int i, int j, int k){
    // Process instruments
    if (current_instrument[k] != song.channels[k].rows[i][j].instrument){
        current_instrument[k] = song.channels[k].rows[i][j].instrument;
    }

    // Process commands
    for (int l = 0; l < song.channels[k].effect_columns_count; ++l){
        int command = song.channels[k].rows[i][j].commands[l].code;
        int value = song.channels[k].rows[i][j].commands[l].value;

        // 20xy - Set noise mode
        if (command == 0x20) {
            noiseMode = value;
        }
    }

    // Note on
    if (song.channels[k].rows[i][j].note && song.channels[k].rows[i][j].octave){
        int octave = song.channels[k].rows[i][j].octave;
        int semitone = song.channels[k].rows[i][j].note;

        // Convert octave + semitone to esf format
        if (semitone == 12){
            octave += 1;
            semitone = 0;
        }

        // Note on command is equal to the command
        if (k < 3){
            *(u8 *)esfPointer = k;
        }else if (k < 6){
            *(u8 *)esfPointer = k + 1;
        }else{
            *(u8 *)esfPointer = k + 2;
        }
        esfPointer += 1;

        // FM channels
        if (k < 6){
            *(u8 *)esfPointer = 32*octave + 2*semitone + 1;
        // PSG melodic channels
        }else if (k < 8){
            *(u8 *)esfPointer = 24*octave + 2*semitone;
        // PSG ch. 3
        }else if (k == 8){
            if ((song.channels[k + 1].rows[i][j].note || song.channels[k + 1].rows[i][j].octave) &&
                        song.channels[k + 1].rows[i][j].note != 100 && ((noiseMode >> 4) & 1))
            {
                octave = song.channels[k + 1].rows[i][j].octave;
                semitone = song.channels[k + 1].rows[i][j].note;

                if (semitone == 12){
                    octave += 1;
                    semitone = 0;
                }
            }
            *(u8 *)esfPointer = 24*octave + 2*semitone;
        // PSG noise channel
        }else{
            // The X argument of 20xy
            // Special noise mode
            if (((noiseMode >> 4) & 1) == 0){
                // Get noise note to play
                *(u8 *)esfPointer = ((semitone > 2) ? 0 : 2 - semitone) + (noiseMode & 1)*4;
            }else{
                *(u8 *)esfPointer = 3 + (noiseMode & 1)*4;
            }
        }
        esfPointer += 1;
    }

    return esfPointer;
}

size_t convertESF(dmf song, void **ret_esf){
    // Write Instruments
    for (int i = 0; i < song.total_instruments; ++i){
        if (song.instruments[i].mode == MODE_FM){
            
        }
    }

    void *esf = malloc(16777216); // esf to write
    void *esfPointer = esf;
    for (int i = 0; i < song.total_rows_in_pattern_matrix; ++i){
        for (int j = 0; j < song.total_rows_per_pattern; ++j){
            for (int k = 0; k < song.system_total_channels; ++k){
                esfPointer = parseRow(esfPointer, song, i, j, k);
            }
        }
    }

    *(u8 *)esfPointer = 0xfd;
    esfPointer += 1;
    *(u8 *)esfPointer = 0xfe;
    esfPointer += 1;
    *(u8 *)esfPointer = 0xff;
    esfPointer += 1;
    *(u8 *)esfPointer = 0xfc;
    esfPointer += 1;

    size_t length = esfPointer - esf;
    *ret_esf = malloc(length);

    memcpy(*ret_esf, esf, length);

    free(esf);

    return (length);
}
