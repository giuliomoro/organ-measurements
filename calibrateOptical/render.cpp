#include <Bela.h>
#include <Utilities.h>
#include <cmath>
#include <rtdk.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <WriteFile.h>
#include <Scope.h>

extern int gShouldLog;
extern const int gFilenameSize = 100;
char gFilename[gFilenameSize]="calib.m";

// setup() is called once before the audio rendering starts.
// Use it to perform any initialisation and allocation which is dependent
// on the period size or sample rate.
//
// userData holds an opaque pointer to a data structure that was passed
// in from the call to initAudio().
//
// Return true on success; returning false halts the program.
float gReference = -1;
float gInc = 1;
bool gNewReference= false;
AuxiliaryTask readTask;
void read(){ //http://stackoverflow.com/questions/21197977/how-can-i-prevent-scanf-to-wait-forever-for-an-input-character
	fd_set readfds;
    struct timeval tv;
    int    fd_stdin;
	fd_stdin = fileno(stdin);
	printf("Reference value: \n");
//	system("stty cbreak");
	while (!gShouldStop){
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 10000;
		fflush(stdout);
		int num_readable = select(fd_stdin + 1, &readfds, NULL, NULL, &tv);
		if(num_readable > 0){
			float reference;
			char buf[100];
			fgets(buf, sizeof(buf), stdin);
			if(buf[0] == '\n'){
				gNewReference = true;
				gReference = gReference + gInc;
			}
			else {
				reference = atof(buf);
				if(reference >= 0){
					gInc = reference - gReference;
					gReference = reference;
					gNewReference = true;
				}
				else
					printf("Reference value.\n");
			}
			printf("Reference value: \n");
		}
		usleep(1000);
	}
//	system("stty icanon");
}

Scope scope;
WriteFile file1;
bool setup(BelaContext *context, void *userData)
{
    if(context->analogInChannels != 2){
    	printf("Wrong number of analog inputs. Run with '-C 2'\n");
    	return false;
    }

	if(gShouldLog)
	{
		int ret = access(gFilename, R_OK);

		if(ret == 0){
			fprintf(stdout, "output file '%s' exists. Abort...\n", gFilename);
			exit(1);
		}

		file1.init(gFilename); //set the file name to write to
		file1.setFileType(kText);
		file1.setHeader("calibration=[\n");
		file1.setFooter("];\n");
		file1.setEcho(true);
		file1.setFormat("%f, %f, %f\n");
	}
    readTask = Bela_createAuxiliaryTask(read, 40, "readTask");
    scope.setup(2, context->audioSampleRate);
    return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numAnalogFrames
// will be 0.

/* basic_blink
* Connect an LED in series with a 470ohm resistor between P8_07 and ground.
* The LED will blink every @interval seconds.
*/

int gNumCalibrationSamples = 10000;
int gSensorChannel[2] = {0, 1};
int gNumMeasurements = 0;
void render(BelaContext *context, void *userData)
{
	static bool init = false;
	if(init == false){
		Bela_scheduleAuxiliaryTask(readTask);
		init = true;
	}
	static int count = -1;
	static float sum[2] = {0};
	static float logs[4];
	static int bufCount = 0;
	if ((bufCount&127) == 0){
		rt_printf("analog in: %7.5f %7.5f\r", analogRead(context, 0, gSensorChannel[0]), 
		analogRead(context, 0, gSensorChannel[1]));
	}
	bufCount++;
	if(gNewReference == true){
		gNewReference = false;
		gNumMeasurements++;
		for( unsigned int n = 0; n < 2; ++n)
		    sum[n] = 0;
		count = gNumCalibrationSamples;
		logs[0] = gReference;
	}
	for(unsigned int n = 0; n < context->analogFrames; n++){
		if(count > 0)
			count--;
		for(unsigned int k = 0; k < 2; ++k){
            sum[k] += analogRead(context, n, gSensorChannel[k]);
		}
        scope.log(analogRead(context, n, gSensorChannel[0]),
            analogRead(context, n, gSensorChannel[1]));

	}
	if(count == 0){
		count = -1;
		logs[1] = sum[0] / gNumCalibrationSamples;
		logs[2] = context->audioFramesElapsed / context->audioSampleRate;
		file1.log(logs, 3);
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BelaContext *context, void *userData)
{
	// Nothing to do here
	printf("Calibration completed.\nIt took %.1f seconds for %d measurements.\n",
			context->audioFramesElapsed/context->audioSampleRate,
			gNumMeasurements);
}
