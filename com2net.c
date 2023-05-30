
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H



#include "appf.h"
#include <termios.h>
#ifdef HAVE_EXPLAIN_H
#include <libexplain/libexplain.h>
#endif  // HAVE_EXPLAIN_H
#define COM2NETBMAIN
#include "com2net.h"
#include "racklink.h"

#define VER_MAJOR 1
#define VER_MINOR 0
#define MAXDECODE	250
#define MAX_CMDS		2048
#define MAX_CMD_BUF		4096
#define DEFAULT_CONNECT_TIMO	5
#define DEFAULT_CMD_TIMO	10
#define DEFAULT_PORT		23

int done=0;

typedef struct _connect
{
	af_client_t *client;	// af_client_t struct
	int busy;				// server state: busy after command sent, idle once prompt detected
	unsigned int connect_timo;	// connect timeout
	unsigned int cmd_timo;		// command timeout
} connect_t;


typedef struct _opts
{
    int debug_mode;
    int hide_prompt;
    int dump_hex;
	char delim_char[2];
	char filename[PATH_MAX];
} opt_t;


typedef struct _command
{
    char buf[MAX_CMD_BUF];
    char *part[MAX_CMDS];
    int num;
    int current;
} command_t;

typedef struct _tcli
{
	opt_t opt;
	connect_t conn;
	command_t cmd;
	int bailout;
} tcli_t;

// init defaults
tcli_t tcli = {
	opt:{
		debug_mode:1,
		hide_prompt:0,
		dump_hex:0,
		delim_char:",",
		filename:""
		},
	conn:{
		busy:0,
		connect_timo:DEFAULT_CONNECT_TIMO,
		cmd_timo:DEFAULT_CMD_TIMO
		},
	cmd:{
		buf:"",
		part:{NULL},
		num:0,
		current:0
		}
};
af_daemon_t mydaemon;
af_server_t myserver;

af_server_t comserver;
af_client_t comclient;

typedef struct _comport {
	int              fd;
	char            *dev;
	int              speed;
	char            *logfile;
	FILE            *logfh;
	int              tcpport;
	af_server_t      comserver;
	af_server_cnx_t *cnx;
	int              telnet_state;
	int		 		 inout;
	char			*remote;
	af_client_t      comclient;
	char			*prompt;
	char			*commands;
	char			*password;
	int				numprompts;
	unsigned int 	connect_timo;	/* connect timeout */
	unsigned int 	cmd_timo;		/* command timeout */
	char			decoded[MAXDECODE];
} comport;

int numcoms=0;
comport coms[MAXCOMS];

void c2m_new_cnx( af_server_cnx_t *cnx, void *context );
void c2m_del_cnx( af_server_cnx_t *cnx );
void c2m_cli( char *cmd, af_server_cnx_t *cnx );
void com_new_cnx( af_server_cnx_t *cnx, void *context );
void com_del_cnx( af_server_cnx_t *cnx );
void com_handler( char *cmd, af_server_cnx_t *cnx );
void com_port_handler( af_poll_t *ap );
void handle_RackLink_server_socket_event( af_poll_t *ap );
void RackLink_com_port_handler( af_poll_t *ap );
void Register_CommandCallBack(unsigned char command, CommandCallBack_t ptr);
CommandCallBack_t ProcessPing (rlsendport_t *af, int destination, int subcommand, unsigned char * envelope, int datasize );
CommandCallBack_t ProcessPowerOutletStatus (rlsendport_t *af, int destination, int subcommand, unsigned char * envelope, int datasize );
CommandCallBack_t ProcessLogin (rlsendport_t *fd, int destination, int subcommand, unsigned char * envelope, int datasize );
int CheckTelnetNegotiationStatus( int fd , unsigned char * outbuf );
int Dump_af_client ( af_client_t * af , char * af_name , char * routine);
char * str2str ( char **str, char ** p , int maxlen);
char * p2str ( char **str, void * p );

extern int termios(int fd);

extern int af_client_start( comport *coms );
extern void handle_server_socket_event( af_poll_t *af );
extern void handle_server_socket_raw_event( af_poll_t *af );

int stripquotes(char ** string ) {
	char * prompt = *string;
	int iret = 0;
	if ( *prompt == '\"' && *(prompt + strlen(prompt) - 1) == '\"' ) {
		prompt = strdup(*string+1);
		free(*string);
		*(prompt + strlen(prompt) - 1) = '\000';
		iret = 1;
	}
	*string = prompt;
	return(iret);
}

void c2m_signal( int signo )
{
	done = 1;
	return;
}

void c2m_usage( )
{
	printf("com2net [opts]\n" );
	printf("opts:\n" );
	printf(" -v    = Version\n" );
	printf(" -f    = run in foreground\n" );
	printf(" -s    = use syslog in foreground\n" );
	printf(" -o    = set log filename\n" );
	printf(" -l    = set log level 0-7\n" );
	printf(" -m    = set log mask (0xfffffff0)\n" );
	printf(" -n    = set application name\n" );
	exit(0);
}

