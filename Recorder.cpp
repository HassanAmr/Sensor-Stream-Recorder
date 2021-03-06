//#include <iostream> // for standard I/O
//#include <sstream>      // std::stringstream, std::stringbuf
//#include <string>   // for strings
#include <time.h>
#include <thread>
#include <fstream>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdexcept>  

//#include <opencv2/core/core.hpp>        // Basic OpenCV structures (cv::Mat)
//#include <opencv2/highgui/highgui.hpp>  // Video write
#include "opencv2/opencv.hpp"

using namespace std;
using namespace cv;

void Help();
int CreateDirectories(String, String, String);//this function will create the necessary folders for the recorder

void SensorStream();
void Timer();

void WriteSRT(int lineCounter, String data);
String FromMillisecondsToSRTFormat(unsigned long val);

void WriteLittleEndian(unsigned int word, int num_bytes);
void WriteWavHeader(const char * filename, int s_rate, int no_input);
unsigned int FloatToBits(float x);

//Globals
FILE * srtFile;
//FILE * trainingFile;//TODO: Delete later
FILE * audioFile;

bool streamComing = false;
bool recordingVideo = false;//to help know when the sensor starts sending stream data
bool recordingStream = false;//to help the SensorStream thread know when to write the 
bool recording = false;//to help the SensorStream thread know when to write the 
bool terminating = false;//to terminate the timer thread when program exits adequately
bool streaming = true;//to terminate the SensorStream thread when program exits adequately
bool startWriting = false;//this is to align the stream with the recordingx

int sensorVectorSize;//the number of expected inputs per 1 feed from the sensor
int samplingFrequency;//the frequency of which a set of sensor feed is sent

int audioValuesWritten;

double lastReceivedVal;//because I'm stupid, and didn't compile my opencv with c++11, I have to create this variable, and hope that a race condition will not occur

struct timeval tv;

bool startedRecording = false;//to recognize a start recording event
bool stoppedRecording = false;//to recognize a stop recording event

unsigned long long startTime;
unsigned long timeNow;

