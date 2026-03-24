#include "os/BufFile.h"
#include "utl/MakeString.h"

bool BufFile::Eof() {
    return (mPos - mBuf) >= mSize;
}

String BufFile::Filename() const {
    FormatString fs("--memory (BufFile)--");
    return String(fs.Str());
}
