/*
  Copyright (c) 2019, miya
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
使用方法：
下記のサイトの方法でArduinoとSpresenseボードマネージャのインストール、ブートローダーの書き込みを行います。
https://developer.sony.com/ja/develop/spresense/developer-tools/get-started-using-arduino-ide/set-up-the-arduino-ide

下記の方法でDSPコーデックをSDカードのBINディレクトリ以下にインストールします。
https://developer.sony.com/ja/develop/spresense/developer-tools/get-started-using-arduino-ide/developer-guide#_dsp_codec_%E3%83%90%E3%82%A4%E3%83%8A%E3%83%AA%E3%81%AE%E3%82%A4%E3%83%B3%E3%82%B9%E3%83%88%E3%83%BC%E3%83%AB

その後、Arduino IDEでこのスケッチを書き込みます。
*/

#include <MediaPlayer.h>
#include <OutputMixer.h>
#include <MemoryUtil.h>

const int MIXER_VOLUME = -160;
const int32_t S_BUFFER_SIZE = 8192;
uint8_t s_buffer[S_BUFFER_SIZE];
bool err_flag = false;

// Pin
const int LEDS = 4;
unsigned char LED[] = {LED3,LED2,LED1,LED0};
const int AUDIO_L = 9;
const int AUDIO_R = 10;

// 0xc7ba632a
const uint32_t RANDOM_SEED = 0xc7ba632d;
const int STATE_ATTACK = 0;
const int STATE_DECAY = 1;
const int STATE_SUSTAIN = 2;
const int STATE_RELEASE = 3;
const int STATE_SLEEP = 4;
const int WAVE_BUFFER_BITS = 8;
const int INT_BITS = 32;
const int FIXED_BITS = 14;
const int FIXED_BITS_ENV = 8;
const int WAVE_ADDR_SHIFT = (INT_BITS - WAVE_BUFFER_BITS);
const int WAVE_ADDR_SHIFT_M = (WAVE_ADDR_SHIFT - FIXED_BITS);
const int FIXED_SCALE = (1 << FIXED_BITS);
const int FIXED_SCALE_M1 = (FIXED_SCALE - 1);
const int WAVE_BUFFER_SIZE = (1 << WAVE_BUFFER_BITS);
const int WAVE_BUFFER_SIZE_M1 = (WAVE_BUFFER_SIZE - 1);
const int OPS = 2;
const int CHANNELS = 4;
const int OSCS = (OPS * CHANNELS);
const int TEMPO = 8192;
const int SEQ_LENGTH = 16;
const int SAMPLE_US = 40;
const int ADD_RATE = 6;
const int DEL_RATE = 12;
const int OCT_MIN = 6;
const int OCT_WIDTH = 2;

const int CONST03 = (1 << FIXED_BITS << FIXED_BITS_ENV);
const int CONST04 = (FIXED_SCALE >> 1);
const int VOLUME = (CONST04 / CHANNELS);
const int REVERB_VOLUME = (int)(VOLUME * 1.9);
const int REVERB_BUFFER_SIZE_L = 0x4000;
const int REVERB_BUFFER_SIZE_R = 0x2000;
const int REVERB_LENGTH_L = (int)(REVERB_BUFFER_SIZE_L * 0.99);
const int REVERB_LENGTH_R = (int)(REVERB_BUFFER_SIZE_R * 0.91);
const int REVERB_DECAY = (int)(FIXED_SCALE * 0.7);

typedef struct
{
  int envelopeLevelA;
  int envelopeLevelS;
  int envelopeDiffA;
  int envelopeDiffD;
  int envelopeDiffR;
  int modPatch0;
  int modPatch1;
  int modLevel0;
  int modLevel1;
  int levelL;
  int levelR;
  int levelRev;

  int state;
  int count;
  int currentLevel;
  int pitch;
  int velocity;
  int mod0;
  int mod1;
  int outData;
  int outWaveL;
  int outWaveR;
  int outRevL;
  int outRevR;
  boolean mixOut;
  boolean noteOn;
  boolean noteOnSave;
  boolean goToggle;
  boolean outDoneToggle;
} params_t;

