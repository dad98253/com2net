
#include "appf.h"
#include <termios.h>
#include "com2net.h"

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
	af_client_t *client;	/* af_client_t struct */
	int busy;				/* server state: busy after command sent, idle once prompt detected */
	unsigned int connect_timo;	/* connect timeout */
	unsigned int cmd_timo;		/* command timeout */
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

extern int termios(int fd);

extern int af_client_start( comport *coms );
extern void handle_server_socket_event( af_poll_t *af );

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
		if ( strncmp( dev, "/dev", 4 ) != 0 )
		{
			printf( "Bad device %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		speed = atoi( ptr );
		if ( speed <= 0 )
		{
			printf( "Bad speed %s\n", ptr );
			continue;
		}
		ptr = strtok( NULL, ", \t\n\r" );
		if ( ptr )
		{
			logfile = NULL;
			logfile = strdup(ptr);
			logfh = fopen( logfile, "a+" );
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
			ptr = strtok( NULL, ",\t\n\r" );
		}
		else
		{
			remote = NULL;
		}
		if ( ptr )
		{
			prompt = strdup(ptr);
			ptr = strtok( NULL, ", \t\n\r" );
		}
		else
		{
			prompt = NULL;
		}
		if ( ptr )
		{
			commands = strdup(ptr);
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
		if ( coms[i].inout ) {
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
		af_log_print(LOG_INFO, "Starting %i connections.",numcoms );
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

	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
				af_log_print( LOG_WARNING, "com port fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );

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
				write( comp->cnx->fd, buf, len );
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
		af_log_print( LOG_ERR, "tcgetattr error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
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
		af_log_print( LOG_ERR, "tcsetattr error %d (%s) on file descriptor %i", errno, strerror(errno), fd );
		return -1;
	}

	return 0;
}

int open_comport( comport *comp )
{
	comp->fd = open( comp->dev, O_RDWR | O_NOCTTY | O_SYNC );
	if ( comp->fd < 0 )
	{
		af_log_print( LOG_ERR, "Failed to open com port %s, errno %d (%s)", comp->dev, errno, strerror(errno) );
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
	TELNET_SUBOPT = 3
};

int com_filter_telnet( void *extra, unsigned char *buf, int len )
{
	comport *comp = (comport*) extra;
	int   i;
	unsigned char  obuf[2048];
	char scratch[50];
	int   olen = 0;
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
				break;
			case 253:	// DO (option code)
				strcat(comp->decoded,(char*)"Do " );
				comp->telnet_state = TELNET_OPT;
				break;
			case 252:	// WON'T (option code)
				strcat(comp->decoded,(char*)"Won\'t " );
				comp->telnet_state = TELNET_OPT;
				break;
			case 251:	// WILL (option code)
				strcat(comp->decoded,(char*)"Will " );
				comp->telnet_state = TELNET_OPT;
				break;
			case 250:	// SB (Indicates that what follows is subnegotiation of the indicated option)
				strcat(comp->decoded,(char*)"SB  (sub start) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_SUBOPT;
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
				break;
			case 1:
				//	Echo
				snprintf(scratch,50,"Echo ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 2:
				//	Reconnection
				snprintf(scratch,50,"Reconnection ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 3:
				//	Suppress Go Ahead
				snprintf(scratch,50,"Suppress Go Ahead ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 5:
				//	Status
				snprintf(scratch,50,"Status ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 17:
				//	Extended ASCII
				snprintf(scratch,50,"Extended ASCII ");
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			case 18:
				// logout
				strcat(comp->decoded,(char*)"logout  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			case 24:
				// Terminal Type
				strcat(comp->decoded,(char*)"Terminal Type  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			case 32:
				// Terminal Speed
				strcat(comp->decoded,(char*)"Terminal Speed  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			case 34:
				// Linemode
				strcat(comp->decoded,(char*)"Linemode  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			case 35:
				//  Display Location
				strcat(comp->decoded,(char*)"X Display Location  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			case 39:
				//  New Environment
				strcat(comp->decoded,(char*)"New Environment  " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				*(comp->decoded) = '\000';
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client connected to %s has requested log out", comp->dev  );
				if ( comp->cnx != NULL ) af_server_disconnect(comp->cnx);
				comp->telnet_state = TELNET_NONE;
				break;
			default:
				snprintf(scratch,50,"option 0x%x ", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
				break;
			}
			break;
		case TELNET_SUBOPT:
			if ( buf[i] == 240 )	// SE (End of subnegotiation parameters)
			{
				strcat(comp->decoded,(char*)"SE (sub end) " );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
			} else {
				snprintf(scratch,50,"Sub opt data: 0x%x", buf[i]);
				strcat(comp->decoded,scratch );
				af_log_print(APPF_MASK_SERVER+LOG_INFO,"%s", comp->decoded);
				comp->telnet_state = TELNET_NONE;
				*(comp->decoded) = '\000';
			}
			break;
		}
	}
	memcpy( buf, obuf, olen );

	return olen;
}

void com_handle_event( af_poll_t *ap )
{
	int              len = 0;
	unsigned char    buf[2048];
	af_server_cnx_t *cnx = (af_server_cnx_t *)ap->context;
	comport *comp = (comport *)cnx->user_data;


	if ( ap->revents & POLLIN )
	{
		len = read( ap->fd, buf, sizeof(buf)-1 );

		// Handle read errors
		if ( len <= 0 )
		{
			if ( errno != EAGAIN || len == 0 )
			{
				af_log_print( APPF_MASK_SERVER+LOG_INFO, "client fd %d closed: errno %d (%s)",\
					ap->fd, errno, strerror(errno)  );

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

			write( comp->fd, buf, len );
		}
	}
	else if ( ap->revents )
	{
		// Anything but POLLIN is an error.
		af_log_print( APPF_MASK_SERVER+LOG_INFO, "dcli socket error, revents: %d", ap->revents );
		af_server_disconnect(cnx);
	}
}

void com_new_cnx( af_server_cnx_t *cnx, void *context )
{
	char         buf[128];
	comport     *comp = (comport *)context;

	// If another client is connected, get rid of him.
	if ( comp->cnx != NULL )
	{
		fprintf( comp->cnx->fh, "Someone connected from another server\r\n\n\nBYE!\r\n" );

		af_server_disconnect(comp->cnx);
	}

	// get rid of the stock handler
	af_poll_rem( cnx->fd );

	// Open the comport if it is not open
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

	if (af_poll_add( comp->fd, POLLIN, com_port_handler, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for event = 0x%x", __func__, comp->fd, POLLIN );

	if ( cnx->inout ) {
		if (af_poll_add( cnx->fd, POLLIN, handle_server_socket_event, comp ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for event = 0x%x", __func__, cnx->fd, POLLIN );
	} else {
		if (af_poll_add( cnx->fd, POLLIN, com_handle_event, cnx ) == 0) af_log_print( LOG_DEBUG, "%s: polling %d for event = 0x%x", __func__, cnx->fd, POLLIN );
	}
	// Set user data.
	cnx->user_data = comp;
	cnx->disconnect_callback = com_del_cnx;

	// see RFC 2355 and RFC 854:
	buf[0] = 0xff;
	buf[1] = 0xfb;	// Will
	buf[2] = 0x03;	// Confirm willingness to negotiate suppress go ahead
	buf[3] = 0xff;
	buf[4] = 0xfb;	// Will
	buf[5] = 0x01;	// Confirm willingness to negotiate echo
	buf[6] = 0xff;
	buf[7] = 0xfe;	// Don't
	buf[8] = 0x01;	// Confirmation that you are no longer expecting the other party to echo
	buf[9]  = 0xff;
	buf[10] = 0xfd;	// Do
	buf[11] = 0x00;	// Confirmation that you are expecting the other party to use Binary transmission

	write( cnx->fd, buf, 12 );	// going out to the newly connected telnet client
	
}

void com_handler( char *cmd, af_server_cnx_t *cnx )
{
}

