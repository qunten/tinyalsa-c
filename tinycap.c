/* tinycap.c
**
** Copyright 2011, The Android Open Source Project
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are met:
**     * Redistributions of source code must retain the above copyright
**       notice, this list of conditions and the following disclaimer.
**     * Redistributions in binary form must reproduce the above copyright
**       notice, this list of conditions and the following disclaimer in the
**       documentation and/or other materials provided with the distribution.
**     * Neither the name of The Android Open Source Project nor the names of
**       its contributors may be used to endorse or promote products derived
**       from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY The Android Open Source Project ``AS IS'' AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
** IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
** ARE DISCLAIMED. IN NO EVENT SHALL The Android Open Source Project BE LIABLE
** FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
** DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
** SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
** CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
** LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
** OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
** DAMAGE.
*/
 
#include "asoundlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
 
#include <pthread.h>
#include <sys/time.h>
#include <unistd.h>
 
#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164
 
#define FORMAT_PCM 1
 
 
int capturing = 1;
static int closed = 0;
char *buffer;
pthread_t thread;
unsigned int card = 0;
unsigned int device = 0;
unsigned int channels = 2;
unsigned int rate = 44100;
unsigned int bits = 16;
unsigned int frames;
unsigned int period_size = 1024;
unsigned int period_count = 4;
 
unsigned int size;
 
unsigned int capture_sample(unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count);
 
void play_sample(unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count);                            
 
void sigint_handler(int sig)
{
    capturing = 0;
    closed = 1;
}
 
void thread1(void)
{
    sleep(2);
    printf("I am thread1\n");
 
    play_sample(card, device, channels, rate, bits, period_size, period_count);
}
 
int main(int argc, char **argv)
{
 
    enum pcm_format format;
    signal(SIGINT, sigint_handler);
    
    switch (bits) {
    case 32:
        format = PCM_FORMAT_S32_LE;
        break;
    case 24:
        format = PCM_FORMAT_S24_LE;
        break;
    case 16:
        format = PCM_FORMAT_S16_LE;
        break;
    default:
        printf("%d bits is not supported.\n", bits);
        return 1;
    }
 
     if((pthread_create(&thread, NULL, (void *)thread1, NULL)) == 0)  //comment2     
    {
        printf("Create pthread ok!\n");
    }else{
        printf("Create pthread failed!\n");
    }
        
    /* install signal handler and begin capturing */
    
    frames = capture_sample(card, device, channels,
                            rate, format,
                            period_size, period_count);
 
    
    printf("Captured %d frames\n", frames);                            
 
    return 0;
}
 
unsigned int capture_sample(unsigned int card, unsigned int device,
                            unsigned int channels, unsigned int rate,
                            enum pcm_format format, unsigned int period_size,
                            unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    
    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    config.format = format;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
 
    pcm = pcm_open(card, device, PCM_IN, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        printf("Unable to open PCM device (%s)\n",
                pcm_get_error(pcm));
        return 0;
    }
 
    size = pcm_frames_to_bytes(pcm, pcm_get_buffer_size(pcm));
    buffer = malloc(size);
    if (!buffer) {
        printf("Unable to allocate %d bytes\n", size);
        free(buffer);
        pcm_close(pcm);
        return 0;
    }
 
    printf("Capturing sample: %u ch, %u hz, %u bit\n", channels, rate,
           pcm_format_to_bits(format));
 
    while (capturing && !pcm_read(pcm, buffer, size)) {
       
    }
 
    free(buffer);
    pcm_close(pcm);
    return 0;
}
 
 
 
int check_param(struct pcm_params *params, unsigned int param, unsigned int value,
                 char *param_name, char *param_unit)
{
    unsigned int min;
    unsigned int max;
    int is_within_bounds = 1;
 
    min = pcm_params_get_min(params, param);
    if (value < min) {
        printf("%s is %u%s, device only supports >= %u%s\n", param_name, value,
                param_unit, min, param_unit);
        is_within_bounds = 0;
    }
 
    max = pcm_params_get_max(params, param);
    if (value > max) {
        printf("%s is %u%s, device only supports <= %u%s\n", param_name, value,
                param_unit, max, param_unit);
        is_within_bounds = 0;
    }
 
    return is_within_bounds;
}
 
int sample_is_playable(unsigned int card, unsigned int device, unsigned int channels,
                        unsigned int rate, unsigned int bits, unsigned int period_size,
                        unsigned int period_count)
{
    struct pcm_params *params;
    int can_play;
 
    params = pcm_params_get(card, device, PCM_OUT);
    if (params == NULL) {
        printf("Unable to open PCM device %u.\n", device);
        return 0;
    }
 
    can_play = check_param(params, PCM_PARAM_RATE, rate, "Sample rate", "Hz");
    can_play &= check_param(params, PCM_PARAM_CHANNELS, channels, "Sample", " channels");
    can_play &= check_param(params, PCM_PARAM_SAMPLE_BITS, bits, "Bitrate", " bits");
    can_play &= check_param(params, PCM_PARAM_PERIOD_SIZE, period_size, "Period size", "Hz");
    can_play &= check_param(params, PCM_PARAM_PERIODS, period_count, "Period count", "Hz");
 
    pcm_params_free(params);
 
    return can_play;
}
 
void play_sample(unsigned int card, unsigned int device, unsigned int channels,
                 unsigned int rate, unsigned int bits, unsigned int period_size,
                 unsigned int period_count)
{
    struct pcm_config config;
    struct pcm *pcm;
    
    memset(&config, 0, sizeof(config));
    config.channels = channels;
    config.rate = rate;
    config.period_size = period_size;
    config.period_count = period_count;
    if (bits == 32)
        config.format = PCM_FORMAT_S32_LE;
    else if (bits == 16)
        config.format = PCM_FORMAT_S16_LE;
    else if (bits == 24)
        config.format = PCM_FORMAT_S24_LE;
    config.start_threshold = 0;
    config.stop_threshold = 0;
    config.silence_threshold = 0;
 
 
    if (!sample_is_playable(card, device, channels, rate, bits, period_size, period_count)) {
        return;
    }
 
    pcm = pcm_open(card, device, PCM_OUT, &config);
    if (!pcm || !pcm_is_ready(pcm)) {
        printf("Unable to open PCM device %u (%s)\n",
                device, pcm_get_error(pcm));
        return;
    }
 
 
    printf("Playing sample: %u ch, %u hz, %u bit\n", channels, rate, bits);
 
 
    do {
        
        if (pcm_write(pcm, buffer, size)) {
            printf("Error playing sample\n");
            break;
        }
        
    } while (!closed);
 
    pcm_close(pcm);
}