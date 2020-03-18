#include <corecrt.h>
#include <stdio.h>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cmath>

#include "wav_transceiver.h"

WavTransceiver::WavTransceiver()
{
    //Does nothing
}

WavTransceiver::~WavTransceiver()
{
    //Flush the buffers and close the file.
    this->CloseInputFile();
    this->CloseOutputFile();
}

WavTransceiver::WavTransceiver(std::filesystem::path input_file,
                               std::filesystem::path output_file)
{
    //Call our setters.
    this->SetInputFile(input_file);
    this->SetOutputFile(output_file);
}

void WavTransceiver::SetInputFile(std::filesystem::path input_file)
{
    if(!std::filesystem::exists(input_file))
    {
        throw std::runtime_error("Input file does not exist.");
    }

    this->input_stream = std::ifstream(input_file, std::ios::binary | std::ios::in);
    if(!input_stream.is_open())
    {
        throw std::runtime_error("Failed to open input file!");
    }
    this->input_header = std::make_unique<wav_header_t>();
    this->input_stream.seekg(0, std::ios::beg);
    this->input_stream.read(reinterpret_cast<char*>(this->input_header.get()), sizeof(wav_header_t));

    if(strncmp(this->input_header->riff_chunk.chunk_id, "RIFF", 4) != 0) {
        throw std::runtime_error("Input file is not a RIFF file.");
    }
    else if(strncmp(this->input_header->riff_chunk.riff_type, "WAVE", 4) != 0) {
        throw std::runtime_error("Input file is not a RIFF/WAVE file.");
    }
    else if(strncmp(this->input_header->fmt_chunk.chunk_id, "fmt", 3) != 0) {
        throw std::runtime_error("Could not find format information in input file.");
    }
    else if(this->input_header->fmt_chunk.compression_type != 1) {
        throw std::runtime_error("Only uncompressed RIFF/WAVE is supported.");
    }
    else if(this->input_header->fmt_chunk.chunk_size != 16) {
        throw std::runtime_error("RIFF/WAVE with extra format data is not supported.");
    }

    //Find data chunk..
    data_chunk_t chunk;
    while(strncmp(chunk.chunk_id, "data", 4) != 0)
    {
        if(this->input_stream.eof())
        {
            throw std::runtime_error("Could not find data chunk in input file.");
        }
        this->input_stream.read(reinterpret_cast<char*>(&chunk), sizeof(data_chunk_t));
    }
}

void WavTransceiver::SetOutputFile(std::filesystem::path output_file, unsigned long slice_rate, unsigned long data_rate)
{
    if(std::filesystem::exists(output_file))
    {
        throw std::runtime_error("Output file already exists! Refusing to overwrite it.");
    }

    this->output_stream = std::ofstream(output_file, std::ios::binary | std::ios::out);
    if(!output_stream.is_open())
    {
        throw std::runtime_error("Failed to open output file!");
    }
    
    //Set some sensible default values for our output
    //This is really just a very basic implementation of the RIFF/WAVE standard, as we only care about getting our payload written to a file, not being super compatible with everything.
    this->output_header = std::make_unique<wav_header_t>();
    riff_chunk_t riff_chunk = {
        {'R', 'I', 'F', 'F'}, // riff tag
        28,                   // total size - 8 bytes; header only: 36 - 8 = 28
        {'W', 'A', 'V', 'E'}  // wave format tag
    };
    fmt_chunk_t fmt_chunk = {
        {'f', 'm', 't', ' '}, // format info tag
        16,                   // Default format length
        1,                    // Uncompressed audio
        2,                    // Stereo ( 2 channels )
        slice_rate,           // Slice rate, default = 48000, will change pitch if mismatched
        data_rate,            // Data rate, default = 176400, unknown effect
        4,                    // Default block alignment
        16                    // Default sample depth
    };
    output_header->riff_chunk = riff_chunk;
    output_header->fmt_chunk = fmt_chunk;

    //Write the RIFF/WAVE + fmt header
    this->output_stream.write(reinterpret_cast<char*>(this->output_header.get()), sizeof(wav_header_t)); // should write 36 bytes

    //Create and write the data header
    data_chunk_t data_chunk = {
        {'d', 'a', 't', 'a'}, // data tag
        8,                    // total data size; header only: 8
    };
    this->output_stream.write(reinterpret_cast<char*>(&data_chunk), sizeof(data_chunk));
}

void WavTransceiver::WriteData(uint8_t * data, unsigned long size, bool flush)
{
    //Bit of thread safety here, ensure nobody messes with our write pointer positions
    std::lock_guard<std::mutex> lock_guard(this->write_lock); //automatically unlocks on loss of scope
    this->output_stream.write(reinterpret_cast<char*>(data), size);

    //Save position and jump to the file header
    unsigned long end = this->output_stream.tellp();
    this->output_stream.seekp(4, std::ios::beg); // jumps to total size field

    //Write new total size
    size_dword_t new_size;
    new_size.size = end - 8;
    this->output_stream.write(new_size.bytes, sizeof(size_dword_t));

    //Jump to data chunk header
    this->output_stream.seekp(sizeof(wav_header_t) + 4, std::ios::beg);

    //Write new sample size
    size_dword_t sample_size;
    sample_size.size = end - sizeof(wav_header_t);
    this->output_stream.write(sample_size.bytes, sizeof(size_dword_t));

    //Go back to the end in anticipation of our next write and flush the write buffer
    this->output_stream.seekp(0, std::ios::end);
    if(flush) {
        this->output_stream.flush();
    }
}

std::shared_ptr<std::vector<uint8_t>> WavTransceiver::GetNextData(int sample_count)
{
    if(this->input_header == nullptr)
    {
        throw std::runtime_error("GetNextData(), although the input file is not initialised.");
    }
    std::shared_ptr<std::vector<uint8_t>> v = std::make_shared<std::vector<uint8_t>>(this->GetInputSampleSize() * sample_count);
    if(this->input_stream.eof())
    {
        return nullptr;
    }
    this->input_stream.read(reinterpret_cast<char*>(v->data()), this->GetInputSampleSize() * sample_count);
    return v;
}

int WavTransceiver::GetInputSampleSize() {
    return this->input_header->fmt_chunk.sample_depth / 8;
}

void WavTransceiver::CloseInputFile()
{
    if(this->input_stream.is_open())
    {
        this->input_stream.close();
    }
}

void WavTransceiver::CloseOutputFile()
{
    if(this->output_stream.is_open())
    {
        this->output_stream.flush();
        this->output_stream.close();
    }
}