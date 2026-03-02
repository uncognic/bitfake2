#include <cstdio>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
namespace gb = globals;


namespace Operations
{
    // Core operations

    /*
        This file will define functions for core operations of the program, such as conversion, replaygain calculation,
        mass tagging for a directory, and other nitty gritty functions that will be a hassle to program. Will try to keep things
        as clean and organized as possible, but no promises >:D
    */

    // void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format);
    // void MassTagDirectory(const fs::path& dirPath, const std::string& tag, const std::string& value);
    // void ApplyReplayGain(const fs::path& path, float trackGain, float albumGain);
    // void CalculateReplayGain(const fs::path& path); 

    // We can now begin implementing these fuckass features:

    void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format, VBRQualities quality)
    {
        if (!fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::FLAC) && !fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::WAV))
        {
            warn("Input file is not a lossless file. Conversion may result in quality loss. :(");
        }


        /* Redundant check here, alr implemnted after strcmp passes for a -cvrt / --convert flag */

        // // Output path here will be the global var -po if specified.
        // if (!fs::exists(outputPath) || !fs::is_directory(outputPath))
        // {
        //     err("Output path does not exist or is not a regular file!");
        //     return; // exit function early due to invalid output path
        // }


        // Get qual val from enum class and that plugs into ffmpegCmd string
        int qualval = static_cast<int>(quality);
        std::string codec = op::ConversionLibMap.at(format);

        if (format == AudioFormat::MP3 && !HasFfmpegEncoder("libmp3lame")) {
            err("FFmpeg on this system does not have MP3 encoder libmp3lame enabled.");
            warn("Install FFmpeg with libmp3lame support, or choose another output format (e.g. ogg/flac/opus). ");
            return;
        }

        fs::path outputFile = outputPath;
        if (fs::is_directory(outputPath)) {
            outputFile = outputPath / (inputPath.stem().string() + OutputExtensionForFormat(format));
        }

        bool hasInputCover = InputHasAttachedCover(inputPath);
        bool carryCover = hasInputCover && FormatSupportsAttachedCover(format);

        if (hasInputCover && !carryCover) {
            warn("Input has cover art, but this output format may not support embedded album cover. Cover art will be skipped.");
        }

        std::string ffmpegCmd = "ffmpeg -y -i \"" + inputPath.string() + "\" -map 0:a:0 ";

        if (carryCover) {
            ffmpegCmd += "-map 0:v:0 -c:v copy -disposition:v:0 attached_pic ";
        } else {
            ffmpegCmd += "-vn ";
        }

        ffmpegCmd += "-map_metadata 0 -metadata LENGTH= -metadata TLEN= -c:a " + codec;

        if (format == AudioFormat::FLAC) {
            ffmpegCmd += " -compression_level " + std::to_string(qualval);
        } else if (format == AudioFormat::WAV) {
            // no quality switch for pcm_s16le
        } else {
            ffmpegCmd += " -q:a " + std::to_string(qualval);
        }

        ffmpegCmd += " \"" + outputFile.string() + "\"";

        // Build ffmpeg command
        plog("Built ffmpeg command: ");
        yay(ffmpegCmd.c_str());

        int ret = std::system(ffmpegCmd.c_str());
        if (ret != 0) {
            err("ffmpeg conversion failed.");
            return;
        }

        yay("Conversion completed successfully!");
        plog("Output file:");
        yay(outputFile.c_str());
    }
}