int main(int argc, const char * argv[]){

  Help();

  String recorderName = "";

  if (argc > 3)
  {
    recorderName = argv[1];
    //recorderObject = argv[2];
    sensorVectorSize = atoi(argv[2]);//TODO: handle error later
    samplingFrequency = atof(argv[3]);//TODO: handle error later
    if (argc > 4)
    {
      printf("Too much arguments. Please enter the name of the recorder, then a string indicating what is being recorded, and finally the sensor data vector size (3 arguments)");
      return 1;
    }

  }
  else
  {
    printf("Too few arguments. Please enter the name of the recorder, then a string indicating what is being recorded, and finally the sensor data vector size (3 arguments)");
    return 1;
  }

  //-------------------------------------- 
  time_t session;
  char buffer [80];
  struct tm * timeinfo;
  time (&session);
  timeinfo = localtime (&session);
  //%d%m%H%M%S
  strftime (buffer,80,"%d%m%H%M%S",timeinfo);

  String sessionName = String(buffer);
  
  String folderName = "/Workspace/Recorder/Videos";
  char *home = getenv ("HOME");
  
  String str(home);
  folderName = str + folderName;
  
  
  int status = CreateDirectories(folderName, recorderName, sessionName);

  if (status == 1)
  {
    printf("The was an error creating the necessary folders for the recorder, the program will now exit.\n");
    return 1;
  }

  int fps = 10;

  int frameCounter = 0;

  double timeIntegral_FPS = 1000/fps;

  VideoCapture vcap(0); 
  if(!vcap.isOpened()){
    cout << "Error opening video stream or file" << endl;
    return -1;
  }
  
  vcap.set(CV_CAP_PROP_CONVERT_RGB , false);
  //vcap.set(CV_CAP_PROP_FPS , fps);
  
  //allocate memory and start threads here after passing all cases in which program might exit
  
  int frame_width=   vcap.get(CV_CAP_PROP_FRAME_WIDTH);
  int frame_height=   vcap.get(CV_CAP_PROP_FRAME_HEIGHT);
    
  ifstream infile("Gestures");

  string gestureString;

  thread ss (SensorStream);
  thread tr (Timer);

  ss.detach();
  tr.detach();

  
  int recordingsCounter = 1;

  unsigned long timeDiff;

  //SRT related variables
  bool readyForGesture = false; //this will be used to indicate whether the recorder is ready to record a gesture or not.
  bool writingSRT = false;
  int srtLineCounter = 1;
  String srtData = "";
  VideoWriter * video;
  
  for(;;){
  
  	
      
    if (startedRecording)
    {
      //TODO: make the files naming also suitable for windows
      String vidFileName =      folderName + "/" + recorderName + "/" + sessionName + "/" + std::to_string(recordingsCounter) + ".avi";
      String srtFileName =      folderName + "/" + recorderName + "/" + sessionName + "/" + std::to_string(recordingsCounter) + ".srt";
      String audioFileName =      folderName + "/" + recorderName + "/" + sessionName + "/" + std::to_string(recordingsCounter) + ".wav";
      //String trainingFileName = "Output/" + recorderName + "/" + sessionName + "/Training/" + std::to_string(recordingsCounter);//TODO: Delete later
      srtFile = fopen (srtFileName.c_str(),"w");
      //trainingFile = fopen (trainingFileName.c_str()  ,"w");
      WriteWavHeader(audioFileName.c_str(), samplingFrequency, sensorVectorSize);
      video = new VideoWriter(vidFileName,CV_FOURCC('M','J','P','G'),fps, Size(frame_width,frame_height),true);  

      
      startedRecording = false;
    }

    if (stoppedRecording)
    {
      fclose (srtFile);
      //fclose (trainingFile);
      fclose (audioFile);
      srtLineCounter = 1;
      recordingsCounter++;
      stoppedRecording = false;
    }

    Mat frame;
    vcap >> frame;

      
    if (startWriting)
    {
      timeDiff = timeNow - startTime;
      String disp1 = "Timer: " + FromMillisecondsToSRTFormat(timeDiff);     
      putText(frame, disp1, Point(20, 40), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 255, 255), 1);
      if (frameCounter * timeIntegral_FPS < timeDiff)     
      {
        video->write(frame); //Record the video till here. It is not needed to have a recording indicator in the recoring output
        frameCounter++;
      }
      //Recording Indicator
      circle(frame, Point(26, 16), 8, Scalar(0, 0, 255), -1, 8, 0);
      putText(frame, "RECORDING", Point(40, 20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 1);
    }
    if(streamComing)
    {
      circle(frame, Point(426, 16), 8, Scalar(255, 0, 0), -1, 8, 0);
      putText(frame, "SENSOR STREAMING", Point(440, 20), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(255, 0, 0), 1);
    }
    imshow( "Recorder", frame );
    
    char c = (char)waitKey(33);
    if( c == 27 ) //Escape button pressed
    {
      if (recording)
      {
        //fclose (srtFile);
        //fclose (trainingFile);
        //fclose(audioFile);
        //recording = false;
        printf("Please end the recording session first.\n");
      }
      else
      {
        terminating = true;
        streaming = false;
        break;
      }      
    }
    else if (c == 'r')
    {
      if (recording)
      {
        //TODO: make a loop that will block untill our audio file reaches the time at which we decided to end the recording
        //take time, and get only the seconds part

        unsigned long endTime;
        struct timeval now;
        gettimeofday(&now, NULL);
        endTime = (unsigned long long)(now.tv_sec) * 1000 + (unsigned long long)(now.tv_usec) / 1000;
        endTime -= startTime;
        endTime /= 1000;
        printf("Please wait while the stream catches up with the video...\n");
        int num_channels_sampling_frequency = sensorVectorSize*samplingFrequency;
        while (audioValuesWritten/(num_channels_sampling_frequency) <= endTime){}
        stoppedRecording = true;
        delete video;
        printf("Stopped recording!\n");
      }
      else
      {
       startedRecording = true;
       printf("Started recording!\n");
      }
      recording = !recording;
    }
    else if (c == 'n')
    {
      //TODO: if writingSRT == true
      if (recording && !writingSRT)
      {
        if(getline(infile, gestureString))
        {
          printf("Recording gesture %s. Press s when ready to record the gesture, and then s again when finished.\n", gestureString.c_str());
          readyForGesture = true;
        }
        else
        {
          printf("No more gestures to record, you can now end the recording session.\n");
        }
      }
      else
      {
        printf("Please start a recording session before trying to record a gestrue.\n");
      }
      
    }
    else if(c == 's')
    {
      if (readyForGesture && !writingSRT)
      {
        //record the time at this instance for the srt file, and write it there
        srtData += FromMillisecondsToSRTFormat(timeNow - startTime);
        srtData += " --> ";
        writingSRT = true;
        readyForGesture = false;
        printf("In\n");
      }
      else if (!readyForGesture && writingSRT)
      {
        srtData += FromMillisecondsToSRTFormat(timeNow - startTime);
        srtData += "\n";
        srtData += gestureString + "\n";
        WriteSRT(srtLineCounter, srtData);
        srtData = "";
        srtLineCounter++;
        writingSRT = false;
        readyForGesture = true;
        printf("Out\n");
      }
    }
  }
  return 0;

}

