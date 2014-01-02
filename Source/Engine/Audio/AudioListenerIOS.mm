/*
*   Very simple volume readout
*/
#if defined(IOS)
#import <AVFoundation/AVFoundation.h>
#import <CoreAudio/CoreAudioTypes.h>

static AVAudioRecorder *listenCapture;

void IOSAudioFixOpenALCapture(void)
{
    // The 'trick' to get OpenAL capture to work
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    NSError *error = nil;
    [audioSession setCategory:AVAudioSessionCategoryPlayAndRecord error:&error];
    [audioSession setActive:YES error:&error];
    
    // The 'trick' to have volume work with recording
    UInt32 audioRouteOverride = kAudioSessionOverrideAudioRoute_Speaker;
    AudioSessionSetProperty (kAudioSessionProperty_OverrideAudioRoute, sizeof(audioRouteOverride), &audioRouteOverride);
}

void IOSAudioListenerInit(void)
{
    // This is so we can detect the current meter level without recording to an actual file.
    if (listenCapture)
        [listenCapture release];

    NSURL *nullSoundPath = [NSURL fileURLWithPath:@"/dev/null"];

    NSDictionary *listenSettings = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithInt: kAudioFormatLinearPCM], AVFormatIDKey,
        [NSNumber numberWithFloat: 11025.0], AVSampleRateKey,
        [NSNumber numberWithInt: 1], AVNumberOfChannelsKey,
        [NSNumber numberWithInt:16], AVLinearPCMBitDepthKey,
        [NSNumber numberWithBool:NO], AVLinearPCMIsFloatKey,
        [NSNumber numberWithBool:NO], AVLinearPCMIsBigEndianKey,
        nil];

    NSError *errorListen = nil;

    listenCapture = [[AVAudioRecorder alloc]
        initWithURL: nullSoundPath
        settings:listenSettings
        error:&errorListen];

    if (listenCapture)
    {
        [listenCapture prepareToRecord];
        listenCapture.meteringEnabled = YES;
        [listenCapture record];
    }
}

float IOSAudioListenerGetMeter(void)
{
    if (listenCapture)
        [listenCapture updateMeters];
        return [listenCapture averagePowerForChannel:0];

    return 0.0f;
}

void IOSAudioListenerRelease(void)
{
    if (listenCapture)
        [listenCapture release];
}

float IOSAudioOutputMeter(void)
{
    AVAudioSession *audioSession = [AVAudioSession sharedInstance];
    return [audioSession outputVolume];
}
#endif

