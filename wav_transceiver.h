#pragma once
#include <fstream>
#include <string>
#include <filesystem>
#include <memory>
#include <mutex>

// RIFF format specific structs
struct riff_chunk_t {
    char chunk_id[4];
    unsigned long chunk_size;
    char riff_type[4];
};

struct fmt_chunk_t {
    char chunk_id[4];
    unsigned long chunk_size;
    unsigned short compression_type;
    unsigned short channels;
    unsigned long slice_rate;
    unsigned long data_rate;
    unsigned short block_alignment;
    unsigned short sample_depth;
};

struct data_chunk_t {
    char chunk_id[4];
    unsigned long chunk_size;
};

struct wav_header_t {
    riff_chunk_t riff_chunk;
    fmt_chunk_t fmt_chunk;
};

union size_dword_t {
    char bytes[4];
    unsigned long size;
};

/**
 * @brief Very simple RIFF/WAVE de- and encoder.
 * Only supports RIFF/WAVE uncompressed audio files. 
 * Note: Does not do any padding while writing, hence the output file most likely having a crackling sound at the end.
 */
class WavTransceiver {
    std::ifstream input_stream;
    std::ofstream output_stream;

    std::mutex write_lock;

    std::unique_ptr<wav_header_t> input_header;
    std::unique_ptr<wav_header_t> output_header;

    public:
        WavTransceiver();
        ~WavTransceiver();
        WavTransceiver(std::filesystem::path input_file,
                      std::filesystem::path output_file);
        
        /**
         * @brief Set the input file. The file will be opened immediately. Throws an exception on failure.
         * 
         * @param input_file File to read data from.
         */
        void SetInputFile(std::filesystem::path input_file);

        /**
         * @brief Set the output file. The file will be opened immediately. Throws an exception on failure.
         * 
         * @param output_file File to write data to.
         */
        void SetOutputFile(std::filesystem::path output_file, unsigned long slice_rate = 48000, unsigned long data_rate = 176400);

        /**
         * @brief Get data from the input file.
         * 
         * @param sample_count The amount of samples to get.
         * @return std::shared_ptr<std::vector<uint8_t>> A vector of bytes containing the requested samples.
         */
        std::shared_ptr<std::vector<uint8_t>> GetNextData(int sample_count = 1);

        /**
         * @brief Append a chunk of WAVE data to the output file.
         * 
         * @param data Audio data.
         * @param size Data length.
         * @param flush Immediately flush the write buffer (default: true).
         */
        void WriteData(uint8_t * data, unsigned long size, bool flush = true);

        /**
         * @brief Flushes the write buffer and closes the output file.
         * 
         */
        void CloseOutputFile();

        /**
         * @brief Closes the input file.
         * 
         */
        void CloseInputFile();

        /**
         * @brief Get the size (in bytes) of samples in the input file.
         * 
         * @return int Size of a single sample.
         */
        int GetInputSampleSize();
};