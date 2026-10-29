#ifndef MMRESULTTRANSLATOR_H
#define MMRESULTTRANSLATOR_H

#include "sound.h"

class CMMError {
public:
    SNDERR handle_error(MMRESULT mmerror);
    CMMError();
    virtual ~CMMError();
};

#endif
