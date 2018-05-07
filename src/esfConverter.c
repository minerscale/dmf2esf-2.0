#include <stdio.h>
#include <string.h>

#include "libdmf.h"
#include "esfConverter.h"
typedef int u8;

// Current state
int noiseMode = 1;

int current_instrument[10] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

// Time since last note in ticks 50/60hz
int time_since_last_event = 0;

int delay_amount = 0;

int delay_flag = 0;

// Is the current tick even?
int tick_even = 1;

int tick_time_1 = 0;
int tick_time_2 = 0;

u8 *addDelay(u8 *esfPointer, int amount){
    while (1){
        if (amount == 0) break;
        else if (amount < 0x10){
            *esfPointer = 0xD0 | (amount - 1);
            esfPointer += 1;
            break;
        }else if (amount < 0x100){
            *esfPointer = 0xFE;
            esfPointer += 1;
            *esfPointer = amount;
            esfPointer += 1;
            break;
        }else{
            *esfPointer = 0xFE;
            esfPointer += 1;
            *esfPointer = 0xFF;
            esfPointer += 1;
            amount -= 0xFF;
        }
    }
    return esfPointer;
}

u8 *parseRow(u8 *esfPointer, dmf song, int i, int j, int k){
    // Process instruments
    if (song.channels[k].rows[i][j].instrument != -1 && current_instrument[k] != song.channels[k].rows[i][j].instrument){
        current_instrument[k] = song.channels[k].rows[i][j].instrument;

        if (k < 3){
            *esfPointer = 0x40 | k;
        }else if (k < 6){
            *esfPointer = 0x40 | (k + 1);
        }else{
            *esfPointer = 0x40 | (k + 2);
        }
        esfPointer += 1;

        *esfPointer = current_instrument[k];
        esfPointer += 1;
    }

    // Process commands
    for (int l = 0; l < song.channels[k].effect_columns_count; ++l){
        int command = song.channels[k].rows[i][j].commands[l].code;
        int value = song.channels[k].rows[i][j].commands[l].value;

        switch(command){
            // Set noise mode
            case 0x20:
                noiseMode = value;
                break;
            // Set tempo 1
            case 0x09:
                if (value != 0 && value <= 20){

                    tick_time_1 = value;
                }
                break;
            // Set tempo 2
            case 0x0F:
                if (value != 0 && value <= 20){
                    tick_time_2 = value;
                }
                break;
            
        }
    }

    // Note off
    if (song.channels[k].rows[i][j].note == 100){
        esfPointer = addDelay(esfPointer, time_since_last_event);
        time_since_last_event = 0;

        if (k < 3){
            *esfPointer = k + 0x10;
        }else if (k < 6){
            *esfPointer = k + 0x11;
        }else if (k == 8){
            // If there's a note on channel 4 that isn't a note off.
            if ((song.channels[k + 1].rows[i][j].note || song.channels[k + 1].rows[i][j].octave) &&
                        song.channels[k + 1].rows[i][j].note != 100 && ((noiseMode >> 4) & 1))
            {
                // Then we *actually* have to note off channel 4.
                *esfPointer = 0x1B;
            }
            *esfPointer = 0x1A;
        }else{
            *esfPointer = k + 0x12;
        }
        esfPointer += 1;
    }
    // Note on
    else if (song.channels[k].rows[i][j].note && song.channels[k].rows[i][j].octave){
        // Add delay
        esfPointer = addDelay(esfPointer, time_since_last_event);
        time_since_last_event = 0;

        int octave = song.channels[k].rows[i][j].octave;
        int semitone = song.channels[k].rows[i][j].note;

        // Convert octave + semitone to esf format
        if (semitone == 12){
            octave += 1;
            semitone = 0;
        }

        if (k < 3){
            *esfPointer = k;
        }else if (k < 6){
            *esfPointer = k + 1;
        }else{
            *esfPointer = k + 2;
        }
        esfPointer += 1;

        // FM channels
        if (k < 6){
            *esfPointer = 32*octave + 2*semitone + 1;
        // PSG melodic channels
        }else if (k < 8){
            *esfPointer = 24*(octave - 1) + 2*semitone;
        // PSG ch. 3
        }else if (k == 8){
            if ((song.channels[k + 1].rows[i][j].note || song.channels[k + 1].rows[i][j].octave) &&
                        song.channels[k + 1].rows[i][j].note != 100 && ((noiseMode >> 4) & 1))
            {
                // Redo octaves from the noise channel.
                octave = song.channels[k + 1].rows[i][j].octave;
                semitone = song.channels[k + 1].rows[i][j].note;

                if (semitone == 12){
                    octave += 1;
                    semitone = 0;
                }
            }
            *esfPointer = 24*(octave - 1) + 2*semitone;
        // PSG noise channel
        }else{
            // The X argument of 20xy
            // Special noise mode
            if (((noiseMode >> 4) & 1) == 0){
                // Get noise note to play
                *esfPointer = ((semitone > 2) ? 0 : 2 - semitone) + (noiseMode & 1)*4;
            }else{
                *esfPointer = 3 + (noiseMode & 1)*4;
            }
        }
        esfPointer += 1;
    }

    return esfPointer;
}

