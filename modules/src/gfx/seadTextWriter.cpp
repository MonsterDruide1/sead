#include "gfx/seadTextWriter.h"

#include "devenv/seadFontMgr.h"

namespace sead {

FontBase* TextWriter::sDefaultFont;

void TextWriter::printImpl_(const char16_t*, int, bool, BoundBox2f*, const Projection*, const Camera*) {
    mFontBase->getEncoding();
}

}
