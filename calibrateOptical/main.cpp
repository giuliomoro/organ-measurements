/*
 * main.cpp
 *
 *  Created on: Oct 24, 2014
 *      Author: parallels
 */
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <libgen.h>
#include <signal.h>
#include <getopt.h>
#include <Bela.h>
#include <string.h>

using namespace std;

// Handle Ctrl-C by requesting that the audio rendering stop
void interrupt_handler(int var)
{
	gShouldStop = true;
}

// Print usage information
void usage(const char * processName)
{
	cerr << "Usage: " << processName << " [options]" << endl;

	Bela_usage();

	cerr << "   --nolog disables writing to file\n";
	cerr << "   --file the output filename\n";
}

extern int gFilenameSize;
extern char gFilename;
int gShouldLog = 1;

int main(int argc, char *argv[])
{
	BelaInitSettings settings;	// Standard audio settings
	float frequency = 440.0;	// Frequency of oscillator

	struct option customOptions[] =
	{
		{"filename", 1, NULL, '\254'},
		{"nolog", 0, NULL, '\253'},
		{NULL, 0, NULL, 0}
	};

	// Set default settings
	Bela_defaultSettings(&settings);
	// Parse command-line arguments
	while (1) {
		int c;
		if ((c = Bela_getopt_long(argc, argv, "i:l:\254\253", customOptions, &settings)) < 0)
				break;
		switch (c) {
		case '\253':
			gShouldLog = 0;
			break;
		case '\254':
			strncpy(&gFilename, optarg, gFilenameSize);
			break;
		default:
			usage(basename(argv[0]));
			exit(1);
		}
	}

	// Initialise the PRU audio device
	if(Bela_initAudio(&settings, &frequency) != 0) {
		cout << "Error: unable to initialise audio" << endl;
		return -1;
	}

	// Start the audio device running
	if(Bela_startAudio()) {
		cout << "Error: unable to start real-time audio" << endl;
		return -1;
	}

	// Set up interrupt handler to catch Control-C and SIGTERM
	signal(SIGINT, interrupt_handler);
	signal(SIGTERM, interrupt_handler);

	// Run until told to stop
	while(!gShouldStop) {
		usleep(100000);
	}

	// Stop the audio device
	Bela_stopAudio();

	// Clean up any resources allocated for audio
	Bela_cleanupAudio();

	// All done!
	return 0;
}
