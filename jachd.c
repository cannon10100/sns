/* -*- mode: C; c-basic-offset: 2  -*- */
/*
 * Copyright (c) 2010, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of the Georgia Tech Research Corporation nor
 *       the names of its contributors may be used to endorse or
 *       promote products derived from this software without specific
 *       prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GEORGIA TECH RESEARCH CORPORATION ''AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL GEORGIA
 * TECH RESEARCH CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/** Author: jscholz
 */

//#define JNAV_MOTION_MAX 512
#define JACH_NBUTTONS 10 //11
#define JACH_NAXES 6

#include <argp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include <somatic.h>
#include <ach.h>

#include <somatic/util.h>
#include <somatic.pb-c.h>

#include <spnav.h>

#include "include/js.h"
#include "include/jach.h"

/* ----------- */
/* GLOBAL VARS */
/* ----------- */

/* Option Vars */
static int opt_jsdev = 0;
static int opt_verbosity = 0;
static int opt_create = 0;
static char *opt_ach_chan = "joystick-data";
static int opt_axis_cnt = JACH_NAXES;

/* ---------- */
/* ARGP Junk  */
/* ---------- */
static struct argp_option options[] = {
    {
        .name = "jsdev",
        .key = 'j',
        .arg = "device-num",
        .flags = 0,
        .doc = "specifies which device to use (default 0)"
    },
    {
        .name = "verbose",
        .key = 'v',
        .arg = NULL,
        .flags = 0,
        .doc = "Causes verbose output"
    },
    {
        .name = "channel",
        .key = 'c',
        .arg = "channel",
        .flags = 0,
        .doc = "ach channel to use (default \"joystick-data\")"
    },
    {
        .name = "axes",
        .key = 'a',
        .arg = "axis-count",
        .flags = 0,
        .doc = "number of joystick axes (default to 6)"
    },
    {
        .name = "Create",
        .key = 'C',
        .arg = NULL,
        .flags = 0,
        .doc = "Create channel with specified name (off by default)"
    },
    {
        .name = NULL,
        .key = 0,
        .arg = NULL,
        .flags = 0,
        .doc = NULL
    }
};


/// argp parsing function
static int parse_opt( int key, char *arg, struct argp_state *state);
/// argp program version
const char *argp_program_version = "jsd-0.0.1";
/// argp program arguments documention
static char args_doc[] = "";
/// argp program doc line
static char doc[] = "reads from linux joystick and pushes out ach messages";
/// argp object
static struct argp argp = {options, parse_opt, args_doc, doc, NULL, NULL, NULL };


static int parse_opt( int key, char *arg, struct argp_state *state) {
    (void) state; // ignore unused parameter
    switch(key) {
    case 'j':
        opt_jsdev = atoi(arg);
        break;
    case 'v':
        opt_verbosity++;
        break;
    case 'c':
        opt_ach_chan = strdup( arg );
        break;
    case 'a':
        opt_axis_cnt = atoi(arg);
        break;
    case 'C':
    	opt_create = 1;
    case 0:
        break;
    }
    return 0;
}

/* ------------- */
/* Function Defs */
/* ------------- */

/**
 * Block, waiting for a mouse event
 */
void jach_read_to_msg(Somatic__Joystick *msg, js_t *js)
{
	int status = js_poll_state( js );
	somatic_hard_assert( status == 0, "Failed to poll joystick\n");

	int i;
	for( i = 0; i < JACH_NAXES; i++ )
		msg->axes->data[i] = js->state.axes[i];

	for( i = 0; i < JACH_NBUTTONS; i++ )
		msg->buttons->data[i] = (int64_t)js->state.buttons[i];
}

/* ---- */
/* MAIN */
/* ---- */
int main( int argc, char **argv ) {

  argp_parse (&argp, argc, argv, 0, NULL, NULL);

  // install signal handler
  somatic_sighandler_simple_install();

  // Open spacenav device
  int sn_r = spnav_open();
  js_t *js = js_open( opt_jsdev );
  somatic_hard_assert( js != NULL, "Failed to open joystick device\n");

  if (opt_create == 1)
	  jach_create_channel(opt_ach_chan, 10, 8); //8

  // Open the ach channel for the spacenav data
  ach_channel_t *chan = jach_open(opt_ach_chan);

  Somatic__Joystick js_msg;
  jach_allocate_msg(&js_msg, JACH_NAXES, JACH_NBUTTONS);

  if( opt_verbosity ) {
      fprintf(stderr, "\n* JSD *\n");
      fprintf(stderr, "Verbosity:    %d\n", opt_verbosity);
      fprintf(stderr, "jsdev:        %d\n", opt_jsdev);
      fprintf(stderr, "channel:      %s\n", opt_ach_chan);
      fprintf(stderr, "message size: %d\n", somatic__joystick__get_packed_size(&js_msg) );
      fprintf(stderr,"-------\n");
  }

  while (!somatic_sig_received) {
	  jach_read_to_msg(&js_msg, js);
	  jach_publish(&js_msg, chan);
	  if( opt_verbosity )
		  jach_print(&js_msg);
  }

  // Cleanup:
  jach_close(chan);
  sn_r = spnav_close();
  somatic_hard_assert( sn_r == 0, "Failed to close spacenav device\n");

  //TODO: cleanup memory and what not

  return 0;
}


