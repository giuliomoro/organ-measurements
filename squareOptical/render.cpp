#include <Bela.h>
#include <Utilities.h>
#include <cmath>
#include <rtdk.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <Scope.h>
#include <WriteFile.h>

extern int gShouldLog;

float gReference = 0.34;
float low = 0.26;
AuxiliaryTask readTask;

void read(){ //http://stackoverflow.com/questions/21197977/how-can-i-prevent-scanf-to-wait-forever-for-an-input-character
	fd_set readfds;
	struct timeval tv;
	int fd_stdin;
	fd_stdin = fileno(stdin);
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
			reference = atof(buf);
			if(reference >= 0){
				gReference = reference;
			}
			printf("Reference value: %.f\n", gReference);
		}
		usleep(1000);
	}
}

Scope scope;
WriteFile file;
bool setup(BelaContext *context, void *userData)
{
    if(gShouldLog)
    {
        file.init("out.bin");
        file.setFileType(kBinary);
    }

    if(context->analogInChannels != 2){
    	printf("Wrong number of analog inputs. Run with '-C 2'\n");
    	return false;
    }

    readTask = Bela_createAuxiliaryTask(read, 40, "readTask");
    scope.setup(3, context->audioSampleRate);
    return true;
}

int gNumCalibrationSamples = 10000;
int gSensorChannel[2] = {0, 1};
int gNumMeasurements = 0;
void render(BelaContext *context, void *userData)
{
	float logs[3];
	float high = gReference;
	for(unsigned int n = 0; n < context->analogFrames; n++){
		float in = analogRead(context, n, 0);
		float loopback = analogRead(context, n, 1);
		static float phase = 0;
		phase += 10.f / context->analogSampleRate;
		if(phase > 1)
			phase -= 1;
		bool state = phase > 0.5;
		float out = state ? high : low;
		analogWrite(context, n, 0, out);
		logs[0] = state * 0.8f + 0.1f;
		logs[1] = in;
		logs[2] = loopback;
		scope.log(logs);
		if(gShouldLog)
			file.log(logs, 3);
		static int count = 0;
		if((count & 4095) == 0)
			rt_printf("analog in: %7.5f \r", in);
		++count;
	}
}

void cleanup(BelaContext *context, void *userData)
{
}
