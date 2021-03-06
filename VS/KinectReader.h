#pragma once
#include "Windows.h"
struct INuiSensor;
//#include "NuiImageCamera.h"
struct Point;

/* Voice Recognition Start */

#include "SpeechController.h"

// For configuring DMO properties
#include <wmcodecdsp.h>

// For FORMAT_WaveFormatEx and such
#include <uuids.h>

#include "KinectAudioStream.h"
#include "resource.h"

// For speech APIs
// NOTE: To ensure that application compiles and links against correct SAPI versions (from Microsoft Speech
//       SDK), VC++ include and library paths should be configured to list appropriate paths within Microsoft
//       Speech SDK installation directory before listing the default system include and library directories,
//       which might contain a version of SAPI that is not appropriate for use together with Kinect sensor.
#include <sapi.h>
#include <sphelper.h>
/* Voice Recognition End */

/// <summary>
/// Application class for using kinect.
/// </summary>
class KinectReader
{
public:
	/// <summary>
    /// Constructor
    /// </summary>
	KinectReader( unsigned int min_depth, unsigned int max_depth, float dist );
	
	/// <summary>
    /// Destructor
    /// </summary>
	~KinectReader();

	float* GetDepth();
	void ProcessDepth();

	/* Voice Recognition Start */

	static LPCWSTR          GrammarFileName;

	// Object that controls moving changes in seen due to voice recognition
    SpeechController*		m_pSpeechController;

	// Audio stream captured from Kinect.
    KinectAudioStream*      m_pKinectAudioStream;

	// Stream given to speech recognition engine
    ISpStream*              m_pSpeechStream;

	// Speech recognizer
    ISpRecognizer*          m_pSpeechRecognizer;
	
    // Speech recognizer context
    ISpRecoContext*         m_pSpeechContext;

    // Speech grammar
    ISpRecoGrammar*         m_pSpeechGrammar;

		
    // Event triggered when we detect speech recognition
    HANDLE                  m_hSpeechEvent;


    /// <summary>
    /// Initialize Kinect audio stream object.
    /// </summary>
    /// <returns>S_OK on success, otherwise failure code.</returns>
    HRESULT                 InitializeAudioStream();
	/// <summary>
    /// Create speech recognizer that will read Kinect audio stream data.
    /// </summary>
    /// <returns>
    /// <para>S_OK on success, otherwise failure code.</para>
    /// </returns>
    HRESULT                 CreateSpeechRecognizer();

    /// <summary>
    /// Load speech recognition grammar into recognizer.
    /// </summary>
    /// <returns>
    /// <para>S_OK on success, otherwise failure code.</para>
    /// </returns>
    HRESULT                 LoadSpeechGrammar();

    /// <summary>
    /// Start recognizing speech asynchronously.
    /// </summary>
    /// <returns>
    /// <para>S_OK on success, otherwise failure code.</para>
    /// </returns>
    HRESULT                 StartSpeechRecognition();

    /// <summary>
    /// Process recently triggered speech recognition events.
    /// </summary>
    void                    ProcessSpeech(Input*);

	/// <summary>
    /// Maps a specified speech semantic tag to the corresponding action to be performed on turtle.
    /// </summary>
    /// <returns>
    /// Action that matches <paramref name="pszSpeechTag"/>, or TurtleActionNone if no matches were found.
    /// </returns>
    SpeechAction            MapSpeechTagToAction(LPCWSTR pszSpeechTag);

	/* Voice Recognition End */

private:
	KinectReader();
	unsigned int _min_depth;
	unsigned int _max_depth;
	float        _active_depth;


	// Current Kinect
    INuiSensor*             m_pNuiSensor;
	static const int        cDepthWidth  = 640;
    static const int        cDepthHeight = 480;
    static const int        cBytesPerPixel = 1;

	float*					m_depth;
	
	/// <summary>
    /// Create the first connected Kinect found.
    /// </summary>
    /// <returns>S_OK on success, otherwise failure code.</returns>
	HRESULT                 CreateFirstConnected();


	HANDLE                  m_hNextDepthFrameEvent;
	HANDLE                  m_pDepthStreamHandle;

	
};