void SensorStream()
{
 //double sensorData[sensorVectorSize];

  char buffer[256];
  float val;
  bool activateStream = true;   //to activate the stream indicator only once, and avoid data races.
  //Data specific to write the srt file
  //int srtLineCounter = 1;
  //String srtData = "";
  //String srtTime = "";
  while (streaming)
  {
    for(int i = 0; i < sensorVectorSize; i++)
    {
      scanf("%s", buffer);
      if(activateStream)
      {
        streamComing = true;
        activateStream = false;  
      }
      //cout <<buffer<<endl; 
      try
      {
        val = stof(buffer);
        
        if(recording)
        {
          if (startWriting)
          {
            WriteLittleEndian(FloatToBits(val), 4);
            audioValuesWritten++;
          }
          else if (i == sensorVectorSize - 1)//this ensures that if the recording started in the middle of a vector, that the stream will be aligned with the recording
          {
            gettimeofday(&tv, NULL);
            startTime = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
            startWriting = true;
            audioValuesWritten = 0;
            //cout<<"started..."<<endl;
          }
        }
        else
        {
          //srtLineCounter = 1;
          startWriting = false;
        }
      }
      catch (const std::invalid_argument& ia)
      {
        printf("Sensor has finshed sending data!\n Please restart the Recorder.\n");
        //printf("Sensor has finshed sending data!\n If you are planning on restarting the sensor feed,"
          //      " please make sure to press the 's' key in advance, otherwise the feed will not be synchronized.\n");
        streaming = false;
      }
    }     
  } 
}

void Timer()
{

  struct timeval now;
  while(!terminating)//TODO, check if there is a better practice
  {
    gettimeofday(&now, NULL);
    timeNow = (unsigned long long)(now.tv_sec) * 1000 + (unsigned long long)(now.tv_usec) / 1000;
  }
  
}

void WriteSRT(int lineCounter, String data)
{
  fprintf (srtFile, "%d\n", lineCounter);
  fprintf (srtFile, "%s\n", data.c_str());
}

String FromMillisecondsToSRTFormat(unsigned long val)
{
  int milliseconds = val/1000;
  milliseconds = val - milliseconds*1000;
  int seconds = (int) (val / 1000) % 60 ;
  int minutes = (int) ((val / (1000*60)) % 60);
  int hours   = (int) ((val / (1000*60*60)) % 24);
  char str[20];
  sprintf(str, "%02d:%02d:%02d,%03d", hours, minutes, seconds, milliseconds);
  //ss<< setfill('0') << setw(2) <<hours<<":"<< setfill('0') << setw(2) <<minutes<<":"<< setfill('0') << setw(2) <<seconds<<","<< milliseconds<<endl;
  return str;

}

unsigned int FloatToBits(float x)
{
    unsigned int y;
    memcpy(&y, &x, 4);
    return y;
}

void WriteLittleEndian(unsigned int word, int num_bytes)
{
  unsigned buf;
  while(num_bytes>0)
  {   
    buf = word & 0xff;
    fwrite(&buf, 1,1, audioFile);
    num_bytes--;
    word >>= 8;
  }
}

