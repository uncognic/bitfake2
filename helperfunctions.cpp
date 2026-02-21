#include <cstdlib>
#include <stdio.h>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>

/*
    helperfunctions.cpp
    This file will contain the implementation of any helper functions that are used across the project.
    The idea is to keep the main function clean and delegate any non-core operations to this file.
    This can include things like metadata extraction, replay gain calculations, MusicBrainz lookups, and more.

    For functions that might request online resources, its best we define them in a different file.
*/

namespace Operations
{
    // Helper Functions

    AudioMetadata GetMetaData(const fs::path& path)
    {
        AudioMetadata tmpdata;
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            warn("Metadata read failed: file does not exist or is not a regular file.");
            return tmpdata;
        }

        TagLib::FileRef fileRef(path.string().c_str());
        if (fileRef.isNull() || fileRef.tag() == nullptr)
        {
            warn("Metadata read failed: unsupported format or unreadable tags.");
            return tmpdata;
        }

        TagLib::Tag* tag = fileRef.tag();
        if (tag->isEmpty())
        {
            warn("Metadata read: tag is empty.");
            return tmpdata;
        }

        tmpdata.title = tag->title().to8Bit(true);
        tmpdata.artist = tag->artist().to8Bit(true);
        tmpdata.album = tag->album().to8Bit(true);
        tmpdata.genre = tag->genre().to8Bit(true);
        if (tag->year() > 0)
        {
            tmpdata.year = std::to_string(tag->year());
        }

        return tmpdata;
    }
}
// constexpr AudioMetadata GetMetaData(const fs::path& path);


// constexpr ReplayGainInfo GetReplayGain(const fs::path& path);
// constexpr MusicBrainzInfo GetMusicBrainzInfo(const fs::path& path);