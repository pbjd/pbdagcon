// Expose required dazzler headers to C++ when compiling dazcon
// DB.h -> QV.h, align.h
extern "C" {
    #include "DB.h"
    #include "align.h"
}