void WriteWavHeader(const char * filename, int s_rate, int no_input)
{
  unsigned int sample_rate;
  unsigned int num_channels;
  unsigned int bytes_per_sample;
  unsigned int byte_rate;
 
  num_channels = no_input;//1   /* monoaural */
  bytes_per_sample = 4;
 
  if (s_rate<=0) sample_rate = 44100;
  else sample_rate = (unsigned int) s_rate;
 
  byte_rate = sample_rate*num_channels*bytes_per_sample;
    //byte_rate = 0;
 
  audioFile = fopen(filename, "w");
  assert(audioFile);   /* make sure it opened */
 
  /* write RIFF header */
  fwrite("RIFF", 1, 4, audioFile);
  //WriteLittleEndian(36 + bytes_per_sample* num_samples*num_channels, 4);
  WriteLittleEndian(0, 4);
  fwrite("WAVE", 1, 4, audioFile);
 
  /* write fmt  subchunk */
  fwrite("fmt ", 1, 4, audioFile);
  WriteLittleEndian(16, 4);   /* SubChunk1Size is 16 */
  WriteLittleEndian(3, 2);    /* PCM is format 1. IEEE float is format 3*/
  WriteLittleEndian(num_channels, 2);
  WriteLittleEndian(sample_rate, 4);
  WriteLittleEndian(byte_rate, 4);
  WriteLittleEndian(num_channels*bytes_per_sample, 2);  /* block align */
  WriteLittleEndian(8*bytes_per_sample, 2);  /* bits/sample */
 
  /* write data subchunk */
  fwrite("data", 1, 4, audioFile);
  //WriteLittleEndian(bytes_per_sample* num_samples*num_channels, 4); 
  WriteLittleEndian(0, 4); 

}

void Help()
{
    cout << "\nThis program helps you train your sensor data by piping the sensor stream directly into this program.\n\n"
            "Please make sure that the Recorder is active before starting the sensor stream feed, otherwise the feed will not be synchronized.\n\n"
            "Usage:\n"
            "\t./Recorder [recorder_name] [expected_#_inputs] [sampling_frequency]\n" << endl;
            //"./Recorder [recorder_name] [symbol_to_train] [expected_#_inputs_from_sensor]\n" << endl;

    cout << "Control keys (Recorder window must be active for these controls to work):\n"
            "\tESC - quit the program\n"
            "\tr - start, or stop a recording\n"
            "\tn - get ready for the next item in the list found in the file called `gesture`\n"
            "\ts - start/end annotating the recording with the current gesture pointed to by using the key `n`\n"
            //"\ts - start stream again, in case the sensor was interrupted.\n"
            << endl;

    cout << "Example:\n"
            "\tunbuffer python SerialPort.py | ./Recorder Hassan 9 20"
            << endl;
}

int CreateDirectories(String folderName, String recorderName, String sessionName)//TODO: make it also suitable for windows
{
  int returnValue = 0; 
  
  printf("%s\n", folderName.c_str());
  
  int status = mkdir(folderName.c_str(), 0777);
  if ( status == 0)
  {
    printf("New folder 'Videos' created in the following location:\n%s\n", folderName.c_str());
  }
  else if (errno == EEXIST)
  {
    //printf("");//Here we don't print anything, because it is going to be distrubing whenever we get notfied about the Output folder being already there.
  }
  else
  {
    printf("Couldn't create folder the 'Videos' folder\n");
    returnValue = 1;
  }

  String directoryName = folderName + "/" + recorderName;

  status = mkdir(directoryName.c_str(), 0777);
  if ( status == 0)
  {
    printf("A new folder created for the recorder\n");
  }
  else if (errno == EEXIST)
  {
    //printf("");//Here we don't print anything, because it is going to be distrubing whenever we get notfied about the Output folder being already there.
  }
  else
  {
    printf("Couldn't create folder for the recorder\n");
    returnValue = 1;
  }
  
  directoryName = directoryName + "/" + sessionName;

  status = mkdir(directoryName.c_str(), 0777);
  if ( status == 0)
  {
    printf("A new folder created for this session\n");
  }
  else if (errno == EEXIST)
  {
    //printf("");//Here we don't print anything, because it is going to be distrubing whenever we get notfied about the Output folder being already there.
  }
  else
  {
    printf("Couldn't create folder for the new session\n");
    returnValue = 1;
  }

  printf("\n");
  return returnValue;

}