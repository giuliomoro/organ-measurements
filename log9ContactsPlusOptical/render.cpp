#include <Bela.h>
#include <Utilities.h>
#include <cmath>
#include <rtdk.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <WriteFile.h>
#include <native/task.h>

// AuxiliaryTask readTask;
float gVelocityThresholdLow = 0.18;
float gVelocityThresholdHigh = 0.4;
bool gShouldLog = true;
/*
void read(){ //http://stackoverflow.com/questions/21197977/how-can-i-prevent-scanf-to-wait-forever-for-an-input-character
	fd_set readfds;
    struct timeval tv;
    int    fd_stdin;
	fd_stdin = fileno(stdin);
//	system("stty cbreak");
	while (!gShouldStop){
		FD_ZERO(&readfds);
		FD_SET(fileno(stdin), &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 1000;
		fflush(stdout);
		int num_readable = select(fd_stdin + 1, &readfds, NULL, NULL, &tv);
		if(num_readable > 0){
			int channel;
			scanf("%i", &channel);
//			printf("channel: %d", channel);
//			gToggle = !gToggle;
//			if(channel >= 0 && channel < 8)
//				gInputChannel = channel;
//			else
//				printf("Wrong channel number.\n");
//			printf("Current input channel: %d\nEnter new input channel: ", gInputChannel);
		}
		usleep(1000);
	}
//	system("stty icanon");
}
*/

WriteFile file1;
bool setup(BelaContext *context, void *userData)
{
	if(gShouldLog){
	    char filename[] ="out.bin";
        int ret = access(filename, R_OK);
        if(ret == 0){
                fprintf(stdout, "output file '%s' exists. Abort...\n", filename);
                exit(1);
        }
		file1.init(filename); //set the file name to write to
		file1.setFileType(kBinary);
	}
	for(int n = 0; n < 15; n++){
		pinMode(context, 0, n, INPUT);
	}
    // readTask = Bela_createAuxiliaryTask(read, 40, "readTask");
    // Bela_scheduleAuxiliaryTask(readTask);
	if(context->analogInChannels != 2){
		printf("Wrong number of analog inputs. Run with '-C 2'\n");
		return false;
	}
    return true;
}

// render() is called regularly at the highest priority by the audio engine.
// Input and output are given from the audio hardware and the other
// ADCs and DACs (if available). If only audio is available, numAnalogFrames
// will be 0.

enum {
	kNone,
	kAttack,
	kSustain,
};

