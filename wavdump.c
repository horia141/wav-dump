#include <stdlib.h>
#include <stdio.h>
#include <math.h>

static const unsigned int MIN_FREQ = 20;
static const unsigned int MAX_FREQ = 22050;

// These structures map .wav headers to C structures.
// See http://ccrma.stanford.edu/courses/422/projects/WaveFormat/ to understand why they have the form
// that they have, and for other discussions on .wav files. Some operations of this software
// will seem arcane without proper documentation (although I've tried to keep things rather clean and
// organized).

struct wave_riffhead
{
  char           chunk0id[4];
  unsigned int   chunk0size;
  char           format[4];
};

struct wave_frmthead
{
  char           chunk1id[4];
  unsigned int   clsize;
  unsigned short audioformat;
  unsigned short audiochannels;
  unsigned int   samplerate;
  unsigned int   byterate;
  unsigned short blockalign;
  unsigned short bitspersample;
};

struct wave_datahead
{
  char           chunk2id[4];
  unsigned int   chunk2size;
};

int main(int argc, char** argv)
{
  // Misc.

  int                  i;
  int                  j;
  int                  k;

  // Command-line arguments

  char*                filename; // the output file name.
  unsigned int         filetime; // the duration of the generated signal.

  // This one two are treated as one command line argument. A buffer of frequencies
  // and it's size;
  unsigned int         freqsize;
  unsigned int*        freqbuff;

  // Read command-line arguments. We assume the first argument is the name of the 
  // output .wav file. The second argument is an integer and is the length in seconds
  // of the signal. Every argument after the second is interpreted as a harmonic frequency
  // of the resulting signal.

  if(argc > 3) {
    filename = argv[1];
    filetime = atol(argv[2]);

    // Allocate enough space for the frequencies buffer. Since all arguments after the
    // second are interpreted as frequencies, we allocate argc - 3 integers.
    
    freqsize = argc - 3;
    freqbuff = (unsigned int*)malloc(sizeof(unsigned int) * freqsize);

    for(i = 0; i < freqsize; i++) {
      freqbuff[i] = atol(argv[3 + i]);
    }
  } else {
    printf("Incomplete arguments to wavdump!\n\n");
    printf("Synopis : wavdump generates a windows .wav file by combinging several harmonics into a complex signal.\n\n");
    printf("Syntax  : wavdump\n\t  [output file name]\n\t  [output file duration (greater than 0)]\n\t  [list of frequencies (values in the range [20 - 22k])]\n\n");
    printf("Usage   : wavdump test.wav 5 440 880\n");
    printf("Usage   : wavdump a.wav 10 1000 2000 3000\n\n");

    exit(0);
  }

  // Validate command-line arguments

  if(filetime == 0) {
    printf("Invalid arguments to wavdump!\n\n");
    printf("Argument 'output file duration' should be a pozitive, non-null number!\n");
    printf("Its current value is '%s'!\n\n",argv[2]);
    exit(0);
  }

  for(i = 0; i < freqsize; i++) {
    if(freqbuff[i] < MIN_FREQ || freqbuff[i] > MAX_FREQ) {
      printf("Invalid arguments to wavdump!\n\n");
      printf("Argument 'list of frequencies' contains a frequency outside the range [%d,%d] Hz!\n",MIN_FREQ,MAX_FREQ);
      printf("Its current value is '%s'!\n\n",argv[5 + i]);
      exit(0);
    }
  }

  // .wav data portion

  int                  wavechnl; // number of channels in the file. defaults to 1
  int                  wavebits; // number of bits used to represent each sample. defaults to 16 (2 bytes)
  int                  waverate; // number of samples in each second. defaults to 44.1k (44100)

  int                  wavesize; // the size of the sample buffer
  short*               wavebuff; // the sample buffer. this contains the samples of the generated signal

  // Seed the .wav file descriptiors with default values.

  wavechnl = 1;
  wavebits = 16;
  waverate = MAX_FREQ * 2;

  // We must first allocate a buffer big enough to hold all our samples. To calculate 
  // the space requirements we use the formula : wavechnl * (wavebits / 8) * wavarate * filetime.
  // So, for a 2 second mono signal, with 16 bits per sample and 44100 samples per second, we'll
  // need 1 * (16 / 8) * 44100 * 2 = 176400 bytes.

  wavesize = wavechnl * (wavebits / 8) * waverate * filetime;
  wavebuff = (short*)malloc(wavesize);

  // Then we generate the signal. We use this algorithm :
  //   foreach second of our signal :
  //      foreach sample of our second :
  //         initialize the corresponding entry in the sample buffer to 0
  //         calculate the sample value as the mean of all the values of the harmonics
  //         at this moment in time in second
  //            where : this moment in time t = is j / waverate (the jth sample of the second).
  //                  : calculating the kth harmonic means computing sin ( 2 * pi * freqbuff(k) * t)
  //            obs : we multiply by 32765 (the maximum 2^15 - 1 - the maximum signed short int)
  //                  to bring the floating point value of the sinus from the [-1,1] interval to
  //                  the [-32675,32675] interval.
  //                : we divide the kth harmonic by freqsize - this is so we don't have to used ints
  //                  to hold our intermediate values until we do a mean. as division is distributive,
  //                  we can divide the kth element, and sum it up, instead of doing the sum then dividing,
  //                  and we'll get the same result.

  for(i = 0; i < filetime; i++)
    for(j = 0; j < waverate; j++) {
      wavebuff[i * waverate + j] = 0;
      
      for(k = 0; k < freqsize; k++) {
	wavebuff[i * waverate + j] += sinf((2 * M_PI * freqbuff[k] * j) / waverate) * (32765 / freqsize);
      }
    }

  // Generate the wave file headers.

  // This is all based on the discussion on http://ccrma.stanford.edu/courses/422/projects/WaveFormat/ .
  // We build these structures so we can fwrite them more convinently to the output file. They each map to
  // a section of the header portion of the .wav file.

  // RIFF header for our signal file
  struct wave_riffhead riffhead = {
    {'R','I','F','F'},
    sizeof(struct wave_riffhead) + 
    sizeof(struct wave_frmthead) + 
    sizeof(struct wave_datahead) + wavesize - 8,
    {'W','A','V','E'}
  };

  // Format subchunk header for our signal file
  struct wave_frmthead frmthead = {
    {'f','m','t',' '},
    sizeof(struct wave_frmthead) - 8,
    1, //PCM format
    wavechnl,
    waverate,
    wavechnl * (wavebits / 8) * waverate,
    wavechnl * (wavebits / 8),
    wavebits
  };

  // Data subchunk header for our signal file
  struct wave_datahead datahead = {
    {'d','a','t','a'},
    wavesize
  };

  // Write the file out

  FILE*                ofile;

  // We've generated the wave data and it's .wav headers. Nothing left to do but write them out
  // to our file.

  ofile = fopen(filename,"wb");

  if(!ofile) {
    printf("Could not open file '%s'!\n",filename);
    perror("Reason : ");
    printf("Aborting program!\n");

    free(wavebuff);
    free(freqbuff);
    exit(0);
  }

  fwrite(&riffhead,1,sizeof(riffhead),ofile);
  fwrite(&frmthead,1,sizeof(frmthead),ofile);
  fwrite(&datahead,1,sizeof(datahead),ofile);

  fwrite(wavebuff,1,wavesize,ofile);

  // All is done. Clean up after us and exit.

  fclose(ofile);

  free(wavebuff);
  free(freqbuff);

  return 0;
}