int convert_speed( int sp )
{
	switch (sp)
	{
#ifdef B0
	case 0:
		return B0;
#endif
#ifdef B50
	case 50:
		return B50;
#endif
#ifdef B75
	case 75:
		return B75;
#endif
#ifdef B110
	case 110:
		return B110;
#endif
#ifdef B134
	case 134:
		return B134;
#endif
#ifdef B150
	case 150:
		return B150;
#endif
#ifdef B200
	case 200:
		return B200;
#endif
#ifdef B300
	case 300:
		return B300;
#endif
#ifdef B600
	case 600:
		return B600;
#endif
#ifdef B1200
	case 1200:
		return B1200;
#endif
#ifdef B1800
	case 1800:
		return B1800;
#endif
#ifdef B2400
	case 2400:
		return B2400;
#endif
#ifdef B4800
	case 4800:
		return B4800;
#endif
#ifdef B9600
	case 9600:
		return B9600;
#endif
#ifdef B19200
	case 19200:
		return B19200;
#endif
#ifdef B38400
	case 38400:
		return B38400;
#endif
#ifdef B57600
	case 57600:
		return B57600;
#endif
#ifdef B115200
	case 115200:
		return B115200;
#endif
#ifdef B230400
	case 230400:
		return B230400;
#endif
#ifdef B460800
	case 460800:
		return B460800;
#endif
#ifdef B500000
	case 500000:
		return B500000;
#endif
#ifdef B576000
	case 576000:
		return B576000;
#endif
#ifdef B921600
	case 921600:
		return B921600;
#endif
#ifdef B1000000
	case 1000000:
		return B1000000;
#endif
#ifdef B1152000
	case 1152000:
		return B1152000;
#endif
#ifdef B1500000
	case 1500000:
		return B1500000;
#endif
#ifdef B2000000
	case 2000000:
		return B2000000;
#endif
#ifdef B2500000
	case 2500000:
		return B2500000;
#endif
#ifdef B3000000
	case 3000000:
		return B3000000;
#endif
#ifdef B3500000
	case 3500000:
		return B3500000;
#endif
#ifdef B4000000
	case 4000000:
		return B4000000;
#endif
	default:
		return -1;
	}
}
// Read configfile
// <tcpport>,</dev/ttyXXX>,<speed>,logfile
//
void c2m_read_config( char *conffile )
{
	FILE    *fh;
	char     line[2048], *ptr;
	int      tcpport;
	char    *dev;
	int      speed;
	char    *logfile;
	FILE    *logfh;
	comport *comp;
	int	inout = 0;
	char 	*remote = NULL;
	char 	*prompt = NULL;
	char 	*commands = NULL;

	fh = fopen( conffile, "r" );
	if ( fh == NULL )
	{
		printf( "Failed to open config file %s\n", conffile );
		exit(-1);
	}

	while ( !feof(fh) )
	{
		ptr = fgets( line, 2047, fh );

		if ( ptr == NULL )
		{
			break;
		}
		if ( line[0] == '#' || strlen( line ) < 3 )
		{
			continue;
		}
		ptr = strtok( line, ", \t\n\r" );
		if (strncmp(ptr,(char *) "out",3) == 0) {
			inout = 1;
			ptr = strtok( NULL, ", \t\n\r" );
		} else if (strncmp(ptr,(char *) "racklinks",9) == 0) {
			inout = 2;
			ptr = strtok( NULL, ", \t\n\r" );
		} else if (strncmp(ptr,(char *) "racklink",8) == 0) {
			inout = 3;
			ptr = strtok( NULL, ", \t\n\r" );
		} else {
			inout = 0;
			if (strncmp(ptr,(char *) "in",2) == 0) {
				ptr = strtok( NULL, ", \t\n\r" );
			}
		}
		tcpport = atoi( ptr );
		if ( (!inout) && (tcpport < 1024) )
		{
			printf( "Bad TCP port number %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		dev = NULL;
		dev = strdup(ptr);
		stripquotes(&dev);
		if ( strlen(dev) > 0 && strncmp( dev, "/dev", 4 ) != 0 )
		{
			printf( "Bad device %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		speed = atoi( ptr );
		if ( speed < 0 )
		{
			printf( "Bad speed %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		if ( ptr )
		{
			logfile = NULL;
			logfile = strdup(ptr);
			stripquotes(&logfile);
			logfh = fopen( logfile, "a+" );
			if ( logfh == NULL ) {
#ifdef HAVE_EXPLAIN_H
				fprintf(stderr, "log file %s could not be opened: errno %d (%s)\n",\
						logfile, errno, explain_fopen(logfile, "a+") );
#else	//  HAVE_EXPLAIN_H
				fprintf(stderr, "log file %s could not be opened: errno %d (%s)\n",\
						logfile, errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
				logfile = NULL;
			}
			ptr = strtok( NULL, ", \t\n\r" );
		}
		else
		{
			logfile = NULL;
			logfh = NULL;
		}
		if ( ptr )
		{
			remote = strdup(ptr);
			stripquotes(&remote);
			ptr = strtok( NULL, ",\t\n\r" );
		}
		else
		{
			remote = NULL;
		}
		if ( ptr )
		{
			prompt = strdup(ptr);
			stripquotes(&prompt);
			ptr = strtok( NULL, ", \t\n\r" );
		}
		else
		{
			prompt = NULL;
		}
		if ( ptr )
		{
			commands = strdup(ptr);
			stripquotes(&commands);
			ptr = strtok( NULL, ", \t\n\r" );
		}
		else
		{
			commands = NULL;
		}

		comp = &coms[numcoms];
		comp->tcpport = tcpport;
		comp->dev = dev;
		comp->speed = convert_speed(speed);
		comp->logfile = logfile;
		comp->logfh = logfh;
		comp->fd = -1;
		comp->cnx = NULL;
		comp->inout = inout;
		comp->remote = remote;
		comp->prompt = prompt;
		comp->commands = commands;
		comp->numprompts = 0;
		if (comp->commands != NULL) comp->numprompts = 1;
		comp->connect_timo = 15;
		numcoms++;
		if (numcoms == MAXCOMS) {
			fprintf(stderr,"too many coms defined in the config file\nmaximum allowed is %i\n",MAXCOMS);
			abort();
		}
	}
}

int main( int argc, char **argv )
{
	int   ca, i;
	char *conffile = "/etc/com2net.conf";
	af_client_t *af_client_temp;

	mydaemon.appname = argv[0];
#ifdef DEBUG
	mydaemon.daemonize = 0;
#else	// DEBUG
	mydaemon.daemonize = 1;
#endif	// DEBUG
	mydaemon.log_level = LOG_WARNING;
	mydaemon.sig_handler = c2m_signal;
	mydaemon.log_name = argv[0];
	mydaemon.use_syslog = 1;

	/* read command line options */
	while ((ca = getopt(argc, argv, "hvfso:l:m:n:")) != -1)
	{
		switch (ca)
		{
		case 'v':
			printf("%s Version: %d.%d\n", argv[0], VER_MAJOR, VER_MINOR);
			exit(0);
			break;
		case 'f':
			mydaemon.daemonize = 0;
			break;
		case 's':
			mydaemon.use_syslog = 1;
			break;

		case 'o':
			mydaemon.log_filename = optarg;
			break;

		case 'l':
			mydaemon.log_level = atoi(optarg);
			break;

		case 'm':
			if ( (strlen(optarg) > 2) && (optarg[1] == 'x') )
				sscanf(optarg,"%x",&mydaemon.log_mask);
			else
				mydaemon.log_mask = atoi(optarg);
			break;

		case 'n':
			mydaemon.appname = optarg;
			mydaemon.log_name = optarg;
			break;

		case 'h':
		default:
			c2m_usage();
			break;
		}
	}

#ifdef HAVE_EXPLAIN_H
	explain_program_name_set(mydaemon.appname);
	explain_option_hanging_indent_set(4);
#endif	// 	HAVE_EXPLAIN_H

	c2m_read_config( conffile );
	
	af_daemon_set( &mydaemon );
	af_daemon_start();

	myserver.port = 0x3300;
	myserver.prompt = "com2net>";
	myserver.local = 1;
	myserver.max_cnx = 5;
	myserver.new_connection_callback = c2m_new_cnx;
	myserver.new_connection_context = &myserver;
	myserver.command_handler = c2m_cli;

	af_server_start( &myserver );

	for ( i=0; i<numcoms; i++ )
	{
		if ( coms[i].inout == 1 ) {
			af_client_temp = af_client_new( (char*)"telnet", (unsigned int)INADDR_LOOPBACK, coms[i].tcpport, coms[i].prompt );
			af_log_print(LOG_INFO, "con2net server: %s, port %d, prompt %s", (char*)"telnet", coms[i].tcpport, coms[i].prompt );
			coms[i].comclient = *af_client_temp;

/*			if ( af_client_connect( &coms[i].comclient ) )
			{
				af_log_print(LOG_ERR, "failed to connect to server (remote=%s, port=%d, prompt=\"%s\")", coms[i].remote, coms[i].comclient.port, coms[i].comclient.prompt );
				continue;
			}
			else
			{
				af_log_print(LOG_DEBUG, "connected to server (remote=%s, port=%d)", coms[i].remote, coms[i].comclient.port );
			}

			// detect the initial prompt
			if ( af_client_get_prompt( &(coms[i].comclient), coms[i].connect_timo*1000 ) )	// Note: this is a macro for af_client_read_timeout (see appf.h)
			{
				af_log_print(LOG_ERR, "failed to connect to detect prompt (\"%s\") within timeout (%d secs)", coms[i].comclient.prompt, coms[i].connect_timo );
				continue;
			}
			// we are now connected to the remote server and logged in, connect the remote to the desired serial port and
			// set up the polls.
*/
			coms[i].comserver.port = coms[i].tcpport;
			coms[i].comserver.prompt = "";
			coms[i].comserver.local = 0;
			coms[i].comserver.max_cnx = 2;
			coms[i].comserver.new_connection_callback = com_new_cnx;
			coms[i].comserver.new_connection_context = &coms[i];
			coms[i].comserver.command_handler = com_handler;
			coms[i].comclient.service = strdup ("telnet");

			af_client_start( &coms[i] );
			coms[i].comserver.fd = coms[i].comclient.sock;

		} else if ( coms[i].inout == 2 ) {
			af_client_temp = af_client_new( (char*)"RackLink", (unsigned int)INADDR_LOOPBACK, coms[i].tcpport, coms[i].prompt );
			af_log_print(LOG_INFO, "con2net server: %s, port %d, prompt %s", (char*)"RackLink", coms[i].tcpport, coms[i].prompt );
			coms[i].comclient = *af_client_temp;
			coms[i].comserver.port = coms[i].tcpport;
			coms[i].comserver.prompt = "";
			coms[i].comserver.local = 0;
			coms[i].comserver.max_cnx = 2;
			coms[i].comserver.new_connection_callback = com_new_cnx;
			coms[i].comserver.new_connection_context = &coms[i];
			coms[i].comserver.command_handler = com_handler;
			coms[i].comclient.service = strdup ("RackLink");

			af_server_start( &coms[i].comserver );
//			coms[i].comserver.fd = coms[i].comclient.sock;
			// This is a RackLink run
			// set any required RackLink callback functions
			REGISTER_CALLBACK(PING_CMD, ProcessPing);
			REGISTER_CALLBACK(READPOWEROUTLET_CMD, ProcessPowerOutletStatus);
			REGISTER_CALLBACK(LOGIN_CMD, ProcessLogin);

		} else if ( coms[i].inout == 3 ) {
			af_client_temp = af_client_new( (char*)"RackLink", (unsigned int)INADDR_LOOPBACK, coms[i].tcpport, coms[i].prompt );
			af_log_print(LOG_INFO, "con2net server: %s, port %d, prompt %s", (char*)"RackLink", coms[i].tcpport, coms[i].prompt );
			coms[i].comclient = *af_client_temp;
			coms[i].comserver.port = coms[i].tcpport;
			coms[i].comserver.prompt = "";
			coms[i].comserver.local = 0;
			coms[i].comserver.max_cnx = 2;
			coms[i].comserver.new_connection_callback = com_new_cnx;
			coms[i].comserver.new_connection_context = &coms[i];
			coms[i].comserver.command_handler = com_handler;
			coms[i].comclient.service = strdup ("RackLink");

			af_client_start( &coms[i] );
			coms[i].comserver.fd = coms[i].comclient.sock;
			// This is a RackLink run
			// set any required RackLink callback functions
			REGISTER_CALLBACK(PING_CMD, ProcessPing);
			REGISTER_CALLBACK(READPOWEROUTLET_CMD, ProcessPowerOutletStatus);

		} else {
			coms[i].comserver.port = coms[i].tcpport;
			coms[i].comserver.prompt = "";
			coms[i].comserver.local = 0;
			coms[i].comserver.max_cnx = 2;
			coms[i].comserver.new_connection_callback = com_new_cnx;
			coms[i].comserver.new_connection_context = &coms[i];
			coms[i].comserver.command_handler = com_handler;

			af_server_start( &coms[i].comserver );
		}
	}
	if (!numcoms) {
		af_log_print(LOG_ERR, "No servers or clients specified; nothing to do." );
		return 1;
	} else {
		af_log_print(LOG_INFO, "Starting %i comm services.",numcoms );
	}

	while ( !done )
	{
		af_poll_run( 100 );
	}

	return 0;
}

void c2m_cli( char *cmd, af_server_cnx_t *cnx )
{
	int i;
	if (strcmp(cmd,(char *)"bye\r\n") == 0) {
		for ( i=0; i<numcoms; i++ )
		{
			af_server_stop( &coms[i].comserver );
		}
	} else {
		fprintf( cnx->fh, " Command NOT defined: %s\n", cmd );
		fprintf( cnx->fh, " Enter \"bye\" to halt all af_servers\n" );
	}
	af_server_prompt( cnx );

}

void c2m_del_cnx( af_server_cnx_t *cnx )
{
	// Remove my client structure
	cnx->user_data = NULL;
}
void c2m_new_cnx( af_server_cnx_t *cnx, void *context )
{
	// Create a new client..
	// Set user data.
	cnx->user_data = NULL;
	cnx->disconnect_callback = c2m_del_cnx;
}

/////////////////////////////////////////////////////////////////////
// Com port
//
void com_port_close( comport *comp )
{
	if ( comp->fd >= 0 )
	{
		close( comp->fd );
		comp->fd = -1;
	}
	if ( comp->cnx )
	{
		af_server_disconnect(comp->cnx);
	}
}
void com_port_handler( af_poll_t *ap )
{
	int              len = 0;
	char             buf[2048];
	comport         *comp = (comport *)ap->context;
	int				 iret = 0;

	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
#ifdef HAVE_EXPLAIN_H
				af_log_print( LOG_WARNING, "com port fd %d closed: errno %d (%s)",\
					ap->fd, errno, explain_read(ap->fd, buf, sizeof(buf)-1 ) );
#else	// HAVE_EXPLAIN_H
				af_log_print( LOG_WARNING, "com port fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );
#endif	// HAVE_EXPLAIN_H
				af_poll_rem( ap->fd );
				com_port_close( comp );
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "com to telnet %d -> [%s]", len, buf );
			// Send data to the client
			if ( comp->cnx )
			{
				if ( ( iret = write( comp->cnx->fd, buf, len ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
					fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, explain_read(comp->cnx->fd, buf, len) );
#else	//  HAVE_EXPLAIN_H
					fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
					//               write to telnet port???
#endif	//  HAVE_EXPLAIN_H
				}
			}
			// Log it
			if ( comp->logfh )
			{
				fwrite( buf, sizeof(char), len, comp->logfh );
				fflush( comp->logfh );
			}
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "com error, revents: %d", ap->revents );
		af_poll_rem( ap->fd );
		com_port_close( comp );
	}
}

void RackLink_com_port_handler( af_poll_t *ap )
{
	int              len = 0;
	char             buf[2048];
	char             dumpbuf[9000];
	int 			 lastchr;
	int 			 morechr;
	int				 i;
	comport         *comp = (comport *)ap->context;
	af_client_t		*client = &(comp->comclient);
	int				 iret = 0;
	rlsendport_t	 rlport;

	rlport.fd = client;
	rlport.comfd = comp->fd;
	rlport.inout =	comp->inout;


	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
#ifdef HAVE_EXPLAIN_H
				af_log_print( LOG_WARNING, "com port fd %d closed: errno %d (%s)",\
					ap->fd, errno, explain_read(ap->fd, buf, sizeof(buf)-1)  );
#else	//  HAVE_EXPLAIN_H
				af_log_print( LOG_WARNING, "com port fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );
#endif	//  HAVE_EXPLAIN_H
				af_poll_rem( ap->fd );
				com_port_close( comp );
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "com to telnet %d", len );
			// Send data to the client
			if ( comp->cnx )
			{
//				if ( ( iret = write( comp->cnx->fd, buf, len ) ) == -1 ) {
//					fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
					//               write to telnet port???
				process_RackLink_message(&rlport, buf, &len);
//				}
			}
			// Log it
			if ( comp->logfh )
			{
				lastchr = sprintf(dumpbuf, "received %i bytes : (0x)",len);
				for ( i = 0; i < len; i++) {
					morechr = sprintf(dumpbuf+lastchr," %02x", (unsigned char)(*(buf+i)));
					lastchr += morechr;
				}
				morechr = sprintf(dumpbuf+lastchr,"\n");
				lastchr += morechr;
				*(dumpbuf+lastchr) = '\000';

				fflush(stdout);
				iret = fwrite( buf, sizeof(char), lastchr+1, comp->logfh );
				if ( iret < lastchr ) {
					af_log_print( LOG_ERR, "fwrite in %s failed - returned %i when %i was expected", __func__ , iret, lastchr );
				}
				fflush( comp->logfh );
			}
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "com error, revents: %d", ap->revents );
		af_poll_rem( ap->fd );
		com_port_close( comp );
	}
}

int com_set_port(int fd, int speed, int parity)
{
	struct termios tty;
// on my linux, the termios struct looks as follows:
// struct termios
//   {
//     tcflag_t c_iflag;		/* input mode flags */
//     tcflag_t c_oflag;		/* output mode flags */
//     tcflag_t c_cflag;		/* control mode flags */
//     tcflag_t c_lflag;		/* local mode flags */
//     cc_t c_line;				/* line discipline */
//     cc_t c_cc[NCCS];			/* control characters */
//     speed_t c_ispeed;		/* input speed */
//     speed_t c_ospeed;		/* output speed */
// #define _HAVE_STRUCT_TERMIOS_C_ISPEED 1
// #define _HAVE_STRUCT_TERMIOS_C_OSPEED 1
//   };
	memset (&tty, 0, sizeof tty);
// get a copy of the current com port attributes
	if ( tcgetattr (fd, &tty) ) {
#ifdef HAVE_EXPLAIN_H
        af_log_print( LOG_ERR, "tcgetattr error %d on file descriptor %i ... %s", errno, fd , explain_errno_tcgetattr(errno, fd, &tty));
#else	// HAVE_EXPLAIN_H
		af_log_print( LOG_ERR, "tcgetattr error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
#endif  // HAVE_EXPLAIN_H
		return -1;
	}
// set the comm port attributes to a known state (this will likely whipe out most, if not all, of the settings)
	cfmakeraw( &tty );
//	if (errno) {
//		af_log_print( LOG_ERR, "cfmakeraw error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
//		return -1;
//	}
// per the ubuntu 22.04 man page:
//    cfmakeraw() sets the terminal to something like the "raw" mode  of  the
//    old  Version 7 terminal driver: input is available character by charac‐
//    ter, echoing is disabled, and all special processing of terminal  input
//    and  output characters is disabled.  The terminal attributes are set as
//    follows:
//        termios_p->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP
//                        | INLCR | IGNCR | ICRNL | IXON);
//        termios_p->c_oflag &= ~OPOST;
//        termios_p->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
//        termios_p->c_cflag &= ~(CSIZE | PARENB);
//        termios_p->c_cflag |= CS8;

// set input and output baud rates
	if ( cfsetospeed (&tty, speed) ) {
		af_log_print( LOG_ERR, "cfsetospeed error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
		return -1;
	}
	if ( cfsetispeed (&tty, speed) ) {
		af_log_print( LOG_ERR, "cfsetispeed error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
		return -1;
	}
// these notes are left over from the original code:
//	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;		// 8-bit chars
	// disable IGNBRK for mismatched speed tests; otherwise receive break
	// as \000 chars
	//tty.c_iflag &= ~IGNBRK;			// disable break processing
//	tty.c_iflag = 0;				// no input processing

//	tty.c_lflag = 0;				// no signaling chars, no echo,
									// no canonical processing
//	tty.c_oflag = 0;				// no remapping, no delays
//	tty.c_cc[VMIN]  = 0;			// read doesn't block
//	tty.c_cc[VTIME] = 5;			// 0.5 seconds read timeout

//	tty.c_iflag &= ~(IXON | IXOFF | IXANY);	// shut off xon/xoff ctrl

//	tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
									// enable reading
//	tty.c_cflag &= ~(PARENB | PARODD);		// shut off parity
//	tty.c_cflag |= parity;

	tty.c_cc[VDISCARD]= 0;          // no discard
//   VDISCARD (Not in POSIX; not supported under Linux; 017, SI, Ctrl-O)  Tog‐
//            gle: start/stop discarding pending output.  Recognized when IEX‐
//            TEN is set, and then not passed as input.
	tty.c_cc[VEOF]    = 0;          // no
//    VEOF   (004, EOT, Ctrl-D) End-of-file character (EOF).  More precisely:
//           this  character  causes the pending tty buffer to be sent to the
//           waiting user program without waiting for end-of-line.  If it  is
//           the first character of the line, the read(2) in the user program
//           returns 0, which signifies end-of-file.  Recognized when  ICANON
//           is set, and then not passed as input.
	tty.c_cc[VEOL]    = 0;          // no
//    VEOL   (0,  NUL)  Additional  end-of-line  character (EOL).  Recognized
//            when ICANON is set.
	tty.c_cc[VEOL2]   = 0;          // no
//    VEOL2  (not in POSIX; 0, NUL) Yet another end-of-line character (EOL2).
//           Recognized when ICANON is set.
	tty.c_cc[VERASE]  = 0;          // no
//    VERASE (0177, DEL, rubout, or 010, BS, Ctrl-H, or also #) Erase charac‐
//           ter (ERASE).  This erases the previous not-yet-erased character,
//           but  does  not  erase past EOF or beginning-of-line.  Recognized
//           when ICANON is set, and then not passed as input.
	tty.c_cc[VINTR]   = 0;          // no
//    VINTR  (003, ETX, Ctrl-C, or also 0177, DEL, rubout) Interrupt  charac‐
//           ter (INTR).  Send a SIGINT signal.  Recognized when ISIG is set,
//           and then not passed as input.
	tty.c_cc[VKILL]   = 0;          // no
//    VKILL  (025, NAK, Ctrl-U, or Ctrl-X, or also @) Kill character  (KILL).
//           This  erases  the input since the last EOF or beginning-of-line.
//           Recognized when ICANON is set, and then not passed as input.
	tty.c_cc[VLNEXT]  = 0;          // no
//    VLNEXT (not in POSIX; 026, SYN, Ctrl-V) Literal next  (LNEXT).   Quotes
//           the  next  input  character,  depriving it of a possible special
//           meaning.  Recognized when IEXTEN is set, and then not passed  as
//           input.
	tty.c_cc[VMIN]    = 0;			// read doesn't block
//    VMIN   Minimum number of characters for noncanonical read (MIN).
	tty.c_cc[VQUIT]   = 0;          // no
//    VQUIT  (034,  FS,  Ctrl-\) Quit character (QUIT).  Send SIGQUIT signal.
//           Recognized when ISIG is set, and then not passed as input.
	tty.c_cc[VREPRINT]= 0;          // no
//    VREPRINT (not in POSIX; 022, DC2, Ctrl-R) Reprint unread characters  (RE‐
//           PRINT).  Recognized when ICANON and IEXTEN are set, and then not
//           passed as input.
	tty.c_cc[VSTART]  = 0;          // no
//    VSTART (021, DC1, Ctrl-Q) Start  character  (START).   Restarts  output
//           stopped by the Stop character.  Recognized when IXON is set, and
//           then not passed as input.
	tty.c_cc[VSTOP]   = 0;          // no
//    VSTOP  (023, DC3, Ctrl-S) Stop character  (STOP).   Stop  output  until
//           Start  character  typed.   Recognized when IXON is set, and then
//           not passed as input.
	tty.c_cc[VSUSP]   = 0;          // no
//    VSUSP  (032, SUB, Ctrl-Z) Suspend character (SUSP).  Send SIGTSTP  sig‐
//           nal.  Recognized when ISIG is set, and then not passed as input.
	tty.c_cc[VTIME]   = 5;			// 0.5 seconds read timeout
//    VTIME  Timeout in deciseconds for noncanonical read (TIME).
// Note (jck): setting Setting both VMIN & VTIME to 0 will give a non-blocking read... I'm not sure why he did this
	tty.c_cc[VWERASE] = 0;          // no
//    VWERASE (not  in  POSIX;  027, ETB, Ctrl-W) Word erase (WERASE).  Recog‐
//           nized when ICANON and IEXTEN are set, and then not passed as in‐
//           put.
	tty.c_cflag &= ~CSTOPB;
//    CSTOPB Set two stop bits, rather than one
	tty.c_cflag &= ~CRTSCTS;
//    CRTSCTS
//           (not in POSIX) Enable RTS/CTS  (hardware)  flow  control.   [re‐
//           quires _BSD_SOURCE or _SVID_SOURCE]

	if ( tcsetattr (fd, TCSANOW, &tty) != 0 )
	{
#ifdef HAVE_EXPLAIN_H
		af_log_print( LOG_ERR, "tcsetattr error %d : %s", errno, explain_tcsetattr(fd, TCSANOW, &tty) );
#else	//  HAVE_EXPLAIN_H
		af_log_print( LOG_ERR, "tcsetattr error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
#endif	//  HAVE_EXPLAIN_H
		return -1;
	}

	return 0;
}

int open_comport( comport *comp )
{
	comp->fd = open( comp->dev, O_RDWR | O_NOCTTY | O_SYNC );
	if ( comp->fd < 0 )
	{
#ifdef HAVE_EXPLAIN_H
		af_log_print( LOG_ERR, "Failed to open com port %s, errno %d (%s)", comp->dev, errno, explain_open(comp->dev, O_RDWR | O_NOCTTY | O_SYNC, 0) );
#else	//  HAVE_EXPLAIN_H
		af_log_print( LOG_ERR, "Failed to open com port %s, errno %d (%s)", comp->dev, errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
		return -1;
	} else {
		af_log_print( LOG_DEBUG, "Opened com port %s (fd=%d)", comp->dev, comp->fd );
#ifdef DEBUG
		termios(comp->fd);
#endif	// DEBUG
	}

	com_set_port( comp->fd, comp->speed, 0);

	return 0;
}


/////////////////////////////////////////////////////////////////////
// delete connection
//
void com_del_cnx( af_server_cnx_t *cnx )
{
	comport *comp = (comport *)cnx->user_data;
	// Remove my client structure

	if ( comp != NULL )
	{
		comp->cnx = NULL;
	}
	cnx->user_data = NULL;
}

enum {
	TELNET_NONE = 0,
	TELNET_IAC = 1,
	TELNET_OPT = 2,
	TELNET_SUBOPT = 3,
	TELNET_SUBOPT_DATA = 4
};

int com_filter_telnet( void *extra, unsigned char *buf, int len )
{
	comport *comp = (comport*) extra;
	int   i;
	unsigned char  obuf[2048];
	char scratch[50];
	int   olen = 0;
	unsigned char	myWillDo = 0;	// Will = 0x08, Won't = 0x04, Do = 0x02, Don't = 0x01
	unsigned char	isSubLinemode = 0;
	// what follows will ignore all telnet option negotiation:
	for ( i=0;i<len; i++ )
	{
		switch ( comp->telnet_state )
		{
		case TELNET_NONE:
			if ( buf[i] == 255 )
			{
				comp->telnet_state = TELNET_IAC;
				strcpy(comp->decoded,(char*)"Telnet IAC " );
				myWillDo = 0;
			}
			else
			{
				obuf[olen++] = buf[i];
				*(comp->decoded) = '\000';
			}
			break;
		case TELNET_IAC:
			switch( buf[i] )
			{
			case 255:	//  IAC
				obuf[olen++] = buf[i];
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 254:	// DON'T (option code)
				strcat(comp->decoded,(char*)"Don\'t " );
				comp->telnet_state = TELNET_OPT;
				myWillDo = CLIENTDONT;
				break;
			case 253:	// DO (option code)
				strcat(comp->decoded,(char*)"Do " );
				comp->telnet_state = TELNET_OPT;
				myWillDo = CLIENTDO;
				break;
			case 252:	// WON'T (option code)
				strcat(comp->decoded,(char*)"Won\'t " );
				comp->telnet_state = TELNET_OPT;
				myWillDo = CLIENTWONT;
				break;
			case 251:	// WILL (option code)
				strcat(comp->decoded,(char*)"Will " );
				comp->telnet_state = TELNET_OPT;
				myWillDo = CLIENTWILL;
				break;
			case 250:	// SB (Indicates that what follows is subnegotiation of the indicated option)
				strcat(comp->decoded,(char*)"SB  (sub start) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_SUBOPT;
				*(comp->decoded) = '\000';
				break;
			case 240:	// SE (Indicates end of subnegotiation)
				strcat(comp->decoded,(char*)"SE  (end sub start) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;

			default:
				snprintf(scratch,50,"command 0x%x ", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			}
			break;
		case TELNET_OPT:
			switch ( buf[i] )
			{
			case 0:
				//	Binary Transmission
				snprintf(scratch,50,"Binary Transmission ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[0] |= myWillDo;
				break;
			case 1:
				//	Echo
				snprintf(scratch,50,"Echo ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[1] |= myWillDo;
				break;
			case 2:
				//	Reconnection
				snprintf(scratch,50,"Reconnection ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[2] |= myWillDo;
				break;
			case 3:
				//	Suppress Go Ahead
				snprintf(scratch,50,"Suppress Go Ahead ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[3] |= myWillDo;
				break;
			case 5:
				//	Status
				snprintf(scratch,50,"Status ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[5] |= myWillDo;
				break;
			case 17:
				//	Extended ASCII
				snprintf(scratch,50,"Extended ASCII ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[17] |= myWillDo;
				break;
			case 18:
				// logout
				strcat(comp->decoded,(char*)"logout  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[18] |= myWillDo;
				break;
			case 24:
				// Terminal Type
				strcat(comp->decoded,(char*)"Terminal Type  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
//				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
//				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[24] |= myWillDo;
				break;
			case 32:
				// Terminal Speed
				strcat(comp->decoded,(char*)"Terminal Speed  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
//				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
//				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[32] |= myWillDo;
				break;
			case 34:
				// Linemode
				strcat(comp->decoded,(char*)"Linemode  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
//				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
///				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[34] |= myWillDo;
				break;
			case 35:
				//  Display Location
				strcat(comp->decoded,(char*)"X Display Location  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
//				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
//				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[35] |= myWillDo;
				break;
			case 39:
				//  New Environment
				strcat(comp->decoded,(char*)"New Environment  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
//				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
//				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				telnetClientMask[39] |= myWillDo;
				break;
			default:
				snprintf(scratch,50,"option 0x%x ", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				telnetClientMask[buf[i]] |= myWillDo;
				break;
			}
			break;
		case TELNET_SUBOPT:
			if ( buf[i] == 255 )	// SE (End of subnegotiation parameters)
			{
				strcat(comp->decoded,(char*)"SE (sub end) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
			} else {
				snprintf(scratch,50,"Sub opt for opt: 0x%x", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_SUBOPT_DATA;
				*(comp->decoded) = '\000';
				if ( buf[i] == TEL_LINEMODE ) {
					isSubLinemode = 1;
				} else {
					isSubLinemode = 0;
				}
			}
			break;
		case TELNET_SUBOPT_DATA:
			if ( buf[i] == 255 )	// SE (End of subnegotiation parameters)
			{
				strcat(comp->decoded,(char*)"SE (sub end) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
			} else {
				snprintf(scratch,50,"Sub opt data: 0x%x", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_SUBOPT_DATA;
				*(comp->decoded) = '\000';
				if ( isSubLinemode && buf[i] == TEL_MODE && ( buf[i+1] & TEL_EDITMODE ) ) {
					telnetEditMode = 1;
					isSubLinemode = 0;
				}
			}
			break;

		}
	}
	memcpy( buf, obuf, olen );

	return olen;
}

int CheckTelnetNegotiationStatus( int fd , unsigned char * outbuf ) {
	int i;
	int status;
	int uncooperative = 0;
	// client: Will = 0x08, Won't = 0x04, Do = 0x02, Don't = 0x01
	// Server: Will = 0x80, Won't = 0x40, Do = 0x20, Don't = 0x10
	for (i=0;i<256;i++) {
		if ( telnetServerMask[i] ) {
			// check if the client is submissive
			if ( (telnetServerMask[i] & SERVERDO)   && !(telnetClientMask[i] & CLIENTWILL) ) uncooperative++;
			if ( (telnetServerMask[i] & SERVERDO)   &&  (telnetClientMask[i] & CLIENTWILL) ) {
				// process command callback if any
				if ( CommandCallBack[i] ) GET_TELNET_CALLBACK(i) ( 1, telnetTerminalMode[i], &status );
				telnetTerminalMode[i] = 1;
			}
			if ( (telnetServerMask[i] & SERVERDONT) && !(telnetClientMask[i] & CLIENTWONT) ) uncooperative++;
			if ( (telnetServerMask[i] & SERVERDONT) &&  (telnetClientMask[i] & CLIENTWONT) )  {
				// process command callback if any
				if ( CommandCallBack[i] ) GET_TELNET_CALLBACK(i) ( 0, telnetTerminalMode[i], &status );
				telnetTerminalMode[i] = 0;
			}
		}
	}
	for (i=0;i<256;i++) {
		if ( telnetClientMask[i] ) {
			// check if the client is submissive
			if ( (telnetClientMask[i] & CLIENTDO)   && !(telnetServerMask[i] & SERVERWILL) ) uncooperative++;
			if ( (telnetClientMask[i] & CLIENTDO)   &&  (telnetServerMask[i] & SERVERWILL) ) {
				// process command callback if any
				if ( CommandCallBack[i] ) GET_TELNET_CALLBACK(i) ( 1, telnetTerminalMode[i], &status );
				telnetTerminalMode[i] = 1;
			}
			if ( (telnetClientMask[i] & CLIENTDONT) && !(telnetServerMask[i] & SERVERWONT) ) uncooperative++;
			if ( (telnetClientMask[i] & CLIENTDONT) &&  (telnetServerMask[i] & SERVERWONT) ) {
				// process command callback if any
				if ( CommandCallBack[i] ) GET_TELNET_CALLBACK(i) ( 0, telnetTerminalMode[i], &status );
				telnetTerminalMode[i] = 0;
			}
		}
	}
	if ( uncooperative == 0 && telnetEditMode ) af_log_print( APPF_MASK_SERVER+LOG_INFO, "telnet terminals are cooperating");
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////xxxxxxx
	return(0);
}

void com_handle_event( af_poll_t *ap )
{
	int              len = 0;
	unsigned char    buf[2048];
	af_server_cnx_t *cnx = (af_server_cnx_t *)ap->context;
	comport *comp = (comport *)cnx->user_data;
	int				 iret = 0;


	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
#ifdef HAVE_EXPLAIN_H
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, explain_read(ap->fd, buf, sizeof(buf)-1)  );
#else	//  HAVE_EXPLAIN_H
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );
#endif	//  HAVE_EXPLAIN_H
				af_server_disconnect(cnx);
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "telnet to com %d -> [%s]", len, buf );

			// filter telnet data out
			len = com_filter_telnet( comp, buf, len );

			if ( ( iret = write( comp->fd, buf, len ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
				fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, explain_write(comp->fd, buf, len) );
#else	//  HAVE_EXPLAIN_H
				fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
			}

		}
	} else if ( ap->revents ) {
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "dcli socket error, revents: %d", ap->revents );
		af_server_disconnect(cnx);
	}
}

void handle_RackLink_server_socket_event( af_poll_t *ap )
{
	int              len = 0;
	int				 numout;
	unsigned char    buf[2048];
	unsigned char    outbuf[2048];
	char *			 ptr;
//	af_server_cnx_t *cnx = (af_server_cnx_t *)ap->context;
//	comport *comp = (comport *)cnx->user_data;
	comport *comp = (comport *)ap->context;	//  try this?? looks right hmmm...
	af_server_cnx_t *cnx = comp->cnx;
	af_client_t *af = &(comp->comclient);
	int				 iret;
	int				 OnOrOff;
	int				 Outlet;
	unsigned char	 envelope[2];
	rlsendport_t	rlport;

	rlport.fd = af;
	rlport.comfd = comp->fd;
	rlport.inout =	comp->inout;


	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
#ifdef HAVE_EXPLAIN_H
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, explain_read(ap->fd, buf, sizeof(buf)-1)  );
#else	//  HAVE_EXPLAIN_H
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );
#endif	//  HAVE_EXPLAIN_H
				af_server_disconnect(cnx);
			}
		}
		else
		{
			// terminate the read data
			buf[len] = 0;

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "telnet to RackLink com %d -> [%s]", len, buf );

			// filter telnet data out
			len = com_filter_telnet( comp, buf, len );
			CheckTelnetNegotiationStatus( ap->fd , outbuf );
			if ( RackLinkIsLoggedIn < 1 ) {
				// strip \n\r
				ptr = strtok( (char *)buf, " \t\n\r" );
				// send the password
				if ( send_RackLink_login( &rlport, ptr) ) {
					fprintf( cnx->fh, "RackLink send Failed on com port %s\r\n\n\nBYE!\r\n", comp->dev );
					af_server_disconnect(cnx);
					return;
				}
				RackLinkIsLoggedIn = -1;	// it won't be set positive until the login succeeds
				len = 0;					// short circuit further processing
			}
//	look for a valid RackLink command:
// HELP
// LOGOUT
// STATUS
// ON <Outlet Number>
// OFF <Outlet Number>
////////////////////////////////////////////////////////////////////////////////

			if ( len == 2 || strncmp ((char *)buf,(char *)"HELP",4) == 0 ) {
				iret = sprintf((char *)outbuf, "RackLink commands are:\r\n\tHELP\r\n\tLOGOUT\r\n\tSTATUS\r\n\tON <Outlet Number>\r\n\tOFF <Outlet Number>\r\n");
				*(outbuf+iret) = '\000';
				if ( ( numout = write( ap->fd, outbuf, iret+1 ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
					fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, explain_write(ap->fd, outbuf, iret+1) );
#else	//  HAVE_EXPLAIN_H
					fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
				}
			}

			if ( len > 3 ) {
				if ( strncmp ((char *)buf,(char *)"LOGOUT",6) == 0 ) {
					af_server_disconnect(cnx);
				}

				if ( strncmp ((char *)buf,(char *)"STATUS",6) == 0 ) {
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(1), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(2), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(3), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(4), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(5), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(6), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(7), 1);
					send_RackLink_command(&rlport,0,READPOWEROUTLET_CMD,GETSTATE_SCMD,c2p(8), 1);
				}


				*(buf+80) = '\000';		// make sure its terminated someplace
				ptr = strtok( (char *)buf, ", \t\n\r" );
				if (strncmp(ptr,(char *) "OFF",3) == 0) {
					OnOrOff = 1;
					ptr = strtok( NULL, ", \t\n\r" );
				} else if (strncmp(ptr,(char *) "ON",2) == 0) {
					OnOrOff = 2;
					ptr = strtok( NULL, ", \t\n\r" );
				} else {
					OnOrOff = 0;
				}
				if ( OnOrOff ) {
					Outlet = atoi( ptr );
					if ( (Outlet < 1) || (Outlet > 8) ) {
						iret = sprintf((char *)outbuf, "Bad Outlet number %s\n", ptr );
						*(outbuf+iret) = '\000';
						if ( ( numout = write( ap->fd, outbuf, iret+1 ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
							fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, explain_write(ap->fd, outbuf, iret+1) );
#else	//  HAVE_EXPLAIN_H
							fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
						}
						return;
					}
					envelope[0] = (unsigned char)Outlet;
					envelope[1] = (unsigned char)(OnOrOff -1);
					send_RackLink_command( &rlport, 0, READPOWEROUTLET_CMD, SET_SCMD, envelope, 2);
				}
			}
		}
		if ( RackLinkIsLoggedIn > 0 ) {
			iret = sprintf((char *)outbuf, "RackLink>");
			*(outbuf+iret) = '\000';
			if ( ( numout = write( ap->fd, outbuf, iret+1 ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
				fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, explain_write(ap->fd, outbuf, iret+1) );
#else	//  HAVE_EXPLAIN_H
				fprintf(stderr, "write to RackLink telnet port could not be performed: errno %d (%s)", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
			}
		}
	} else if ( ap->revents ) {
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "dcli socket error, revents: %d", ap->revents );
		af_server_disconnect(cnx);
	}
}

void com_new_cnx( af_server_cnx_t *cnx, void *context )
{
	char         buf[128];
	comport     *comp = (comport *)context;
	int 		 iret = 0;
	int			 buflen = 0;

	// If another client is connected, get rid of him.
	if ( comp->cnx != NULL )
	{
		fprintf( comp->cnx->fh, "Someone connected from another server\r\n\n\nBYE!\r\n" );

		af_server_disconnect(comp->cnx);
	}

	// get rid of the stock handler
	af_poll_rem( cnx->fd );

	cnx->inout = comp->inout;
	if ( cnx->inout != 3 ) {	// don't open a com port if this is a RackLink connect via TCP
	// Open the com port if it is not open
		if ( comp->fd < 0 )
		{
			if ( open_comport( comp ) < 0 )
			{
				fprintf( cnx->fh, "Failed to open com port %s\r\n\n\nBYE!\r\n", comp->dev );
				af_server_disconnect(cnx);
				return;
			}
		}

		comp->cnx = cnx;

		if ( cnx->inout == 2 ) {
			if (af_poll_add( comp->fd, POLLIN, RackLink_com_port_handler, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for event = 0x%x", __func__, comp->fd, POLLIN );
		} else {
			if (af_poll_add( comp->fd, POLLIN, com_port_handler, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for event = 0x%x", __func__, comp->fd, POLLIN );
		}
	}

	switch ( cnx->inout )
	{
		case 1 :
			if (af_poll_add( cnx->fd, POLLIN, handle_server_socket_event, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for telnet client event = 0x%x", __func__, cnx->fd, POLLIN );
			break;
		case 2 :
			if (af_poll_add( cnx->fd, POLLIN, handle_RackLink_server_socket_event, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for RackLink telnet server event = 0x%x", __func__, cnx->fd, POLLIN );
			break;
		case 3 :
			if (af_poll_add( cnx->fd, POLLIN, handle_server_socket_raw_event, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for RackLink telnet client event = 0x%x", __func__, cnx->fd, POLLIN );
			break;
		default :
			if (af_poll_add( cnx->fd, POLLIN, com_handle_event, cnx ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for telnet server event = 0x%x", __func__, cnx->fd, POLLIN );
			break;
	}

	// Set user data.
	cnx->user_data = comp;
	cnx->disconnect_callback = com_del_cnx;

	if ( cnx->inout == 2 ) {		//  RackLink telnet connections will use line mode:
		// see RFC 2355 and RFC 854:
/*		buf[0] = IAC;
		buf[1] = WILL;	// Will
		buf[2] = TEL_SUPRESSGOAHEAD;	// Confirm willingness to negotiate suppress go ahead
		telnetServerMask[TEL_SUPRESSGOAHEAD] |= SERVERWILL;	// Will = 0x80, Won't = 0x40, Do = 0x20, Don't = 0x10
		buf[3] = IAC;
		buf[4] = WILL;	// Do
		buf[5] = TEL_LINEMODE;	// Confirmation that you are expecting the other party to use line mode
		telnetServerMask[TEL_LINEMODE] |= SERVERDO;
		buf[6] = IAC;
		buf[7] = WONT;	// Do
		buf[8] = TEL_ECHO;	// Confirmation that you are expecting the other party to echo
		telnetServerMask[TEL_ECHO] |= SERVERDO;
		buf[9]  = IAC;
		buf[10] = DO;	// Do
		buf[11] = TEL_BINARY;	// Confirmation that you are expecting the other party to use Binary transmission
		telnetServerMask[TEL_BINARY] |= SERVERDO;
		buflen = 12; */
		buflen = sprintf(buf,"Password:");
		buf[buflen] = '\000';

	} else {					// all other type of telnet connections
		buf[0] = IAC;
		buf[1] = WILL;	// Will
		buf[2] = TEL_SUPRESSGOAHEAD;	// Confirm willingness to negotiate suppress go ahead
		telnetServerMask[TEL_SUPRESSGOAHEAD] |= SERVERWILL;
		buf[3] = IAC;
		buf[4] = WILL;	// Will
		buf[5] = TEL_ECHO;	// Confirm willingness to negotiate echo
		telnetServerMask[TEL_ECHO] |= SERVERWILL;
		buf[6] = IAC;
		buf[7] = DONT;	// Don't
		buf[8] = TEL_ECHO;	// Confirmation that you are no longer expecting the other party to echo
		telnetServerMask[TEL_ECHO] |= SERVERDONT;
		buf[9]  = IAC;
		buf[10] = DO;	// Do
		buf[11] = TEL_BINARY;	// Confirmation that you are expecting the other party to use Binary transmission
		telnetServerMask[TEL_BINARY] |= SERVERDO;
		buflen = 12;
	}

	if (buflen ) {
		if ( ( iret = write( cnx->fd, buf, buflen ) ) == -1 ) {		// going out to the newly connected telnet client
#ifdef HAVE_EXPLAIN_H
			fprintf(stderr, "write to telnet port %s could not be performed: errno %d (%s)", "?", errno, explain_write(cnx->fd, buf, buflen ) );
#else	//  HAVE_EXPLAIN_H
			fprintf(stderr, "write to telnet port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
		}
	}

	
}

void com_handler( char *cmd, af_server_cnx_t *cnx )
{
	int i = 0;
	i++;
	if(i>1000) i=0;
}

void Register_CommandCallBack(unsigned char command, CommandCallBack_t ptr) {
	CommandCallBack[command] = ptr;
	printf("Callback routine registered for RackLink command number 0x%02x\n", command);
}


CommandCallBack_t ProcessLogin ( rlsendport_t *rlport, int destination, int subcommand, unsigned char * envelope, int datasize ) {
	char	buf[256];
	int buflen = 0;
	int iret;
	af_client_t *fd = rlport->fd;
	comport *coms = (comport*)fd->extra_data;
	// login/response
	if ( subcommand == 0x10 && datasize == 4) {
		if ( *envelope == 0 ) {
			buflen = sprintf( buf, "login rejected...\ntry again.\nPassword:");
		} else if ( *envelope == 1 ) {
			buflen = sprintf( buf, "login successful...\nRackLink>");
		} else {
			buflen = sprintf( buf, "unrecognized login response...\nPossibly a connection problem. Try again.\nPassword:");
		}
		buf[buflen] = '\000';
	}

	if (buflen ) {
		if ( ( iret = write( coms->cnx->fd, buf, buflen ) ) == -1 ) {		// going out to the newly connected telnet client
#ifdef HAVE_EXPLAIN_H
			fprintf(stderr, "write to telnet port %s could not be performed: errno %d (%s)", "?", errno, explain_write(coms->cnx->fd, buf, buflen ) );
#else	//  HAVE_EXPLAIN_H
			fprintf(stderr, "write to telnet port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
		}
	}
	return(0);
}

CommandCallBack_t ProcessPing ( rlsendport_t *rlport, int destination, int subcommand, unsigned char * envelope, int datasize ) {
	// let's interogate switch settings on each ping...
	if ( subcommand == 1 ) {
		printf("Interrogating Power Outlet status...\n");
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(1), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(2), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(3), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(4), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(5), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(6), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(7), 1);
		send_RackLink_command(rlport,0,READPOWEROUTLET_CMD,0x02,c2p(8), 1);
	}
	return(0);
}

CommandCallBack_t ProcessPowerOutletStatus ( rlsendport_t *rlport, int destination, int subcommand, unsigned char * envelope, int datasize ) {
	char ctemp[260];
	// print the switch setting
	if ( datasize > 255 ) goto ret1;
	if ( subcommand == 0x10 ) {			// Response to a command
		if ( datasize < 9 ) goto ret1;
		strncpy(ctemp,(const char *)(envelope+2),4);
		ctemp[4] = '\000';
		printf ("Power outlet %u is %s, cycle time is %s seconds\n",*envelope,ONOFF(*(envelope+1)),ctemp );
	}
ret1:
	return(0);
}

unsigned char * c2p ( const int i ) { ctempjk = (unsigned char)i; return(&ctempjk);}

int send_RackLink_command( rlsendport_t *rlport, int destination,int command,int subcommand,unsigned char * data, unsigned int datasize) {
	unsigned int sum = 0;
	unsigned char * datapacket;
	unsigned char * ptr;
	int i;
	int status;
	int exitval;
	int packetsize;
	af_client_t *fd = rlport->fd;

	packetsize = datasize + 5;
	ptr = datapacket = (unsigned char *) calloc(packetsize + 3 , 1);
	*ptr = 0xfe;							// header
	ptr++;
	*ptr = (unsigned char)(datasize +3);	// length
	ptr++;
	*ptr = (unsigned char)(destination);	// Destination
	ptr++;
	*ptr = (unsigned char)(command);		// Command
	ptr++;
	*ptr = (unsigned char)(subcommand);		// Sub Command
	ptr++;
	if (datasize) {
		for (i = 0; i<datasize; i++) {
			*ptr = *(data+i);				// next byte of data
			ptr++;
		}
	}
	for ( i=0; i<packetsize; i++ ) {
		sum+=*(datapacket+i);
	}
	*ptr = sum & 0x7f;
	ptr++;
	*ptr = 0xff;




		af_log_print(LOG_DEBUG, "%s: sending RackLink command \"%i\", sub command \"%i\" to %i with %i data bytes", __func__, command, subcommand, destination, datasize );

		// show user the command we're sending unless suppressed
		if (tcli.opt.hide_prompt == FALSE)
		{

			printf("sending : (0x)");
			for (i=0; i< (packetsize+2); i++) {
				printf(" %02x", *(datapacket+i));
			}
			printf("\n");
			fflush(stdout);
		}

		if ( rlport->inout != 2 ) {
			status = af_client_send_raw( fd, datapacket, (size_t)(packetsize+2) );
			/*
			client_send can return:
				AF_TIMEOUT
				AF_ERRNO
				AF_SOCKET
				AF_OK
			*/

			switch ( status )
			{
			case AF_OK:
				// command was sent successfully
				tcli.conn.busy = TRUE;
				exitval = 0;
				break;
			default:
				//  send failed
				af_log_print(LOG_ERR, "%s: client_send returned %d (cmd=%s)", __func__, errno, strerror(errno) );
				exitval = 1;
				break;
			}
		} else {	// RackLink connected via com port
			if ( ( status = write( rlport->comfd, datapacket, (size_t)(packetsize+2) ) ) == -1 ) {
#ifdef HAVE_EXPLAIN_H
				fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, explain_write(rlport->comfd, datapacket, (size_t)(packetsize+2)) );
#else	//  HAVE_EXPLAIN_H
				fprintf(stderr, "write to com port %s could not be performed: errno %d (%s)", "?", errno, strerror(errno) );
#endif	//  HAVE_EXPLAIN_H
				exitval = 1;
			} else {
				tcli.conn.busy = TRUE;
				exitval = 0;
			}
		}


	free(datapacket);
	return(exitval);
}

int send_RackLink_login( rlsendport_t *rlport, char * password) {
	char * userpassword = NULL;
	userpassword = (char*)malloc(strlen(password)+7);
	strcpy(userpassword,"user|");
	strcat(userpassword,password);
	if ( send_RackLink_command(rlport,0,LOGIN_CMD,SET_SCMD,(unsigned char *)userpassword, strlen(userpassword)) ) {
		free(userpassword);
		return(1);
	}
	free(userpassword);
	return(0);
}

int process_RackLink_message( rlsendport_t *rlport, char *buf, int *len) {
	int destination,command,subcommand,datasize;
	unsigned char *envelope;
	unsigned char *nextpacket;
	char *tempbuf;
	int templen;
	int iret = -1;
	tempbuf = buf;
	templen = *len;

	while ( iret < 0 ) {
		iret = decode_RackLink_command( &destination, &command, &subcommand, (unsigned char *)tempbuf, (unsigned int)(templen), &envelope, &datasize, &nextpacket);
		if ( iret > 0 ) return(iret);
		printf( " destination, command, subcommand - %i, %i, %i\n",destination, command, subcommand);
		switch ( (unsigned char)command )
			{
				case NACK_CMD :	// NACK
					printf("NACK received - error detected at device\n");
					switch ( *envelope )
					{
					case BADCRC_ERR :	//	 Bad CRC on previous command
						printf("Bad CRC on previous command\n");
						break;
					case BADLENGTH_ERR :	//	 Bad Length on previous command
						printf("Bad Length on previous command\n");
						break;
					case BADESCAPE_ERR :	//   Bad Escape sequence on previous command
						printf("Bad Escape sequence on previous command\n");
						break;
					case COMMANDINVALID_ERR :	//   Previous command invalid
						printf("Previous command invalid\n");
						break;
					case SUBCOMMANDINVALID_ERR :	//   Previous sub-command invalid
						printf("Previous sub-command invalid\n");
						break;
					case INCORRECTBYTECOUNT_ERR :	//   Previous command incorrect byte count
						printf("Previous command incorrect byte count\n");
						break;
					case INVALIDDATABYTES_ERR :	//   Invalid data bytes in previous command
						printf("Invalid data bytes in previous command\n");
						break;
					case INVALIDCREDENTIALS_ERR :	//   Invalid Credentials (note: need to login again)
						printf("Invalid Credentials (note: need to login again)\n");
						break;
					case UNKNOWN_ERR :	//   Unknown Error
						printf("Unknown Error\n");
						break;
					case ACCESSDENIED_ERR :	//   Access Denied (EPO)
						printf("Access Denied (EPO)\n");
						break;
					default:
						printf("un-recognized error number (\'0x%02x\')\n",*envelope);
						return(-99);
					}
					break;
				case PING_CMD :	// Ping/Pong
					if ( subcommand == 1 ) {
						printf("Ping...(Sending Pong)\n");
						// send Pong
						send_RackLink_command(rlport,0,0x01,0x10,(unsigned char *)"", 0);
					}
					break;
				case LOGIN_CMD :	// login/response
					if ( subcommand == 0x10 && datasize == 4) {
						if ( *envelope == 0 ) {
							printf("login rejected...\n");
						} else if ( *envelope == 1 ) {
							printf("login successful...\n");
						} else {
							printf("unrecognized login response...\n");
						}
					}
					break;
				case READPOWEROUTLET_CMD :	// Read/write Power Outlet
					break;
				case READDRYCONTACT_CMD :	// Read/write Dry Contact
					break;
				case READOUTLETNAME_CMD :	// Read/write Outlet name
					break;
				case READCONTACTNAME_CMD :	// Read/write Contact name
					break;
				case READOUTLETCOUNT_CMD :	// Read Outlet count
					break;
				case READCONTACTCOUNT_CMD :	// Read Contact count
					break;
				case SEQUENCING_CMD :	// Sequencing command
					break;
				case ENERGYMANAGEMENT_CMD :	// Energy Management Command
					break;
				case EMERGENCYPOWEROFF_CMD :	// Emergency Power Off Command
					break;
				case LOGALERTS_CMD :	// Log Alerts Commands
					break;
				case LOGSTATUS_CMD :	// Log Status Commands
					break;
					// Sensor Value Commands:
				case KILOWATTHOURS_CMD :	// Kilowatt Hours
					break;
				case PEAKVOLTAGE_CMD :	// Peak Voltage
					break;
				case RMSVOLTAGECHANGES_CMD :	// RMS Voltage Changes
					break;
				case PEAKLOAD_CMD :	// Peak Load
					break;
				case RMSLOAD_CMD :	// RMS Load
					break;
				case TEMPERATURE_CMD :	// Temperature
					break;
				case WATTAGE_CMD :	// Wattage
					break;
				case POWERFACTOR_CMD :	// Power Factor
					break;
				case THERMALLOAD_CMD :	// Thermal Load
					break;
				case SURGEPROTECTSTATE_CMD :	// Surge Protection State
					break;
				case ENERGYMANAGEMENTSTATE_CMD :	// Energy Management State
					break;
				case OCCUPANCYSTATE_CMD :	// Occupancy State
					break;
					// Threshold Commands:
				case LOWVOLTAGETHRESHOLD_CMD :	// Low Voltage Threshold
					break;
				case HIGHVOLTAGETHRESHOLD_CMD :	// High Voltage Threshold
					break;
				case MAXLOADCURRENT_CMD :	// Max Load Current
					break;
				case MINLOADCURRENT_CMD :	// Min Load Current
					break;
				case MAXTEMPERATURE_CMD :	// Max Temperature
					break;
				case MINTEMPERATURE_CMD :	// Min Temperature
					break;
					// Log stuff:
				case LOGENTRYREAD_CMD :	// Log Entry Read
					break;
				case LOGENTRYCOUNT_CMD :	// Get Log Count
					break;
				case CLEARLOG_CMD :	// Clear Log
					break;
					// Product Rating and Information:
				case PARTNUMBER_CMD :	// Part Number
					break;
				case AMPHOURRATING_CMD :	// Product Amp Hour Rating
					break;
				case SURGEPROTECTEXIST_CMD :	// Product Surge Existence
					break;
				case IPADDRESS_CMD :	// Current IP Address
					break;
				case MACKADDRESS_CMD :	// MAC Address
					break;

				default:
					return(-99);
					break;
			}
// process command callbacks
		if ( CommandCallBack[command] ) GET_CALLBACK(command) ( rlport, destination, subcommand, envelope, datasize );
// are there more commands to process in this record?
		if ( iret < 0 ) {
			templen -= ( nextpacket - (unsigned char *)tempbuf );
			tempbuf = (char *)nextpacket;
		}
	}

	return(1);
}

int decode_RackLink_command(int *destination,int *command,int *subcommand,unsigned char * raw, unsigned int len, unsigned char ** data, int * datasize, unsigned char ** nextpacket) {
	unsigned int sum = 0;
	unsigned char * datapacket;
	unsigned char chksum;
	int i;
	int packetsize;
	*nextpacket = NULL;

	if ( len < 7 ) {					// minimum length
		printf ("Not a RackLink message\n");
		return(1);
	}
	*datasize = (int)(*(raw+1));			// datasize

	packetsize = *datasize + 2;
	// check checksum
	if (packetsize > 0) {
		for ( i=0; i<packetsize; i++ ) {
			sum+=*(raw+i);
		}
		chksum = sum & 0x7f;
	}
	if ( chksum != *(raw+packetsize) ) {	// checksum
		printf ("Bad message checksum, calculated = 0x%02x, read = 0x%02x\n",chksum , *(raw+packetsize) );
		return(2);
	}
	if ( *raw != 0xfe ) {					// header
		printf ("Bad RackLink message header\n");
		return(3);
	}
	if ( *(raw+packetsize+1) != 0xff ) {	// tail
		printf ("Bad RackLink message tail character\n");
		return(4);
	}
	if ( *(raw+1) != (unsigned char)( len - 4 ) ) {	// length
		*nextpacket = raw + packetsize + 2;
	}


	datapacket = raw + 2;
	*destination = (int)(*datapacket);		// destination
	datapacket++;
	*command = (int)(*datapacket);			// command
	datapacket++;
	*subcommand = (int)(*datapacket);		// sub command
	datapacket++;
	*data = datapacket;						// data

	if ( *nextpacket ) return(-1);
	return(0);
}

char * p2str ( char **str, void * p ) {
	if ( p == NULL ) {
		strcpy(*str,(char *)"--null--");
		return(*str);
	}
	sprintf(*str,"%p",(void *)p);
	return(*str);
}

char * str2str ( char **str, char ** p , int maxlen) {
	if ( *p == NULL ) {
		strcpy(*str,(char *)"--null--");
		return(*str);
	}
	strncpy(*str,*p,maxlen);
	*(str+maxlen) = '\000';
	return(*str);
}


int Dump_af_client ( af_client_t * af , char * af_name , char * routine) {
	char strd[256];
	char * strp = strd;
	char * stratch;
/*	struct _af_client_s
	{
		char                *service;  // Specifiy /etc/services name
		int                  port;     // TCP port
		unsigned int         ip;       // Remote IP

		int                  sock;     // Connection

		// Prompt detection
		char                 prompt[MAX_PROMPT];
		int                  prompt_len;
		char                 saved[MAX_PROMPT];
		int                  saved_len;
		void				*extra_data;
		int					 filter_telnet;

		struct _af_client_s *next;

	}; */
	fprintf (stderr,"Dumping struct _af_client \"%s\" from %s, address is %s :\n", af_name, routine, p2str( &strp, (void*)af ));
	if ( af == NULL ) return(1);
	fprintf (stderr,"  {\n\t.service = \"%s\"\n",str2str(&strp,&af->service,254));
	fprintf (stderr,"\t.port = %i\n",af->port);
	fprintf (stderr,"\t.ip = %u\n",af->ip);
	fprintf (stderr,"\t.sock = %i\n",af->sock);
	stratch = af->prompt;
	fprintf (stderr,"\t.prompt = \"%s\"\n",str2str(&strp,&stratch,MAX_PROMPT));
	fprintf (stderr,"\t.prompt_len = %i\n",af->prompt_len);
	stratch = af->saved;
	fprintf (stderr,"\t.saved = \"%s\"\n",str2str(&strp,&stratch,MAX_PROMPT));
	fprintf (stderr,"\t.saved_len = %i\n",af->saved_len);
	fprintf (stderr,"\t.extra_data = \"%s\"\n",p2str(&strp,af->extra_data));
	fprintf (stderr,"\t.filter_telnet = %i\n",af->filter_telnet);
	fprintf (stderr,"\t.next = \"%s\"\n  }\n",p2str(&strp,af->next));

	return(0);
}
