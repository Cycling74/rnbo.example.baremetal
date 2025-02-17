//
//  main.cpp
//  baremetal
//
//  Created by Stefan Brunner on 15.03.24.
//


//#define RNBO_NOSTL          // do not use any C++ standard library objects and functionality
//#define RNBO_USE_FLOAT32    // use 32 bit floats instead of 64 bit double values (degrades audio performance a little)
#define RNBO_NOTHROW        // no exceptopns

//#define RNBO_USECUSTOMPLATFORM    // completely replace the RNBO platform methods wit your own

#define RNBO_USECUSTOMPLATFORMPRINT // only replace the print out methods
#define RNBO_USECUSTOMALLOCATOR     // use a custom memory allocator

#define RNBO_FIXEDLISTSIZE 64       // when you use lists and want to avoid allocations, define this
                                    // to set the maximum allowed list length

//#define RNBO_MINENGINEQUEUESIZE 256 // the built in minimal engine works with a fixed queue size for
                                    // scheduled events, it defaults to 128 and you can set it via this define


#define SAMPLERATE  44100                   // arbitrary sample rate chosen for this demo
#define VECTORSIZE  64                      // arbitrary vector size chosen for this demo
#define OUTPUTBUFFERSIZE 10 * SAMPLERATE    // record max 10 secs for this demo

#include "tlsf.h"   // a nice and simple pool allocator https://github.com/mattconte/tlsf
#include <iostream> // only used for output in this example, otherwise not needed

tlsf_t myPool;      // the memory pool

// we are defining our custom memory allocation functions BEFORE we include the RNBO header
// so we have them available all over the place

namespace RNBO {
	namespace Platform {

		void *malloc(size_t size) {
			return tlsf_malloc(myPool, size);
		}

		void  free(void * ptr)
		{
			tlsf_free(myPool, ptr);
		}

		void *realloc(void * ptr, size_t size) {
			return tlsf_realloc(myPool, ptr, size);
		}

		void *calloc(size_t count, size_t size);
	}
}

// you can include the rnbo source header directly

// if you do not want to add the include dir in your poject/cmake/makefile, you can uncomment the following line to set it
//#define RNBO_LIB_PREFIX common

#include "export/rnbo_source.h"

// more custom platform definitions

namespace RNBO {
	namespace Platform {
		void *calloc(size_t count, size_t size) {
			auto mem = malloc(count * size);
			memset(mem, 0, count * size);
			return mem;
		}

        /**
         * @brief Print a message to stdout or some kind of log
         */
        static void printMessage(const char* message) {
            printf("%s\n", message);
        }

        /**
         * @brief Prints an error message
         *
         * Defaults to printMessage.
         */
        static void printErrorMessage(const char* message) {
            printMessage(message);
        }
	}
}

// a handy C++ audio file reader/writer/encode/decode https://github.com/adamstark/AudioFile
// only used for the sake of this demo
#include "AudioFile.h"

// the minimal export provides a built in "Engine", with very minimal capabilities - it is mainly used
// to schedule clock events

// if you want to get any MIDI notifications out of your RNBO patcher you will have to provide your own
// engine call that overloads the MIDI out handlers, same for messages etc.
class MyEngine : public RNBO::MinimalEngine<>
{
public:
    MyEngine(RNBO::PatcherInterface* patcher)
    : RNBO::MinimalEngine<>(patcher)
    {}

    void sendMidiEvent(int port, int b1, int b2, int b3, RNBO::MillisecondTime time = 0.0) override {
        std::cout << "MIDI Event received: " << b1 << " " << b2 << " " << b3 << std::endl;
    }

    void sendListMessage(RNBO::MessageTag tag, RNBO::MessageTag objectId, const RNBO::list& payload, RNBO::MillisecondTime time) override {
        std::cout << "Message List Event received: " << tag << " " << objectId << " " ;
        for (auto i = 0; i < payload.length; i++) {
            std::cout << payload[i] << " ";
        }
        std::cout << std::endl;
    }

    // a really simple and stupid way to set some data on your engine instance
    // for example for referrring back to the owner object ?

    //    void setData(void *someData) {
    //        _data = someData;
    //    }
    //
    //private:
    //    void * _data;

};