typedef struct
{
  unsigned char note;
  unsigned char oct;
} seqData_t;

short waveData[] = {
  0,402,803,1205,1605,2005,2403,2800,
  3196,3589,3980,4369,4755,5139,5519,5896,
  6269,6639,7004,7365,7722,8075,8422,8764,
  9101,9433,9759,10079,10393,10700,11002,11296,
  11584,11865,12139,12405,12664,12915,13158,13394,
  13621,13841,14052,14254,14448,14633,14810,14977,
  15135,15285,15425,15556,15677,15789,15892,15984,
  16068,16141,16205,16259,16304,16338,16363,16378,
  16383,16378,16363,16338,16304,16259,16205,16141,
  16068,15984,15892,15789,15677,15556,15425,15285,
  15135,14977,14810,14633,14448,14254,14052,13841,
  13621,13394,13158,12915,12664,12405,12139,11865,
  11584,11296,11002,10700,10393,10079,9759,9433,
  9101,8764,8422,8075,7722,7365,7004,6639,
  6269,5896,5519,5139,4755,4369,3980,3589,
  3196,2800,2403,2005,1605,1205,803,402,
  0,-403,-804,-1206,-1606,-2006,-2404,-2801,
  -3197,-3590,-3981,-4370,-4756,-5140,-5520,-5897,
  -6270,-6640,-7005,-7366,-7723,-8076,-8423,-8765,
  -9102,-9434,-9760,-10080,-10394,-10701,-11003,-11297,
  -11585,-11866,-12140,-12406,-12665,-12916,-13159,-13395,
  -13622,-13842,-14053,-14255,-14449,-14634,-14811,-14978,
  -15136,-15286,-15426,-15557,-15678,-15790,-15893,-15985,
  -16069,-16142,-16206,-16260,-16305,-16339,-16364,-16379,
  -16383,-16379,-16364,-16339,-16305,-16260,-16206,-16142,
  -16069,-15985,-15893,-15790,-15678,-15557,-15426,-15286,
  -15136,-14978,-14811,-14634,-14449,-14255,-14053,-13842,
  -13622,-13395,-13159,-12916,-12665,-12406,-12140,-11866,
  -11585,-11297,-11003,-10701,-10394,-10080,-9760,-9434,
  -9102,-8765,-8423,-8076,-7723,-7366,-7005,-6640,
  -6270,-5897,-5520,-5140,-4756,-4370,-3981,-3590,
  -3197,-2801,-2404,-2006,-1606,-1206,-804,-403,
};

int scaleTable[] = {
  153791,162936,172624,182889,193764,205286,217493,230426,
  244128,258644,274024,290319,307582,325872,345249,365779,
};

