#ifndef _ESF_CONVERTER_H_
#define _ESF_CONVERTER_H_

int debug;

size_t convertESF(dmf song, unsigned char **ret_esf, unsigned char ***instruments);

#endif // _ESF_CONVERTER_H_