size_t convertESF(dmf song, unsigned char **ret_esf, unsigned char ***instruments){

    /*****************************************************************************\
    | Write Instruments                                                           |
    \*****************************************************************************/
    
    *instruments = malloc(song.total_instruments * sizeof(unsigned char *));

    for (int i = 0; i < song.total_instruments; ++i){
        if (song.instruments[i].mode == MODE_FM){
            (*instruments)[i] = malloc(29 * sizeof(u8));

            // Write bytes to array
            // Feedback and Algorithm
            (*instruments)[i][0] = (song.instruments[i].FB << 3) | song.instruments[i].ALG;

            for (int j = 0; j < 4; ++j){
                FM_operator cur_op = song.instruments[i].FM_operators[j];
                
                // Detune is a special snowflake
                // Fixing negatives
                static const u8 dt_lookup[] = {7, 6, 5, 0, 1, 2, 3};
                u8 dt_actual = dt_lookup[cur_op.DT];

                (*instruments)[i][1  + j] = (dt_actual << 4) | cur_op.MULT;
                (*instruments)[i][5  + j] = cur_op.TL;
                (*instruments)[i][9  + j] = (cur_op.RS << 6) | cur_op.AR;
                (*instruments)[i][13 + j] = (cur_op.AM << 7) | cur_op.DR;
                (*instruments)[i][17 + j] = cur_op.D2R;
                (*instruments)[i][21 + j] = (cur_op.SL << 4) | cur_op.RR;
                (*instruments)[i][25 + j] = cur_op.SSGMODE;
            }
        // STD instrument
        }else{
            (*instruments)[i] = malloc(song.instruments[i].volume_envelope_size + 2);
            // Volume + Arpeggio marcro
            if (song.instruments[i].volume_envelope_size){
                int offset = 0;
                int j;
                for (j = 0; j < (song.instruments[i].volume_envelope_size - 1); ++j){
                    if (song.instruments[i].volume_loop_position == j){
                        (*instruments)[i][offset] = 0xFE;
                        offset += 1;
                    }
                    (*instruments)[i][offset] =  15 - song.instruments[i].volume_envelope[j];
                    offset += 1;
                }

                if (song.instruments[i].volume_loop_position == -1 ||
                    song.instruments[i].volume_loop_position == song.instruments[i].volume_envelope_size ||
                    song.instruments[i].volume_loop_position == song.instruments[i].volume_envelope_size - 1)
                {
                    (*instruments)[i][offset] = 0xFE;
                    offset += 1;
                }
                (*instruments)[i][offset] = 15 - song.instruments[i].volume_envelope[j];
                offset += 1;
                (*instruments)[i][offset] = 0xFF;

            }else{
                (*instruments)[i][0] = 0xFE;
                (*instruments)[i][1] = 0x00;
                (*instruments)[i][2] = 0xFF;
            }
        }
    }

    /*****************************************************************************\
    | Write ESF                                                                   |
    \*****************************************************************************/

    tick_time_1 = song.tick_time_1;
    tick_time_2 = song.tick_time_2;

    u8 *esf = malloc(16777216); // esf to write
    u8 *esfPointer = esf;
    for (int i = 0; i < song.total_rows_in_pattern_matrix; ++i){
        for (int j = 0; j < song.total_rows_per_pattern; ++j){

            for (int l = 0; l < (tick_even ? tick_time_1 : tick_time_2) * (song.time_base + 1); ++l){
                for (int k = 0; k < song.system_total_channels; ++k){
                    // Is there a delay command?
                    int is_delay = 0;
                    int delay_value = 0;
                    for(int m = 0; m < song.channels[k].effect_columns_count; ++m){
                        is_delay = song.channels[k].rows[i][j].commands[m].code == 0xED;
                        delay_value = song.channels[k].rows[i][j].commands[m].value;
                    }
                    
                    if (is_delay){
                        // If there is a delay do it in order of earliest to latest.
                        if (delay_value == l){
                            esfPointer = parseRow(esfPointer, song, i, j, k);
                        }
                    // If not, play command on the beat
                    }else if (l == 0){
                        esfPointer = parseRow(esfPointer, song, i, j, k);
                    }
                }
                time_since_last_event += 1;
            }
            
            tick_even = !tick_even;
        }
    }
    esfPointer = addDelay(esfPointer, time_since_last_event);
    time_since_last_event = 0;
    
    *esfPointer = 0xff;
    esfPointer += 1;

    size_t length = esfPointer - esf;
    *ret_esf = malloc(length);

    for (int i = 0; i < length; ++i){
        (*ret_esf)[i] = esf[i];
    }

    free(esf);

    return (length);
}