const int CHORD_LENGTH = 5;
const int CHORDS = 3;
unsigned char chordData[CHORDS][8] = {
  {0,2,4,7,9,0,0,0},
  {0,2,4,7,9,0,0,0},
  {0,2,4,7,9,0,0,0}
};
unsigned char progressionData[CHORDS][2] = {
  {0, 3},
  {0, 3},
  {0, 3}
};
unsigned char bassData[CHORDS] = {4, 2, 1};
unsigned char bassPattern[SEQ_LENGTH] = {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned char toneData[CHANNELS][OPS];

params_t params[OSCS];
seqData_t seqData[OSCS][SEQ_LENGTH];
bool activeCh[OSCS];

short reverbBufferL[REVERB_BUFFER_SIZE_L];
short reverbBufferR[REVERB_BUFFER_SIZE_R];

MediaPlayer *player;
OutputMixer *mixer;
int32_t timer;
int32_t timerNext;
int counter;
int seqCounter;
int barCounter;
int deleteCounter;
int chord;
int note;
int sc;
int reverbAddrL;
int reverbAddrR;
int outL;
int outR;
uint32_t randNum;

static void error_callback(const ErrorAttentionParam *errparam)
{
  if (errparam->error_code > AS_ATTENTION_CODE_WARNING)
  {
    err_flag = true;
  }
}

static void mixer_done_callback(MsgQueId id, MsgType type, AsOutputMixDoneParam *param)
{
  return;
}

static void mixer_send_callback(int32_t id, bool is_end)
{
  AsRequestNextParam next;
  next.type = (!is_end) ? AsNextNormalRequest : AsNextStopResRequest;
  AS_RequestNextPlayerProcess(AS_PLAYER_ID_0, &next);
  return;
}

static bool player_done_callback(AsPlayerEvent event, uint32_t result, uint32_t sub_result)
{
  if (event == AsPlayerEventPlay)
  {
  }
  return true;
}

void player_decode_callback(AsPcmDataParam pcm_param)
{
  int16_t *ls = (int16_t*)pcm_param.mh.getPa();
  int16_t *rs = ls + 1;

  for (uint32_t cnt = 0; cnt < pcm_param.size; cnt += 4)
  {
    process_audio();
    *ls = outL;
    *rs = outR;
    ls += 2;
    rs += 2;
  }
  mixer->sendData(OutputMixer0, mixer_send_callback, pcm_param);
}

uint32_t rand32()
{
  randNum = randNum ^ (randNum << 13);
  randNum = randNum ^ (randNum >> 17);
  randNum = randNum ^ (randNum << 5);
  return randNum;
}

int randI(int max)
{
  uint32_t r = rand32() & 0xffff;
  r = r * max;
  return (r >> 16);
}

void render()
{
  int mixL = 0;
  int mixR = 0;
  int mixRevL = 0;
  int mixRevR = 0;

  for (int i = 0; i < OSCS; i++)
  {
    if (activeCh[i] == true)
    {
      // envelope generator
      switch (params[i].state)
      {
        case STATE_SLEEP:
        {
          break;
        }

        case STATE_ATTACK:
        {
          params[i].currentLevel += params[i].envelopeDiffA;
          if (params[i].currentLevel > params[i].envelopeLevelA)
          {
            params[i].currentLevel = params[i].envelopeLevelA;
            params[i].state = STATE_DECAY;
          }
          break;
        }

        case STATE_DECAY:
        {
          params[i].currentLevel += params[i].envelopeDiffD;
          if (params[i].currentLevel < params[i].envelopeLevelS)
          {
            params[i].currentLevel = params[i].envelopeLevelS;
            params[i].state = STATE_SUSTAIN;
          }
          break;
        }

        case STATE_SUSTAIN:
        {
          break;
        }

        case STATE_RELEASE:
        {
          params[i].currentLevel += params[i].envelopeDiffR;
          if (params[i].currentLevel < 0)
          {
            params[i].currentLevel = 0;
            activeCh[i] = false;
            params[i].state = STATE_SLEEP;
          }
          break;
        }
      }

      params[i].mod0 = params[params[i].modPatch0].outData;
      params[i].mod1 = params[params[i].modPatch1].outData;

      int waveAddr = (unsigned int)(params[i].count +
                                    (params[i].mod0 * params[i].modLevel0) +
                                    (params[i].mod1 * params[i].modLevel1)) >> WAVE_ADDR_SHIFT_M;

      // fetch wave data
      int waveAddrF = waveAddr >> FIXED_BITS;
      int waveAddrR = (waveAddrF + 1) & WAVE_BUFFER_SIZE_M1;    
      int oscOutF = waveData[waveAddrF];
      int oscOutR = waveData[waveAddrR];
      int waveAddrM = waveAddr & FIXED_SCALE_M1;
      int oscOut = ((oscOutF * (FIXED_SCALE - waveAddrM)) >> FIXED_BITS) +
        ((oscOutR * waveAddrM) >> FIXED_BITS);
      params[i].outData = (oscOut * (params[i].currentLevel >> FIXED_BITS_ENV)) >> FIXED_BITS;
      params[i].count += params[i].pitch;

      // mix
      if (params[i].mixOut == 0)
      {
        params[i].outWaveL = 0;
        params[i].outWaveR = 0;
        params[i].outRevL = 0;
        params[i].outRevR = 0;
      }
      else
      {
        params[i].outWaveL = (params[i].outData * params[i].levelL) >> FIXED_BITS;
        params[i].outWaveR = (params[i].outData * params[i].levelR) >> FIXED_BITS;
        params[i].outRevL = (params[i].outWaveL * params[i].levelRev) >> FIXED_BITS;
        params[i].outRevR = (params[i].outWaveR * params[i].levelRev) >> FIXED_BITS;
      }

      mixL += params[i].outWaveL;
      mixR += params[i].outWaveR;
      mixRevL += params[i].outRevL;
      mixRevR += params[i].outRevR;
    }
  }

  // reverb
  int reverbL = ((int)reverbBufferR[reverbAddrR] * REVERB_DECAY) >> FIXED_BITS;
  int reverbR = ((int)reverbBufferL[reverbAddrL] * REVERB_DECAY) >> FIXED_BITS;
  reverbL += mixRevR;
  reverbR += mixRevL;
  reverbBufferL[reverbAddrL] = reverbL;
  reverbBufferR[reverbAddrR] = reverbR;
  reverbAddrL++;
  if (reverbAddrL > REVERB_LENGTH_L)
  {
    reverbAddrL = 0;
  }
  reverbAddrR++;
  if (reverbAddrR > REVERB_LENGTH_R)
  {
    reverbAddrR = 0;
  }
  outL = mixL + reverbBufferL[reverbAddrL];
  outR = mixR + reverbBufferR[reverbAddrR];
}

void setup()
{
  counter = 0;
  seqCounter = 0;
  barCounter = 0;
  deleteCounter = 0;
  chord = 0;
  note = 0;
  sc = 0;
  reverbAddrL = 0;
  reverbAddrR = 0;

  randNum = RANDOM_SEED;

  for (int i = 0; i < OSCS; i++)
  {
    activeCh[i] = false;
    params[i].state = STATE_SLEEP;
    params[i].envelopeLevelA = CONST03;
    params[i].envelopeLevelS = 0;
    params[i].envelopeDiffA = CONST03 >> (randI(9) + 4);
    params[i].envelopeDiffD = (0 - CONST03) >> (randI(9) + 13);
    params[i].envelopeDiffR = (0 - CONST03) >> 13;
    params[i].levelL = VOLUME;
    params[i].levelR = VOLUME;
    params[i].levelRev = REVERB_VOLUME;
    params[i].mixOut = true;
    params[i].modLevel0 = FIXED_SCALE * randI(5);
    params[i].modPatch0 = i;
    params[i].modPatch1 = i;
    params[i].modLevel1 = 0;
  }
  for (int i = 0; i < CHANNELS; i++)
  {
    int osc = i * OPS;
    params[osc].modPatch0 = osc + 1;
    params[osc + 1].modPatch0 = osc + 1;
    params[osc + 1].modLevel0 = 0;
    params[osc + 1].mixOut = false;
  }
  params[4].levelL = VOLUME >> 1;
  params[4].levelR = VOLUME;
  params[6].levelL = VOLUME;
  params[6].levelR = VOLUME >> 1;

  for (int i = 0; i < LEDS; i++)
  {
    pinMode(LED[i], OUTPUT);
    digitalWrite(LED[i], LOW);
  }

  for (int i = 0; i < CHANNELS; i++)
  {
    for (int j = 0; j < OPS; j++)
    {
      toneData[i][j] = 0;
    }
  }

  for (int i = 0; i < 3; i++)
  {
    bassPattern[randI(12)] = 1;
  }

  // init spresense audio
  initMemoryPools();
  createStaticPools(MEM_LAYOUT_PLAYER);
  player = MediaPlayer::getInstance();
  mixer = OutputMixer::getInstance();
  player->begin();
  mixer->activateBaseband();
  player->create(MediaPlayer::Player0, error_callback);
  mixer->create(error_callback);
  player->activate(MediaPlayer::Player0, player_done_callback);
  mixer->activate(OutputMixer0, mixer_done_callback);
  usleep(100 * 1000);
  player->init(MediaPlayer::Player0, AS_CODECTYPE_WAV, "/mnt/sd0/BIN", AS_SAMPLINGRATE_48000, AS_BITLENGTH_16, AS_CHANNEL_STEREO);
  mixer->setVolume(MIXER_VOLUME, 0, 0);

  // start audio
  memset(s_buffer, 0, sizeof(s_buffer));
  player->writeFrames(MediaPlayer::Player0, s_buffer, S_BUFFER_SIZE);
  player->start(MediaPlayer::Player0, player_decode_callback);
}

void process_audio()
{
  // sequencer
  counter++;
  if (counter > TEMPO)
  {
    counter = 0;
    if (seqCounter >= SEQ_LENGTH)
    {
      seqCounter = 0;
      if ((barCounter & 3) == 0)
      {
        // tone change
        for (int i = 0; i < CHANNELS; i++)
        {
          int osc = i * OPS;
          params[osc].modLevel0 = FIXED_SCALE * randI(5);
          params[osc + 1].modLevel0 = FIXED_SCALE * randI(4);
          for (int j = 0; j < OPS; j++)
          {
            osc = i * OPS + j;
            params[osc].envelopeDiffA = CONST03 >> (randI(10) + 3);
            params[osc].envelopeDiffD = -CONST03 >> (randI(9) + 13);
            toneData[i][j] = randI(3) - 1;
          }
        }
        // chord change
        chord = randI(progressionData[chord][1]) + progressionData[chord][0];
        // delete rate change
        deleteCounter = randI(DEL_RATE);
      }
      // delete note
      for (int i = 0; i < deleteCounter; i++)
      {
        seqData[randI(CHANNELS)][randI(SEQ_LENGTH)].oct = 0;
      }
      // add note
      for (int i = 0; i < ADD_RATE; i++)
      {
        int ch = randI(CHANNELS);
        int beat = randI(SEQ_LENGTH);
        int beat_prev = beat - 1;
        if (beat_prev < 0)
        {
          beat_prev += SEQ_LENGTH;
        }
        seqData[ch][beat].note = randI(CHORD_LENGTH);
        seqData[ch][beat].oct = randI(OCT_WIDTH) + OCT_MIN;
        seqData[ch][beat_prev].oct = 0;
      }
      // add bass
      for (int i = 0; i < SEQ_LENGTH; i++)
      {
        if (bassPattern[i] == 1)
        {
          seqData[0][i].note = bassData[chord];
          seqData[0][i].oct = OCT_MIN;
        }
      }
      barCounter++;
    }
    // note on/off
    for (int i = 0; i < CHANNELS; i++)
    {
      if (seqData[i][seqCounter].oct != 0)
      {
        int n = chordData[chord][seqData[i][seqCounter].note];
        for (int j = 0; j < OPS; j++)
        {
          int osc = i * OPS + j;
          params[osc].pitch = scaleTable[n] << (seqData[i][seqCounter].oct + toneData[i][j]);
          params[osc].state = STATE_ATTACK;
          params[osc].noteOn = true;
          activeCh[osc] = true;
        }
      }
      else
      {
        for (int j = 0; j < OPS; j++)
        {
          int osc = i * OPS + j;
          if (params[osc].noteOn == true)
          {
            params[osc].state = STATE_RELEASE;
            params[osc].noteOn = false;
          }
        }
      }
    }
    seqCounter++;

    // LED control
    for (int i = 0; i < LEDS; i++)
    {
      if (params[i * OPS].noteOn)
      {
        digitalWrite(LED[i], HIGH);
      }
      else
      {
        digitalWrite(LED[i], LOW);
      }
    }
  }

  render();
}

void loop()
{
  player->writeFrames(MediaPlayer::Player0, s_buffer, S_BUFFER_SIZE);

  if (err_flag)
  {
    puts("Error!");
    player->stop(MediaPlayer::Player0);
    exit(1);
  }

  usleep(100);
  return;
}
