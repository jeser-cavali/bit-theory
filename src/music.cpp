//
// Created by jeser on 03/03/2026.
//

#include "music.h"

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <filesystem>
#include <unordered_map>

#include "id3.h"
#include "input.h"
#include "tools.h"

void clearScreen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

// if something breaks check here
ma_result init_decoder_from_path(const std::filesystem::path& path, ma_decoder_config* pConfig, ma_decoder* pDecoder) {
    #ifdef _WIN32
        return ma_decoder_init_file_w(path.wstring().c_str(), pConfig, pDecoder);
    #else
        return ma_decoder_init_file(path.c_str(), pConfig, pDecoder);
    #endif
}

void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {

    auto* pSongData = static_cast<SongData *>(pDevice->pUserData);

    if (pSongData->playing.load()) {
        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&pSongData->decoder, pOutput, frameCount, &framesRead);

        pSongData->currentFrame.fetch_add(framesRead);

        if (framesRead < frameCount) {
            pSongData->finished.store(true);
        }

        auto* pFloatOutput = static_cast<float *>(pOutput);
        float currentVolume = pSongData->volume.load();

        ma_uint32 totalSamplesRead = framesRead * pSongData->decoder.outputChannels;
        for (ma_uint32 i = 0; i < totalSamplesRead; i++) {
            pFloatOutput[i] *= currentVolume;
        }

        if (framesRead < frameCount) {
            ma_uint32 framesRemaining = frameCount - framesRead;
            size_t emptyBytes = framesRemaining * pDevice->playback.channels * sizeof(float);

            memset(pFloatOutput + totalSamplesRead, 0, emptyBytes);
        }
    } else {
        size_t bufferSizeInBytes = frameCount * pDevice->playback.channels * sizeof(float);
        memset(pOutput, 0, bufferSizeInBytes);
    }
}

