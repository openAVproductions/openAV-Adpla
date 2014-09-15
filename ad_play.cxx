/*
 * Author: Harry van Haaren 2014
 *         harryhaaren@gmail.com
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/** This project is created for restaurants to stream music, and every 1/2 hour
 *  it will play an advert, while reducing the volume of the music in the 
 *  background.
 */

#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <jack/jack.h>

#include "dsp_adsr.hxx"

#include <sndfile.h>

static float* loadSample( long& length )
{
  SF_INFO info;
  SNDFILE* const sndfile = sf_open( "advert.wav", SFM_READ, &info);
  
  
  if (!sndfile) // || !info.frames ) { // || (info.channels != 1)) {
  {
    printf("!sndfile\n");
    return NULL;
  }
  
  // Read data
  float* data = (float*)malloc(sizeof(float) * info.frames * info.channels );
  if (!data) {
    printf("Failed to allocate memory for sample\n");
    return NULL;
  }
  
  sf_seek(sndfile, 0ul, SEEK_SET);
  sf_read_float(sndfile, data, info.frames * info.channels);
  sf_close(sndfile);
  
  int chnls = info.channels;
  
  length = info.frames * info.channels;
  
  return data;
}

class Engine
{
  public:
    Engine( int mins, float fadeTime, float volReduc )
    {
      client = jack_client_open( "Adpla", JackNullOption, 0 );
      int sr = jack_get_sample_rate( client );
      
      // samples = sampleRate * toGetSeconds * toGetMinutes * mins
      sampsBetweenPlays = sr * 60 * mins;
      
      frameCounter = 0;
      
      sampleIndex = 0;
      sampleLength = 0;
      
      names[0] = "left_in";
      names[1] = "right_in";
      names[2] = "left_out";
      names[3] = "right_out";
      
      adsr = new ADSR();
      
      if( fadeTime > 1.0 ) fadeTime = 1.0;
      if( fadeTime < 0.0 ) fadeTime = 0.0;
      float v  = (pow( 3, fadeTime * 1.63 ) - 0.99 ) * sr;
      adsr->setAttackRate ( v );
      adsr->setReleaseRate( v );
      
      for( int i = 0; i < PORT_COUNT; i++)
      {
        int isInput = JackPortIsInput;
        if ( i >= 2 )
          isInput = JackPortIsOutput;
        
        ports[i] = jack_port_register(  client,
                                        names[i], 
                                        JACK_DEFAULT_AUDIO_TYPE,
                                        isInput, 0 );
      }
      
      jack_set_process_callback( client, static_process, this );
      
      jack_activate( client );
      
      sample = loadSample( sampleLength );
      
      // don't play on startup
      sampleIndex = sampleLength;
      
      if ( !sample )
      {
        printf("Error loading sample! File doesn't exist? Quitting.\n");
      }
      
      // enable music
      adsr->gate( true );
      
      gateOffDone = false;
    }
    
  private:
    jack_client_t*  client;
    char* names[4];
    jack_port_t*    ports[4];
    
    long frameCounter;
    
    long long sampsBetweenPlays;
    
    float* sample;
    long   sampleIndex;
    long   sampleLength;
    
    ADSR* adsr;
    
    bool gateOffDone;
    float volReduc;
    
    enum PORTS {
      IN_L = 0,
      IN_R,
      OUT_R,
      OUT_L,
      PORT_COUNT
    };
    
    void playAd()
    {
      printf("playing ad!\n");
      adsr->gate( true );
      sampleIndex = 0;
    }
    
    // JACK callback
    int process(jack_nframes_t nframes)
    {
      float* portBufs[PORT_COUNT];
      for(int i = 0; i < PORT_COUNT; i++ )
        portBufs[i] = (float*)jack_port_get_buffer( ports[i], nframes );
      
      /// check if we need to trigger an ad
      if ( frameCounter > sampsBetweenPlays )
      {
        playAd();
        frameCounter = 0;
        gateOffDone = false;
      }
      
      float adsrOut = 0;
      for(int i = 0; i < nframes; i++ )
      {
        float tmpL = 0;
        float tmpR = 0;
        
        if( sampleLength > sampleIndex )
        {
          if ( adsr->getState() == ADSR::ENV_SUSTAIN )
          {
            tmpL = sample[sampleIndex++];
            tmpR = sample[sampleIndex++];
          }
        }
        else
        {
          if ( !gateOffDone )
            adsr->gate( false );
          gateOffDone = true;
        }
        
        adsrOut = adsr->process();
        portBufs[OUT_L][i] = tmpL + portBufs[IN_L][i] * (1-(adsrOut / 1. + volReduc) );
        portBufs[OUT_R][i] = tmpR + portBufs[IN_R][i] * (1-(adsrOut / 1. + volReduc) );
      }
      
      frameCounter += nframes;
      
      // for debugging time
      //printf("coutn %li, sampsBtwn %li\n", frameCounter, sampsBetweenPlays );
      
      return 0;
    }
    static int static_process(jack_nframes_t nframes, void* instance)
    {
      return static_cast<Engine*>(instance)->process(nframes);
    }
};


int main( int argc, char** argv)
{
  float timeBetween = 30;
  float fadeTime = 1.0;
  float volReduc = 0.3;
  
  for(int i = 0; i < argc -1; i++)
  {
    if( strcmp( argv[i], "-fade" ) == 0 )
    {
      int fT = atoi( argv[i+1] );
      fadeTime = fadeTime + fT/10.;
    }
    if( strcmp( argv[i], "-time" ) == 0 )
    {
      int tBet = atoi( argv[i+1] );
      timeBetween = tBet;
    }
  }
  
  printf("Adpla :\n\tTime: %i min,\n\tFades: %f\n\tVolume Reduction: %f\n",
          int(timeBetween),
          fadeTime,
          volReduc );
  
  Engine e( timeBetween, fadeTime, volReduc);
  
  /// just stall the thread, JACK's callback
  /// will handle the audio processing
  sleep(-1);
  
  return 0;
}