// a simple helper function to copy sample output to an audio file for this demo
void copyToOoutput(RNBO::SampleValue** outs, AudioFile<RNBO::SampleValue>& outputFile, size_t size, size_t& samplesWritten) {
    for (size_t i = 0; i < size && samplesWritten < OUTPUTBUFFERSIZE; i++) {
        for (size_t channel = 0; channel < outputFile.getNumChannels(); channel++) {
            outputFile.samples[channel][samplesWritten] = outs[channel][i];
        }
        samplesWritten++;
    }
}

int main(int argc, const char * argv[])
{
	// a memory pool based custom allocator- in no way necessary on this Desktop machine
	// but quite handy for embedded systems
    const size_t poolSize = 100 * 1024*1024;
    void* mem = ::malloc(poolSize);
    myPool = tlsf_create_with_pool(mem, poolSize);

	/* some notes regarding HEAP and allocations: what do we allocate on the HEAP:

	 - sample buffers (data and buffer~) - allocated in initialize, unless you resize them
	 - lists (ONLY if their length exceeds the RNBO_FIXEDLISTSIZE)
	 - signal buffers (you can avoid that by exporting with a fixed audio vector size), but these will all be allocated when you
       you call prepareToProcess, no allocations later on
	 */

	RNBO::rnbomatic<MyEngine> rnbo;
	rnbo.initialize();	// after initialize all sample buffers are allocated

    // really simple and stupid way to set some user data on your engine for example for callbacks
//    static_cast<MyEngine*>(rnbo.getEngine())->setData(nullptr);

    const auto numInputs = rnbo.getNumInputChannels();
    const auto numOutputs = rnbo.getNumOutputChannels();

    std::vector<std::vector<RNBO::SampleValue>> inChannels(numInputs);
    for (auto& c : inChannels) c.resize(VECTORSIZE);
    std::vector<std::vector<RNBO::SampleValue>> outChannels(numOutputs);
    for (auto& c : outChannels) c.resize(VECTORSIZE);

    std::vector<RNBO::SampleValue*> ins;
    for (auto& c : inChannels) ins.push_back(c.data());
    std::vector<RNBO::SampleValue*> outs;
    for (auto& c : outChannels) outs.push_back(c.data());

    // this could allocate if:
    // - you did not export with a fixed sample block size
    // - some (rare) objects might allocate buffers based on the sample rate
    //   if you must avoid this, you will need to check for your specific patcher
    rnbo.prepareToProcess(SAMPLERATE, VECTORSIZE, true);

	// from here on out there should be no allocations, unless you exceed the fixed list size,
	// resize audio buffers or change samplerate or vector size by calling prepareToProcess

    // let us see if we have any parameters:
    const auto numParams = rnbo.getNumParameters();
    for (RNBO::ParameterIndex i = 0; i < numParams; i++) {
        RNBO::ParameterInfo info;
        rnbo.getParameterInfo(i, &info);
        std::cout << "Param " << i << ": " << rnbo.getParameterName(i) << " min: " << info.min << " max: " << info.max << std::endl;
    }

    // buffer and data handling
    // internally in RNBO all data is an untyped char* pointer
    // the generated code itself decides how to view it (for example as a Float64AudioBufferView
    // this untyped data is called a DataRef
    const auto numDataRefs = rnbo.getNumDataRefs();
    for (RNBO::DataRefIndex i = 0; i < numDataRefs; i++) {
        auto ref = rnbo.getDataRef(i);
        std::cout << "DataRef " << i << ": " << ref->getName()
            << " internal: " << (ref->isInternal() ? "yes " : "no ")
            << ref->getType().type << std::endl;
    }

    // here a theoretical example of setting a 32bit audio buffer
    if ((0)) {
        const float mySuperSample[SAMPLERATE * 2] = {}; // an interleaved 2 channel audio buffer of 1 second length

        // this will establish the data ref to be a 32 bit audio buffer - BUT be aware that you CANNOT change the bitness
        // of the audio buffer, so you cannot go from 32 bit to 64 bit or back, but cou can change length and/or
        // channel count compared to the originally generated one
        const RNBO::Float32AudioBuffer newType(2, SAMPLERATE);

        // get the data ref, you want to set the audio data on
        auto ref = rnbo.getDataRef(0);

        // all data is untyped (char *) data
        ref->setData((char *) mySuperSample, SAMPLERATE * 2 * sizeof(float));

        // update the type, this is important if you changed samplerate or channel count
        ref->setType(newType);

        // update all the data views in to patcher (one data ref can have multiple views on it)
        rnbo.processDataViewUpdate(0, RNBO::TimeNow);
    }

    // prepare a file to write to
	AudioFile<RNBO::SampleValue> outputFile;
	outputFile.setAudioBufferSize((int)numOutputs, OUTPUTBUFFERSIZE);
	outputFile.setBitDepth(16);
	outputFile.setSampleRate(SAMPLERATE);
    size_t samplesWritten = 0;

    // process one buffer, this will already set intial values etc.
    // processing will also advance the internal millisecond time (and beattime if transport is started)
    rnbo.process(ins.data(), numInputs, outs.data(), numOutputs, VECTORSIZE);
    copyToOoutput(outs.data(), outputFile, VECTORSIZE, samplesWritten);

    const auto currentTime = rnbo.getEngine()->getCurrentTime();
    std::cout << "current time: " << currentTime << std::endl;

    if (numParams > 0) {
        // set the first parameter value to something
        RNBO::ParameterInfo info;
        rnbo.getParameterInfo(0, &info);
        rnbo.setParameterValue(0, info.min + (info.max - info.min) * 0.5, RNBO::TimeNow);

        // or just set it normalized 0.5 milliseconds into the next audio buffer
        rnbo.setParameterValueNormalized(0, 0.5, rnbo.getEngine()->getCurrentTime() + rnbo.msToSamps(0.5, SAMPLERATE));

        // or schedule it to a sample accurate time in the future (for example a few ms into the next audio buffer)
        // take care - you only have a limited queue size !
        rnbo.getEngine()->scheduleParameterChange(0, info.min + (info.max - info.min) * 0.2, 200);
    }

    const auto numMessages = rnbo.getNumMessages();
    if (numMessages > 0 ) {
        for (RNBO::MessageIndex i = 0; i < numMessages; i++) {
            const auto info = rnbo.getMessageInfo(i);
            // note: the message tag info is the hunman readable string that you used to name your message port
            // the MessageTag is the in hash of it, that can be generated by the helper function RNBO::TAG()
            std::cout << info.tag << ", " << info.type << std::endl;
        }

        const RNBO::list l;
        // send something through a message port (ignore the second param objectId)
        rnbo.processNumMessage(RNBO::TAG("mymessage"), RNBO::TAG(""), RNBO::TimeNow, 0);
        rnbo.processListMessage(RNBO::TAG("mymessage"), RNBO::TAG(""), RNBO::TimeNow, l);
        rnbo.processBangMessage(RNBO::TAG("mymessage"), RNBO::TAG(""), RNBO::TimeNow);
    }

    // start transport
    rnbo.processTransportEvent(RNBO::TimeNow, RNBO::TransportState::RUNNING);

    // similar if you want to sync its beattime/time signature, BBU
    rnbo.processTimeSignatureEvent(RNBO::TimeNow, 4, 4);

    // send some MIDI event
	uint8_t midiNote[3];
	midiNote[0] = 144;
	midiNote[1] = 60;
	midiNote[2] = 100;

	rnbo.processMidiEvent(0, 0, midiNote, 3);
    
    // and process for (roughtly) a second
    for (int i = 0; i < SAMPLERATE/VECTORSIZE; i++) {
        rnbo.process(ins.data(), numInputs, outs.data(), numOutputs, VECTORSIZE);
        copyToOoutput(outs.data(), outputFile, VECTORSIZE, samplesWritten);
    }

    // send a MIDI note off event
    midiNote[0] = 144;
    midiNote[1] = 60;
    midiNote[2] = 0;

    rnbo.processMidiEvent(0, 0, midiNote, 3);

    // and process for another second
    for (int i = 0; i < SAMPLERATE/VECTORSIZE; i++) {
        rnbo.process(ins.data(), numInputs, outs.data(), numOutputs, VECTORSIZE);
        copyToOoutput(outs.data(), outputFile, VECTORSIZE, samplesWritten);
    }

    // uncomment the next line to save your file after you adapted the path
//	outputFile.save("/PATH/TO/MY/FILE/out.wav");

	return 0;
}
