#include <cstdlib>
#include <stdio.h>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include <algorithm>
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tpropertymap.h>
#include <taglib/xiphcomment.h>
#include <regex>

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
        tmpdata.trackNumber = tag->track();
        if (tag->year() > 0)
        {
            tmpdata.year = std::to_string(tag->year());
        }

        // Try to extract additional metadata from Xiph comments (for Opus, Vorbis, FLAC)
        if (auto* xc = dynamic_cast<TagLib::Ogg::XiphComment*>(fileRef.file()->tag()))
        {
            auto fields = xc->fieldListMap();
            
            // Override with Xiph comment values if available and not empty
            if (fields.find("TITLE") != fields.end() && !fields["TITLE"].isEmpty() && tmpdata.title.empty())
            {
                tmpdata.title = fields["TITLE"][0].to8Bit(true);
            }
            if (fields.find("ARTIST") != fields.end() && !fields["ARTIST"].isEmpty() && tmpdata.artist.empty())
            {
                tmpdata.artist = fields["ARTIST"][0].to8Bit(true);
            }
            if (fields.find("ALBUM") != fields.end() && !fields["ALBUM"].isEmpty() && tmpdata.album.empty())
            {
                tmpdata.album = fields["ALBUM"][0].to8Bit(true);
            }
            if (fields.find("GENRE") != fields.end() && !fields["GENRE"].isEmpty() && tmpdata.genre.empty())
            {
                tmpdata.genre = fields["GENRE"][0].to8Bit(true);
            }
            if (fields.find("DATE") != fields.end() && !fields["DATE"].isEmpty() && tmpdata.year.empty())
            {
                std::string dateStr = fields["DATE"][0].to8Bit(true);
                // Extract year from DATE field (format: YYYY or YYYY-MM-DD)
                if (dateStr.length() >= 4)
                {
                    tmpdata.year = dateStr.substr(0, 4);
                }
            }
            if (fields.find("TRACKNUMBER") != fields.end() && !fields["TRACKNUMBER"].isEmpty() && tmpdata.trackNumber == 0)
            {
                try {
                    std::string trackStr = fields["TRACKNUMBER"][0].to8Bit(true);
                    tmpdata.trackNumber = std::stoi(trackStr);
                } catch (...) {}
            }
        }



        return tmpdata;
    } // GetMetaData

    std::vector<AudioMetadataResult> GetMetaDataList(const fs::path& path)
    {
        std::vector<AudioMetadataResult> results;

        if (!fs::exists(path))
        {
            warn("Metadata list failed: input path does not exist.");
            return results;
        }

        if (fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (fc::IsValidAudioFile(entry.path()))
                {
                    results.push_back({entry.path(), GetMetaData(entry.path())});
                }
            }

            if (results.empty())
            {
                warn("Metadata list: no valid audio files found in directory.");
            }
            else
            {
                std::sort(results.begin(), results.end(),
                    [](const AudioMetadataResult& a, const AudioMetadataResult& b)
                    {
                        return a.metadata.trackNumber < b.metadata.trackNumber;
                    });
            }
            return results;
        }

        if (!fc::IsValidAudioFile(path))
        {
            warn("Metadata list failed: input file is not a valid audio file.");
            return results;
        }

        results.push_back({path, GetMetaData(path)});
        return results;
    } // GetMetaDataList


    ReplayGainInfo GetReplayGain(const fs::path& path)
    {
        ReplayGainInfo tmpdata = {"", "", "", 0, 0.0f, 0.0f, 0.0f, 0.0f};


        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            warn("ReplayGain read failed: file does not exist or is not a regular file.");
            return tmpdata;
        }

        TagLib::FileRef fileRef(path.string().c_str());
        if (fileRef.isNull() || fileRef.tag() == nullptr)
        {
            warn("ReplayGain read failed: unsupported format or unreadable tags.");
            return tmpdata;
        }

        tmpdata.title = fileRef.tag()->title().to8Bit(true);
        tmpdata.artist = fileRef.tag()->artist().to8Bit(true);
        tmpdata.album = fileRef.tag()->album().to8Bit(true);
        tmpdata.trackNumber = fileRef.tag()->track();

        // Get Xiph comment for ReplayGain (works for Opus, Vorbis, FLAC, etc.)
        if (auto* xc = dynamic_cast<TagLib::Ogg::XiphComment*>(fileRef.file()->tag()))
        {
            auto fields = xc->fieldListMap();
            
            // Extract ReplayGain values and remove the "dB" suffix
            if (fields.find("REPLAYGAIN_TRACK_GAIN") != fields.end() && !fields["REPLAYGAIN_TRACK_GAIN"].isEmpty())
            {
                std::string gainStr = fields["REPLAYGAIN_TRACK_GAIN"][0].to8Bit(true);
                // Remove " dB" suffix if present
                gainStr = std::regex_replace(gainStr, std::regex(" dB$"), "");
                try {
                    tmpdata.trackGain = std::stof(gainStr);
                } catch (...) {}
            }
            
            if (fields.find("REPLAYGAIN_TRACK_PEAK") != fields.end() && !fields["REPLAYGAIN_TRACK_PEAK"].isEmpty())
            {
                std::string peakStr = fields["REPLAYGAIN_TRACK_PEAK"][0].to8Bit(true);
                try {
                    tmpdata.trackPeak = std::stof(peakStr);
                } catch (...) {}
            }
            
            if (fields.find("REPLAYGAIN_ALBUM_GAIN") != fields.end() && !fields["REPLAYGAIN_ALBUM_GAIN"].isEmpty())
            {
                std::string gainStr = fields["REPLAYGAIN_ALBUM_GAIN"][0].to8Bit(true);
                gainStr = std::regex_replace(gainStr, std::regex(" dB$"), "");
                try {
                    tmpdata.albumGain = std::stof(gainStr);
                } catch (...) {}
            }
            
            if (fields.find("REPLAYGAIN_ALBUM_PEAK") != fields.end() && !fields["REPLAYGAIN_ALBUM_PEAK"].isEmpty())
            {
                std::string peakStr = fields["REPLAYGAIN_ALBUM_PEAK"][0].to8Bit(true);
                try {
                    tmpdata.albumPeak = std::stof(peakStr);
                } catch (...) {}
            }
        }

        return tmpdata;
    } // GetReplayGain


    std::vector<ReplayGainResult> GetReplayGainList(const fs::path& path)
    {
        std::vector<ReplayGainResult> results;

        if (!fs::exists(path))
        {
            warn("ReplayGain list failed: input path does not exist.");
            return results;
        }

        if (fs::is_directory(path))
        {
            for (const auto& entry : fs::directory_iterator(path))
            {
                if (fc::IsValidAudioFile(entry.path()))
                {
                    results.push_back({entry.path(), GetReplayGain(entry.path())});
                }
            }

            if (results.empty())
            {
                warn("ReplayGain list: no valid audio files found in directory.");
            }
            else
            {
                std::sort(results.begin(), results.end(),
                    [](const ReplayGainResult& a, const ReplayGainResult& b)
                    {
                        return a.info.trackNumber < b.info.trackNumber;
                    });
            }
            return results;
        }

        if (!fc::IsValidAudioFile(path))
        {
            warn("ReplayGain list failed: input file is not a valid audio file.");
            return results;
        }

        results.push_back({path, GetReplayGain(path)});
        return results;
    } // GetReplayGainList
} // namespace Operations

// constexpr AudioMetadata GetMetaData(const fs::path& path);


// constexpr ReplayGainInfo GetReplayGain(const fs::path& path);
// constexpr MusicBrainzInfo GetMusicBrainzInfo(const fs::path& path);