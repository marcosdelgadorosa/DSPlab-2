#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>

#include "fdacoefs.h"

jack_port_t *input_port_left;
jack_port_t *output_port_left;
jack_port_t *input_port_right;
jack_port_t *output_port_right;
jack_client_t *client;

// filter taps
//jack_default_audio_sample_t taps[BL];

int processSampleDirect (jack_nframes_t nframes, void *arg) {
  jack_default_audio_sample_t *in, *out;

  static jack_default_audio_sample_t *dl = NULL;   // delay line (circular)
  static jack_default_audio_sample_t *t  = NULL;   // taps copy
  static int bl = 0;                               // number of taps
  static size_t wr = 0;                            // write index

  if (!dl) {
    bl = BL;
    if (bl <= 0) return 0;
    dl = (jack_default_audio_sample_t*)calloc((size_t)bl, sizeof(*dl));
    t  = (jack_default_audio_sample_t*)malloc((size_t)bl * sizeof(*t));
    if (!dl || !t) return 0;

    for (int k = 0; k < bl; ++k) t[k] = (jack_default_audio_sample_t)B[k];
    wr = 0;
  }


  in  = jack_port_get_buffer(input_port_left,  nframes);
  out = jack_port_get_buffer(output_port_left, nframes);

  for (jack_nframes_t i = 0; i < nframes; ++i) {
    dl[wr] = in[i];

    jack_default_audio_sample_t acc = 0.0f;
    size_t idx = wr;
    for (int k = 0; k < bl; ++k) {
      acc += t[k] * dl[idx];
      idx = (idx == 0) ? (size_t)(bl - 1) : (idx - 1);  // Step to past sample
    }
    out[i] = acc;

    wr++;
    if ((int)wr == bl) wr = 0;
  }

  in  = jack_port_get_buffer(input_port_right,  nframes);
  out = jack_port_get_buffer(output_port_right, nframes);
  memcpy(out, in, sizeof (jack_default_audio_sample_t) * nframes);

  return 0;
}

int processSampleOptimized (jack_nframes_t nframes, void *arg) {

  jack_default_audio_sample_t *in, *out;

  static jack_default_audio_sample_t *dl = NULL;   // delay line (circular)
  static jack_default_audio_sample_t *t  = NULL;   // taps copy (half + center)
  static int bl = 0;                               // number of taps
  static size_t wr = 0;                            // write index (most recent)

  if (!dl) {
    bl = BL;
    if (bl <= 0) return 0;

    dl = (jack_default_audio_sample_t*)calloc((size_t)bl,
                                              sizeof(*dl));
    t  = (jack_default_audio_sample_t*)malloc(
            (size_t)((bl + 1) / 2) * sizeof(*t));

    if (!dl || !t) return 0;

    for (int k = 0; k < (bl + 1) / 2; ++k)
      t[k] = (jack_default_audio_sample_t)B[k];

    wr = 0;
  }

   in  = jack_port_get_buffer(input_port_left,  nframes);
   out = jack_port_get_buffer(output_port_left, nframes);

  for (jack_nframes_t i = 0; i < nframes; ++i) {

    dl[wr] = in[i];

    jack_default_audio_sample_t acc = 0.0f;

    // Pair up symmetric samples around the center of the impulse response
    int left  = (int)wr;  // starts at x[n]
    int right = (int)wr;  // will walk forward

    for (int k = 0; k < bl / 2; ++k) {
      int idx1 = left;              // x[n - k]
      left = (left - 1 + bl) % bl;  // move one sample older

      right = (right + 1) % bl;     // move one sample further in the other direction
      int idx2 = right;             // x[n - (BL-1-k)]

      acc += t[k] * (dl[idx1] + dl[idx2]);
    }

    // Odd length: add center tap (unpaired)
    if (bl & 1) {
      int center = (int)((wr + bl) - bl / 2) % bl; // x[n - (BL-1)/2]
      acc += t[bl / 2] * dl[center];
    }

    out[i] = acc;

    wr++;
    if (wr == (size_t)bl) wr = 0;
  }

  in  = jack_port_get_buffer(input_port_right,  nframes);
  out = jack_port_get_buffer(output_port_right, nframes);
  memcpy(out, in, sizeof (jack_default_audio_sample_t) * nframes);

  return 0;
}



