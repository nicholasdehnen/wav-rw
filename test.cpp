#include <cstdio>
#include <iostream>
#include "wav_transceiver.h"

int main(int argc, char * argv[])
{
    WavTransceiver wt;
    try {
        wt.SetInputFile(R"(test.wav)");
        wt.SetOutputFile(R"(out.wav)", 48000);
    } catch(std::exception &e)
    {  
        printf("Exception occurred! : %s", e.what());
        exit(1);
    }
    int counter;
    std::shared_ptr<std::vector<uint8_t>> v;
    printf("I/O file init OK.\n");
    while((v = wt.GetNextData(1)) != nullptr)
    {
        counter++;
        wt.WriteData(v->data(), v->size(), false);
    }
    printf("Read / wrote %d samples.", counter);
    wt.CloseOutputFile();
    return 0;
}