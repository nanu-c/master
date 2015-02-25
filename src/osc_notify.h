// osc_notify.h
// LiVES (lives-exe)
// (c) G. Finch 2008 - 2010
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// this is a system for monitoring LiVES using OSC

// for example, LiVES can be started like: lives -oscstart 49999
// a client can then connect to UDP port 49999, and can ask LiVES to open a notify socket on UDP port 49997
//   sendOSC -host localhost 49999 /lives/open_notify_socket,49997
//
// LiVES will then send messages of the form:
//   msg_number|msg_string 
// (msg_string may be of 0 length. The message is terminated with \n\0).
// when various events happen. The event types are enumerated below. 
//

#ifndef HAS_LIVES_OSC_NOTIFY_H
#define HAS_LIVES_OSC_NOTIFY_H

#ifdef __cplusplus
extern "C" {
#endif

#define LIVES_NOTIFY_FRAME_SYNCH 1 ///< sent when a frame is displayed
#define LIVES_NOTIFY_PLAYBACK_STARTED 2 ///< sent when a/v playback starts or clip is switched
#define LIVES_NOTIFY_PLAYBACK_STOPPED 3 ///< sent when a/v playback ends

/// sent when a/v playback ends and there is recorded data for 
/// rendering/previewing
#define LIVES_NOTIFY_PLAYBACK_STOPPED_RD 4

#define LIVES_NOTIFY_DIALOG_CLOSED 16

#define LIVES_NOTIFY_RECORD_STARTED 32 ///< sent when record starts (TODO)
#define LIVES_NOTIFY_RECORD_STOPPED 33 ///< sent when record stops (TODO)

#define LIVES_NOTIFY_QUIT 64 ///< sent when app quits

#define LIVES_NOTIFY_CLIP_OPENED 128  ///< msg_string starts with new clip number
#define LIVES_NOTIFY_CLIP_CLOSED 129


#define LIVES_NOTIFY_CLIPSET_OPENED 256 ///< msg_string starts with setname
#define LIVES_NOTIFY_CLIPSET_SAVED 257


#define LIVES_NOTIFY_SUCCESS 512
#define LIVES_NOTIFY_FAILED 1024
#define LIVES_NOTIFY_CANCELLED 2048

#define LIVES_NOTIFY_MODE_CHANGED 4096 ///< mode changed to clip editor or to multitrack


#define LIVES_NOTIFY_PRIVATE_INT 32767
#define LIVES_NOTIFY_PRIVATE_STRING 32768


  // > 65536 reserved for custom


#ifdef __cplusplus
}
#endif

#endif
