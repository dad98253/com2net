/*
 * com2net2.h
 *
 *  Created on: Apr 21, 2023
 *      Author: dad
 */

#ifndef COM2NET_H_
#define COM2NET_H_


#define CMD_VECTSIZE	256

#ifndef COM2NETBMAIN
#define EXTERN          extern
#define INITIZERO
#define INITSZERO
#define INITBOOLFALSE
#define INITBOOLTRUE
#define INITNULL
#define INITNULLVECTOR
#define INITBUFFERSIZE
#define INITDIRSIZE
#else
#define EXTERN
#define INITIZERO       =0
#define INITSZERO       ={0}
#define INITBOOLFALSE   =false
#define INITBOOLTRUE    =true
#define INITNULL        =NULL
#define INITNULLVECTOR	={NULL}
#define INITBUFFERSIZE  =BUFFERSIZE
#define INITDIRSIZE     =DIRSIZE
#endif


#define MAXCOMS 128

#define SE				0xf0
#define SB				0xfa
#define WILL			0xfb
#define WONT			0xfc
#define DO				0xfd
#define DONT			0xfe
#define IAC				0xff
#define TEL_MODE		0x01
#define TEL_BINARY		0x00
#define TEL_ECHO		0x01
#define TEL_SUPRESSGOAHEAD	0x03
#define TEL_LOGOUT		0x12
#define TEL_FLOW		0x21
#define TEL_LINEMODE	0x22
#define TEL_EDITMODE	0x01
#define TEL_TRAPSIG		0x00

// client: Will = 0x08, Won't = 0x04, Do = 0x02, Don't = 0x01
// Server: Will = 0x80, Won't = 0x40, Do = 0x20, Don't = 0x10
#define CLIENTWILL		0x08
#define CLIENTWONT		0x04
#define CLIENTDO		0x02
#define CLIENTDONT		0x01

#define SERVERWILL		0x02
#define SERVERWONT		0x01
#define SERVERDO		0x08
#define SERVERDONT		0x04


// the telnet mode call back function vector:
typedef void (*TelnetModeCallBack_t)(int SetUnset, unsigned char PreviousState, int * status );
EXTERN TelnetModeCallBack_t TelnetModeCallBack[CMD_VECTSIZE] INITNULLVECTOR;
EXTERN void Register_Telnet_CommandCallBack(unsigned char mode, TelnetModeCallBack_t ptr);
#define REGISTER_TELNET_CALLBACK(mode,function) Register_Telnet_CommandCallBack(mode, (TelnetModeCallBack_t)function)
#define GET_TELNET_CALLBACK(mode) (TelnetModeCallBack[mode])



#endif /* COM2NET_H_ */