int processSampleTransposed (jack_nframes_t nframes, void *arg) {

  jack_default_audio_sample_t *in, *out;

  static jack_default_audio_sample_t *t  = NULL;   // taps copy
  static jack_default_audio_sample_t *s  = NULL;   // transposed-form states
  static int bl = 0;                               // number of taps

  if (!t) {
    bl = BL;                       /* BL and B[] come from fdacoefs.h */
    if (bl <= 0) return 0;


    t = (jack_default_audio_sample_t*)malloc((size_t)bl * sizeof(*t));
    if (!t) return 0;
    for (int k = 0; k < bl; ++k)
      t[k] = (jack_default_audio_sample_t)B[k];

    // Allocate states: BL-1 of them (0 if BL == 1)
    if (bl > 1) {
      s = (jack_default_audio_sample_t*)calloc((size_t)(bl - 1),
                                               sizeof(*s));
      if (!s) return 0;
    }
  }

  in  = jack_port_get_buffer(input_port_left,  nframes);
  out = jack_port_get_buffer(output_port_left, nframes);

  for (jack_nframes_t i = 0; i < nframes; ++i) {
    jack_default_audio_sample_t x = in[i];
    jack_default_audio_sample_t y;

    if (bl == 1) {

      y = t[0] * x;
    } else {

      y = t[0] * x + s[0];


      for (int k = 0; k < bl - 2; ++k) {
        s[k] = t[k + 1] * x + s[k + 1];
      }
      s[bl - 2] = t[bl - 1] * x;
    }

    out[i] = y;
  }


  in  = jack_port_get_buffer(input_port_right,  nframes);
  out = jack_port_get_buffer(output_port_right, nframes);
  memcpy(out, in, sizeof (jack_default_audio_sample_t) * nframes);


  return 0;
}


void jack_shutdown (void *arg) {
        exit (1);
}

int main (int argc, char *argv[]) {
  const char **ports;
  const char *client_name = "simple";
  const char *server_name = NULL;
  jack_options_t options = JackNullOption;
  jack_status_t status;

  client = jack_client_open (client_name, options, &status, server_name);
  if (client == NULL) {
    fprintf (stderr, "jack_client_open() failed, status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf (stderr, "Unable to connect to JACK server\n");
    }
    exit (1);
  }
  if (status & JackServerStarted) {
    fprintf (stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf (stderr, "unique name `%s' assigned\n", client_name);
  }

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <0|1|2>\n", argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "0") == 0) {
    jack_set_process_callback (client, processSampleDirect, 0);
    fprintf(stderr, "Running processSampleDirect\n");
  } else if (strcmp(argv[1], "1") == 0) {
    jack_set_process_callback (client, processSampleOptimized, 0);
    fprintf(stderr, "Running processSampleOptimized\n");
  } else if (strcmp(argv[1], "2") == 0) {
    jack_set_process_callback (client, processSampleTransposed, 0);
    fprintf(stderr, "Running processSampleTransposed\n");
  }else {
    printf("Invalid argument: %s\n", argv[1]);
    return 1;
  }

  jack_on_shutdown (client, jack_shutdown, 0);

  printf ("engine sample rate: %" PRIu32 "\n", jack_get_sample_rate (client));

  input_port_left = jack_port_register (client, "input_left",
                                   JACK_DEFAULT_AUDIO_TYPE,
                                   JackPortIsInput, 0);
  output_port_left = jack_port_register (client, "output_left",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput, 0);
  input_port_right = jack_port_register (client, "input_right",
                                   JACK_DEFAULT_AUDIO_TYPE,
                                   JackPortIsInput, 0);
  output_port_right = jack_port_register (client, "output_right",
                                    JACK_DEFAULT_AUDIO_TYPE,
                                    JackPortIsOutput, 0);

  if ((input_port_left == NULL) || (output_port_left == NULL) ||
      (input_port_right == NULL) || (output_port_right == NULL)) {
    fprintf(stderr, "no more JACK ports available\n");
    exit (1);
  }

  if (jack_activate (client)) {
    fprintf (stderr, "cannot activate client");
    exit (1);
  }

  while (1)
    sleep(10);

  jack_client_close (client);
  
  exit (0);
}