void playFolder(std::string& folderpath) {

    setupTerminal();

    //customize terminal
    std::cout << "\033[?25l";                //hide cursor
    std::cout << "\033]0;String-Theory\007"; //window name
    //std::cout << "\033[8;2;80t";             //window size

    //set UTF-8 characters for display on console
    std::setlocale(LC_ALL, ".UTF8");

    std::unordered_map<int, std::string> keymapping = {
        {32, "space"},
    };

    std::vector<std::filesystem::path> playlist = scrapeFolder(folderpath);
    if (playlist.empty()) {
        std::cout << "The folder could not be read." << std::endl;
        return;
    }

    int currentTrackIndex = 0;

    SongData song; //Song struct
    song.volume = 1.0f;
    song.playing = true;
    song.finished = false;
    song.paused = false;

    //setup encoder config
    ma_decoder_config decoder_config = ma_decoder_config_init(ma_format_f32, 2, 44100);

    //check if it worked
    if (init_decoder_from_path(playlist[currentTrackIndex], &decoder_config, &song.decoder) != MA_SUCCESS) {
        std::cout << "Failed to load the audio file. Check the path!" << std::endl;
        return;
    }

    //get metadata for 1st song
    loadID3v2Tag(playlist[currentTrackIndex], song);

    //setup audio configs
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = song.decoder.outputFormat;
    config.playback.channels = song.decoder.outputChannels;
    config.sampleRate = song.decoder.outputSampleRate;
    config.dataCallback = data_callback;
    config.pUserData = &song;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
        std::cout << "Failed to initialize playback device." << std::endl;
        ma_decoder_uninit(&song.decoder);
        return;
    }

    ma_decoder_get_length_in_pcm_frames(&song.decoder, &song.totalFrames);
    ma_device_start(&device); //start music

    std::cout << "[" << currentTrackIndex + 1  << "/"  << playlist.size() << "] Playing " << song.title << " by " << song.artist << " <volume: " << (song.volume * 100) << ">" << std::endl;

    //playlist loop
    while (currentTrackIndex < static_cast<int>(playlist.size())) {

        //handle song finish
        if (song.finished.load()) {
            currentTrackIndex++;

            if (currentTrackIndex >= playlist.size()) {
                clearScreen();
                std::cout << "Finished!" << std::endl;
                break;
            }

            loadID3v2Tag(playlist[currentTrackIndex], song);
            clearScreen();
            std::cout << "[" << currentTrackIndex + 1  << "/"  << playlist.size() << "] Playing " << song.title << " by " << song.artist << " <volume: " << (song.volume * 100) << ">" << std::endl;

            ma_device_stop(&device);
            ma_decoder_uninit(&song.decoder);

            if (init_decoder_from_path(playlist[currentTrackIndex], &decoder_config, &song.decoder) != MA_SUCCESS) {
                song.finished.store(true);
                continue;
            }

            song.currentFrame.store(0);
            song.totalFrames = 0;
            ma_decoder_get_length_in_pcm_frames(&song.decoder, &song.totalFrames);

            song.finished.store(false);
            ma_device_start(&device);
        }

        PlayerCommand cmd = getKeyboardInput();
        switch (cmd) {
            case PlayerCommand::Exit: {
                clearScreen();
                std::cout << "Exiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                currentTrackIndex = static_cast<int>(playlist.size());
                break;
            }
            case PlayerCommand::PlayPause: {
                if (!song.paused.load()) {
                    clearScreen();
                    std::cout << "<paused>" << std::endl;
                    ma_device_stop(&device);
                    song.paused.store(true);
                } else {
                    clearScreen();
                    std::cout << "[" << currentTrackIndex + 1  << "/"  << playlist.size() << "] Playing " << song.title << " by " << song.artist << " <volume: " << (song.volume * 100) << ">" << std::endl;
                    ma_device_start(&device);
                    song.paused.store(false);
                }
                break;
            }
            case PlayerCommand::VolUp: {
                float vol = song.volume.load();
                if (vol < 1.0f) {
                    song.volume.store(std::min(vol + 0.1f, 1.0f));
                    clearScreen();
                    std::cout << "[" << currentTrackIndex + 1  << "/"  << playlist.size() << "] Playing " << song.title << " by " << song.artist << " <volume: " << (song.volume * 100) << ">" << std::endl;
                }
                break;
            }
            case PlayerCommand::VolDown: {
                if (song.volume > 0.0f) {
                    float vol = song.volume.load();
                    vol -= 0.1f;
                    if (vol < 0.05) { vol = 0.0f; }
                    song.volume.store(std::clamp(vol, 0.0f, 1.0f));
                    clearScreen();
                    std::cout << "[" << currentTrackIndex + 1  << "/"  << playlist.size() << "] Playing " << song.title << " by " << song.artist << " <volume: " << (song.volume * 100) << ">" << std::endl;
                }
                break;
            }
            case PlayerCommand::SkipBack: {
                ma_uint64 cursor = song.currentFrame.load();
                double currentSecond = static_cast<double>(cursor) / song.decoder.outputSampleRate;
                if (currentSecond > 2.0) {
                    ma_device_stop(&device);
                    ma_decoder_seek_to_pcm_frame(&song.decoder, 0);
                    song.currentFrame.store(0);
                    ma_device_start(&device);
                } else if (currentSecond <= 2 && currentTrackIndex != 0) {
                    clearScreen();
                    currentTrackIndex -= 2;
                    song.finished.store(true);
                }
                break;
            }
            case PlayerCommand::SkipForward: {
                clearScreen();
                song.finished.store(true);
                break;
            }
            case PlayerCommand::None:
                break;
        }

        if (!song.paused.load()) {
            ma_uint64 cursorFrames = song.currentFrame.load();
            ma_uint64 totalFrames = song.totalFrames;

            int currentSeconds = static_cast<int>(cursorFrames / song.decoder.outputSampleRate);
            int totalSeconds = 0;
            if (song.decoder.outputSampleRate > 0) {
                totalSeconds = static_cast<int>(totalFrames / song.decoder.outputSampleRate);
            }

            int curMin = currentSeconds / 60;
            int curSec = currentSeconds % 60;
            int totMin = totalSeconds / 60;
            int totSec = totalSeconds % 60;

            printf("\r[%02d:%02d/%02d:%02d] ", curMin, curSec, totMin, totSec);
            fflush(stdout);
        }

        //sleep between songs
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    //uninitialize device
    restoreTerminal();
    ma_device_uninit(&device);
    ma_decoder_uninit(&song.decoder);

}