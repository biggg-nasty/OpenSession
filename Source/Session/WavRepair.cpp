// OpenSession - recording-first multitrack DAW.
// Copyright (C) 2026 The OpenSession contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "WavRepair.h"

#if JUCE_MAC || JUCE_LINUX
 #include <unistd.h>
#endif

namespace opensession::WavRepair
{

static uint32_t readLE32 (juce::InputStream& in)   { return (uint32_t) in.readInt(); }

Report inspect (const juce::File& f)
{
    Report r;
    r.fileSize = f.getSize();
    juce::FileInputStream in (f);

    if (! in.openedOk() || r.fileSize < 12)
    {
        r.error = "Not readable or too short";
        return r;
    }

    char tag[4];
    in.read (tag, 4);

    if (std::memcmp (tag, "RIFF", 4) != 0 && std::memcmp (tag, "RF64", 4) != 0)
    {
        r.error = "Not a RIFF file";
        return r;
    }

    const uint32_t riffSize = readLE32 (in);
    in.read (tag, 4);

    if (std::memcmp (tag, "WAVE", 4) != 0)
    {
        r.error = "Not a WAVE file";
        return r;
    }

    r.isWav = true;
    bool foundFmt = false, foundData = false;

    while (in.getPosition() + 8 <= r.fileSize)
    {
        in.read (tag, 4);
        const uint32_t chunkSize = readLE32 (in);
        const int64_t chunkStart = in.getPosition();

        if (std::memcmp (tag, "fmt ", 4) == 0)
        {
            foundFmt = true;
            in.readShort();                               // format tag
            r.numChannels = in.readShort();
            r.sampleRate = (double) readLE32 (in);
            readLE32 (in);                                // byte rate
            r.bytesPerFrame = in.readShort();
            r.bitsPerSample = in.readShort();
        }
        else if (std::memcmp (tag, "data", 4) == 0)
        {
            foundData = true;
            r.dataChunkOffset = chunkStart;
            r.headerDataSize = chunkSize;
            r.actualDataSize = r.fileSize - chunkStart;
            break;                                        // data is the last chunk the writer emits
        }

        if (chunkSize == 0 && std::memcmp (tag, "data", 4) != 0)
        {
            // A zero-sized non-data chunk before data would be odd; keep scanning safely
        }

        in.setPosition (chunkStart + chunkSize + (chunkSize & 1));
    }

    if (! foundFmt || ! foundData)
    {
        r.error = "Missing fmt or data chunk";
        return r;
    }

    if (r.bytesPerFrame > 0)
        r.recoverableFrames = r.actualDataSize / r.bytesPerFrame;

    const int64_t expectedRiff = r.fileSize - 8;
    r.needsRepair = (r.headerDataSize != r.actualDataSize) || ((int64_t) riffSize != expectedRiff && r.headerDataSize == 0);
    return r;
}

juce::Result repair (const juce::File& f)
{
    auto r = inspect (f);

    if (! r.isWav)
        return juce::Result::fail (r.error.isNotEmpty() ? r.error : "Not a WAV file");

    if (! r.needsRepair)
        return juce::Result::ok();

    if (r.bytesPerFrame <= 0 || r.actualDataSize <= 0)
        return juce::Result::fail ("No audio data to recover in " + f.getFileName());

    // Only whole frames
    const int64_t dataSize = (r.actualDataSize / r.bytesPerFrame) * r.bytesPerFrame;

    if (dataSize > 0xffffffffLL)
        return juce::Result::fail ("File too large for a standard RIFF header");

    juce::FileOutputStream out (f);

    if (! out.openedOk())
        return juce::Result::fail ("Can't open " + f.getFileName() + " for writing");

    out.setPosition (4);
    out.writeInt ((int) (r.dataChunkOffset + dataSize - 8));     // RIFF size
    out.setPosition (r.dataChunkOffset - 4);
    out.writeInt ((int) dataSize);                                 // data chunk size
    out.flush();

    // A trailing partial frame, if any, is ignored by readers.
    return juce::Result::ok();
}

bool createHardLink (const juce::File& file, const juce::File& linkPath)
{
   #if JUCE_MAC || JUCE_LINUX
    linkPath.getParentDirectory().createDirectory();
    linkPath.deleteFile();
    return ::link (file.getFullPathName().toRawUTF8(), linkPath.getFullPathName().toRawUTF8()) == 0;
   #else
    juce::ignoreUnused (file, linkPath);
    return false;
   #endif
}

} // namespace opensession::WavRepair
