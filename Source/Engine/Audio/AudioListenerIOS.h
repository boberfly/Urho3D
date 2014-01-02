/*
 * AudioListenerIOS.h
 */

#ifndef AUDIOLISTENERIOS_H_
#define AUDIOLISTENERIOS_H_
#if defined(IOS)
void IOSAudioFixOpenALCapture(void);
void IOSAudioListenerInit(void);
float IOSAudioListenerGetMeter();
void IOSAudioListenerRelease(void);
float IOSAudioOutputMeter(void);
#endif
#endif /* AUDIOLISTENERIOS_H_ */