void render(BelaContext *context, void *userData)
{
	static bool init = false;

	static int analogKeyPresses = 0;
	static  int count = 0;
	static float logs[11] = {0};
	const int numDigitalIns = 9;
	static int digitalIns[numDigitalIns] = {0, 1, 2, 3, 4, 5, 8, 9, 11};
	static int stateCount = 0;
	static float lastOnsetVelocity = 0;
	static int analogState = kNone;
	static int attackStart = 0;
	static float lastDuration = 0;
	static int lastLog = 0;
	static int logEvery = context->audioSampleRate * 0.1f;
	if(context->audioFramesElapsed - lastLog > logEvery){
		lastLog = context->audioFramesElapsed;
		//printf("%10d\n", lastLog);
		//rt_printf("\r");
		rt_printf("\e[1;1H\e[2J");
		for(int  n = 0; n<numDigitalIns; n++){
			rt_printf("%d", (int)digitalRead(context,0,digitalIns[n]));
		}
		rt_printf(" a0: %5.3f, a1: %5.3f, keyPresses: %3d, vel: %3.0f, dur: %2.2fs, buf: %.3f%% ",
				context->analogIn[0], context->analogIn[1], analogKeyPresses, lastOnsetVelocity, lastDuration, 100.f * file1.getBufferStatus());
		if(lastDuration < 1.95)
			rt_printf("HOLDHOLDHOLDHOLDHOLDHOLD");
		else
			rt_printf("                        ");
//		rt_printf("keyPressed: %d, keyPresses: %d, stateCount: %d\n",keyPressed, keyPresses, stateCount);
		rt_printf("\n");
	}
	count++;
	/*again, we are assuming -C 2 , so we average the two analog frames
	 * to reduce noise and data size and write the output at 44.1kHz
	*/
	int minCount = 2000;
	const int numF0s = 6;
	float f0s[numF0s] = {130.81, 155.56, /*174.61,*/ 185.00, 196.00, /*220.00*,*/ 233.08, /*246.94,*/ 261.63};
	static float f0 = f0s[numF0s-1];
	for(unsigned int n = 0; n < context->digitalFrames; n++){
		float sum = 0;
		for(int k = 0; k < numDigitalIns; k++){
			logs[k]=digitalRead(context, n, digitalIns[k]);
			sum += logs[k];
		}

		//averaging for the optical sensor, assuming -C2
		float opticalSensor = (analogRead(context, 2*n, 0) + analogRead(context, 2*n+1, 0))*0.5f;
		logs[9] = opticalSensor;
		logs[10] = analogRead(context, 2*n+1, 1);
		if(gShouldLog)
		{
			if(file1.getBufferStatus() > 0.02)
			{
				file1.log(logs, 11);
			}
			else
			{
				rt_printf("Hiccup\n");
				rt_task_sleep(100000000);
			}
		}
		if(opticalSensor < gVelocityThresholdHigh && analogState == kNone){
			analogState = kAttack;
//			rt_printf("\nattack\n");
			attackStart = context->audioFramesElapsed + n;
		}
		if(opticalSensor < gVelocityThresholdLow && analogState == kAttack){
			analogState = kSustain;
			int attackEnd = context->audioFramesElapsed + n;
			int diff = attackStart - attackEnd;
//			rt_printf("\nstart: %d, end: %d, diff: %d\n", attackStart, attackEnd, diff);
			lastOnsetVelocity = (gVelocityThresholdHigh-gVelocityThresholdLow)/(float)(attackEnd - attackStart) * 65000;
			analogKeyPresses++;
		}
		if(opticalSensor > gVelocityThresholdHigh*1.05 && analogState == kSustain){ //*1.05 serves as histeresis
			analogState = kNone;
		}
		if(analogState != kNone)
			lastDuration = (context->audioFramesElapsed - attackStart)/context->audioSampleRate;
	}

	//synthesis
	float b[3] = {0.131106439916626, 0.262212879833252, 0.131106439916626};
	float a[3] = {1, -0.747789178258503, 0.272214937925007};
//	float b[3] = {0.5, 0.5, 0};
//	float a[3] = {1, 0, 0};
	static float pxl = 0;
	static float ppxl = 0;
	static float pyl = 0;
	static float ppyl = 0;
	static float pxr = 0;
	static float ppxr = 0;
	static float pyr = 0;
	static float ppyr = 0;
	static float phases[numDigitalIns] = {0};
	static float ratios[numDigitalIns] = {.5, 1.5, 1, 2, 3, 4, 5, 6, 7};
	static float gains[numDigitalIns] = {1, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.016, 0.08, 0.04};
//	static float pans[numDigitalIns] = {0.5, 0.6, 0.4, 0.7, 0.3, 0.8, 0.2, 0.9, 0.1};
	static float pans[numDigitalIns] = {0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5};
	float T = 1/context->audioSampleRate;
	for(unsigned int n = 0; n < context->audioFrames; n++){
		float xl = 0;
		float xr = 0;
		for(unsigned int m = 0; m < numDigitalIns; m++){
			phases[m] += 2 * M_PI * f0 * ratios[m] * T;
			if(phases[m] >= 2 * M_PI){
				phases[m] -= 2 * M_PI;
			}
			float value = gains[m] * !digitalRead(context, n, digitalIns[m]) * sinf(phases[m]);
			xl += pans[m] * value;
			xr += (1 - pans[m]) * value;
		}
		xl /= 5;
		xr /= 5;
		float yl = xl*b[0] + pxl*b[1] + ppxl*b[2] - pyl*a[1] - ppyl*a[2];
		float yr = xr*b[0] + pxr*b[1] + ppxr*b[2] - pyr*a[1] - ppyr*a[2];
		ppxl = pxl;
		pxl = xl;
		ppyl = pyl;
		pyl = yl;
		ppxr = pxr;
		pxr = xr;
		ppyr = pyr;
		pyr = yr;
		audioWrite(context, n, 0, yl);
		audioWrite(context, n, 1, yr);
	}
}

// cleanup() is called once at the end, after the audio has stopped.
// Release any resources that were allocated in setup().

void cleanup(BelaContext *context, void *userData)
{
	float seconds = context->audioFramesElapsed/context->audioSampleRate;
	printf("Acquisition completed.\nIt took %.1f seconds (%f MB).\n",
			seconds, context->audioFramesElapsed * sizeof(float)*10/1024.f/1024.f);
	printf("\n");
	// Nothing to do here
}
