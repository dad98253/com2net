/*
 * netlink.h
 *
 *  Created on: May 27, 2023
 *      Author: dad
 */

#ifndef RACKLINK_H_
#define RACKLINK_H_



#define CMD_VECTSIZE	256
#define BUFFERSIZE		2048
#define TWICEBUFFERSIZE	BUFFERSIZE*2
#define FOURBUFFERSIZE	BUFFERSIZE*4
#define MAX_CMDS		BUFFERSIZE
#define MAX_CMD_BUF		TWICEBUFFERSIZE

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


#define YESNO(x) ( (x) ? ("Yes") : ("No") )
#define ONOFF(x) ( (x) ? ("On") : ("Off") )

#define PING_CMD				0x01
#define LOGIN_CMD				0x02
#define NACK_CMD				0x10
#define READPOWEROUTLET_CMD		0x20
#define READDRYCONTACT_CMD		0x30
#define READOUTLETNAME_CMD		0x21
#define READCONTACTNAME_CMD		0x31
#define READOUTLETCOUNT_CMD		0x22
#define READCONTACTCOUNT_CMD	0x32
#define SEQUENCING_CMD			0x36
#define ENERGYMANAGEMENT_CMD	0x23
#define EMERGENCYPOWEROFF_CMD	0x37
#define LOGALERTS_CMD			0x40
#define LOGSTATUS_CMD			0x41
#define KILOWATTHOURS_CMD		0x50
#define PEAKVOLTAGE_CMD			0x51
#define RMSVOLTAGECHANGES_CMD	0x52
#define PEAKLOAD_CMD			0x53
#define RMSLOAD_CMD				0x54
#define TEMPERATURE_CMD			0x55
#define WATTAGE_CMD				0x56
#define POWERFACTOR_CMD			0x57
#define THERMALLOAD_CMD			0x58
#define SURGEPROTECTSTATE_CMD	0x59
#define ENERGYMANAGEMENTSTATE_CMD	0x60
#define OCCUPANCYSTATE_CMD		0x61
#define LOWVOLTAGETHRESHOLD_CMD	0x70
#define HIGHVOLTAGETHRESHOLD_CMD	0x71
#define MAXLOADCURRENT_CMD		0x73
#define MINLOADCURRENT_CMD		0x74
#define MAXTEMPERATURE_CMD		0x76
#define MINTEMPERATURE_CMD		0x77
#define LOGENTRYREAD_CMD		0x80
#define LOGENTRYCOUNT_CMD		0x81
#define CLEARLOG_CMD			0x82
#define PARTNUMBER_CMD			0x90
#define AMPHOURRATING_CMD		0x91
#define SURGEPROTECTEXIST_CMD	0x93
#define IPADDRESS_CMD			0x94
#define MACKADDRESS_CMD			0x95


#define BADCRC_ERR				0x01
#define BADLENGTH_ERR			0x02
#define BADESCAPE_ERR			0x03
#define COMMANDINVALID_ERR		0x04
#define SUBCOMMANDINVALID_ERR	0x05
#define INCORRECTBYTECOUNT_ERR	0x06
#define INVALIDDATABYTES_ERR	0x07
#define INVALIDCREDENTIALS_ERR	0x08
#define UNKNOWN_ERR				0x10
#define ACCESSDENIED_ERR		0x11

#define SET_SCMD				0x01
#define GETSTATE_SCMD			0x02



typedef struct RackLinkSendPort
{
	af_client_t *fd;
    int comfd;
    int inout;
} rlsendport_t;



// the RackLink call back function vector:
typedef void (*CommandCallBack_t)(rlsendport_t *cl, int destination, int subcommand, unsigned char * data, int datasize );
EXTERN CommandCallBack_t CommandCallBack[CMD_VECTSIZE] INITNULLVECTOR;
EXTERN void Register_CommandCallBack(unsigned char command, CommandCallBack_t ptr);
#define REGISTER_CALLBACK(command,function) Register_CommandCallBack(command, (CommandCallBack_t)function)
#define GET_CALLBACK(command) (CommandCallBack[command])


EXTERN int send_RackLink_command(rlsendport_t *fd, int destination,int command,int subcommand,unsigned char * data, unsigned int datasize);
EXTERN int send_RackLink_login(rlsendport_t *fd, char * password);
EXTERN int process_RackLink_message( rlsendport_t *rlport, char *buf, int *len, int *unused);
EXTERN int decode_RackLink_command(int *destination,int *command,int *subcommand,unsigned char * raw, unsigned int len, unsigned char ** data, int * datasize, unsigned char ** nextpacket);
EXTERN unsigned char * c2p ( const int i );

EXTERN unsigned char ctempjk;
EXTERN int retjk;

EXTERN unsigned char telnetServerMask[256] INITSZERO;
EXTERN unsigned char telnetClientMask[256] INITSZERO;
EXTERN unsigned char telnetTerminalMode[256] INITSZERO;
EXTERN unsigned char telnetEditMode INITIZERO;
EXTERN char RackLinkIsLoggedIn INITIZERO;

#endif /* RACKLINK_H_ */
