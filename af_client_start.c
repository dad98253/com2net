#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

/*
 * af_client_start.c
 *
 *  Created on: Apr 21, 2023
 *          By: John Kuras
 *    Based on: af_server_start (by Colin Whittaker)
 */
//
// So, what I'm looking for here is something that will replace the following
// for use in my ham shack:
//     socat /dev/ttyS0 TCP4:dxc.kb2s.net:7300
// I have out-of-date logging software running on an long unsupported version
// of Windows 95. I need to trick Windows into thinking that I still have a TNC
// connected to it (my local DX cluster, ve7cc, went dark several years ago).
// Now his data stream is only available via telnet on the web. I also have an
// old linux firewall machine in my shack which runs Red Hat 7. My plan is to
// nul modem wire the serial ports of the two machines together, connect to
// the telnet site on the linux box and pipe the stream out the com port at 2400
// baud. The Windows machine will think it is the data stream from my TNC. In
// theory, that should make my logging software happy. Socat can do this.
// Unfortunately, socat isn't available on Red Hat 7. By turning com2net into
// a telnet client rather than a telnet server, this should do the same thing. The
// tcli app almost gets me there. However, it only writes to stdout. I need
// to send data both ways via the com port...
//

#include "appf.h"
#include "netlink.h"
#include <termios.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "com2net.h"

#define MAX_CMDS		2048
#define MAX_CMD_BUF		4096
#define HEX_PRINT_WIDTH		16
#define DEFAULT_CONNECT_TIMO	5
#define DEFAULT_CMD_TIMO	10
#define DEFAULT_PORT		23
#define MAXDECODE	250
#define TEMPBUFSIZE     512

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

extern tcli_t tcli;

void dump_as_hex(char *buf, int len);
af_server_cnx_t *_af_client_add_connection( comport *coms );
void _af_client_handle_new_connection( comport *coms );
void myexit(int status);
void handle_server_socket_event( af_poll_t *af );
void handle_server_socket_raw_event( af_poll_t *af );

extern int numcoms;
extern comport coms[MAXCOMS];
extern int com_filter_telnet( comport *comp, unsigned char *buf, int len );
extern int af_server_set_sockopts( int s, int server_sock );
extern void _af_server_cnx_handle_event( af_poll_t *ap );
extern int send_client_command(af_client_t *cl, char * prompt, char * command);
extern int process_NetLink_message(af_client_t *cl, char *buf, int *len);

int af_client_start( comport *coms )
{
	af_client_t *client;
	af_server_t *comserver;
	char *service = "telnet";
	unsigned int ip = 0;
	char *default_server_name = (char*)"localhost";
	char *servername = NULL;
	unsigned int addr;
	struct hostent *hp;
	struct sockaddr_in server;
	char *buf;
	unsigned short usport = DEFAULT_PORT;
//	*client = (af_client_t*)(&(coms->comclient));
	client = (af_client_t *)&(coms->comclient);
	comserver = (af_server_t *)&(coms->comserver);

/*
	if ( client->service != NULL )			//////////////  why are we doing this here?????
	{
//		port = af_server_get_port(  );
		port = client->port;
		p = af_server_get_prompt ( client->service );
		if ( port == 0 || p == NULL )
		{
			if ( client->port && client->prompt )
			{
				af_log_print( LOG_NOTICE, "%s not found in /etc/services, adding", client->service );
				_af_server_add_service( client->service, client->port, client->prompt );

			}
			else
			{
				af_log_print( LOG_ERR, "%s not found in /etc/services. (client) Server NOT started", client->service );
				return -1;
			}
		}
		else
		{
			client->port = port;
			strcpy((char *)&(client->prompt), p);
		}
	}
	if ( (client->port == 0) || (client->prompt == NULL) )
	{
		af_log_print( LOG_ERR, "Client port or prompt not found. Client service not started." );
		return -1;
	}

	// Get socket
	if ( ( s = socket( PF_INET, SOCK_STREAM, IPPROTO_TCP ) ) < 0 )
	{
		af_log_print( LOG_ERR, "%s: socket() failed in af_client_start, errno=%d (%s)",
			__func__, errno, strerror(errno) );

		return -1;
	}

	if ( af_server_set_sockopts( s, 1 ) != 0 )
	{
		return -1;
	}
	
	memset( &sin, '\0', sizeof(sin) );

	sin.sin_family = AF_INET;
	if ( 1 )		///////////////////   for testing only!  if ( server->local )
	{
		sin.sin_addr.s_addr = htonl( INADDR_LOOPBACK );
	}
	else
	{
		sin.sin_addr.s_addr = htonl( INADDR_ANY );
	}

	sin.sin_port = htons( client->port );

	if ( bind( s, (struct sockaddr*)&sin, sizeof(struct sockaddr) ) < 0 )
	{
		close( s );

		af_log_print( LOG_ERR, "bind() failed for port %d in af_client_start, fd=%d errno=%d (%s)",
					  client->port, s, errno, strerror(errno) );

		return -1;
	}
// ;;;;;;;;;;;;;;;;;;;;;;;;;;  fix =============================================
	// Listen for incomming connections
	if ( listen( s, coms->comserver.max_cnx ) != 0 )
	{
		close( s );

		af_log_print( LOG_ERR, "%s: listen() failed for fd=%d errno=%d (%s)",
			__func__, s, errno, strerror(errno) );

		return -1;
	}

	coms->comserver.fd = client->sock = s;
//	client->num_cnx = 0;
//	client->cnx = NULL;

	// Add pollfd for new connections
//	af_poll_add( client->sock, (POLLIN|POLLPRI), _af_server_handle_new_connection, (void*)client );
	af_poll_add( coms->comserver.fd, (POLLIN|POLLPRI), _af_server_handle_new_connection, (void*)&(coms->comserver) );
*/


	if ( 1 ) {
		if ( coms->remote == NULL ) {
			servername = default_server_name;
		} else {
			servername = coms->remote;
		}
		//
		// Attempt to detect if we should call gethostbyname() or
		// gethostbyaddr()
		if (isalpha(servername[0])) {   /* server address is a name */
			hp = gethostbyname(servername);
		}
		else  { /* Convert nnn.nnn address to a usable one */
			addr = inet_addr(servername);
			hp = gethostbyaddr((char *)&addr,4,AF_INET);
		}
		memset(&server,0,sizeof(server));
		if (hp == NULL ) {
			int errsv = errno;
			int herrsv = h_errno;
			af_log_print(LOG_INFO,"Client: Cannot resolve address [%s]: Error %d, h_err = %i\n",
				servername,errsv,herrsv);
			af_log_print(LOG_INFO,"%s\n", hstrerror(h_errno));
			if (h_errno == HOST_NOT_FOUND) af_log_print(LOG_INFO,"Note: on linux, ip addresses must have valid RDNS entries\n");
			ip = server.sin_addr.s_addr = inet_addr(servername);
			server.sin_family = AF_INET;
		} else {
			memcpy(&(server.sin_addr),hp->h_addr,hp->h_length);
			server.sin_family = hp->h_addrtype;
			ip = ntohl(server.sin_addr.s_addr);
		}
		if (coms->tcpport) usport = coms->tcpport;
		server.sin_port = htons(usport);
	}

	af_log_print(LOG_INFO, "trying to connect to service: %s at ip = %u, port %u", service, ip, usport);
//	ip = INADDR_LOOPBACK;	// for debug:	test known ip address

	client = af_client_new( service, ip, usport, coms->prompt );
	// af_client_new mallocs a whole new cliet struct. We need to move this over into the coms struct
	coms->comclient = *client;
	// now free the client pointer and point it into coms
	free(client);
	client = &(coms->comclient);
	//set telnet filter option
	client->filter_telnet = 1;
	//set the extra pointer back to the coms struct
	client->extra_data = (void*) coms;

	if (coms->prompt != NULL) {
		af_log_print(LOG_INFO, "client data initialized for connection to server: %s, port %d, prompt \"%s\"", service, coms->tcpport, coms->prompt );
	} else {
		af_log_print(LOG_INFO, "client data initialized for connection to server: %s, port %d", service, coms->tcpport );
	}
/*
	// send the help string if we didn't get a command
	if ( ! strlen(tcli.cmd.buf) && ! strlen(tcli.opt.filename) )
	{
		strcpy(tcli.cmd.buf, "?");
	}

	if ( strlen(tcli.cmd.buf) )
	{
	 	// split the command buffer on delim char
		tcli.cmd.num = tokenize(tcli.cmd.buf, tcli.opt.delim_char, tcli.cmd.part, MAX_CMDS);
	}
	else
	{
		tcli.cmd.num = 0;
	}

	for ( ch=0;ch<tcli.cmd.num;ch++)
	{
		af_log_print(LOG_DEBUG, "tcli cmd[%d]: %s", ch, tcli.cmd.part[ch] );
	}
*/
	// connect to telnet server
	if ( af_client_connect( client ) )
	{
		af_log_print(LOG_ERR, "failed to connect to server (port=%d, socket=%d, prompt=\"%s\")", client->port, client->sock ,client->prompt );
		return -1;
	}
	else
	{
		comserver->fd = client->sock;
		af_log_print(LOG_DEBUG, "connected to server (port=%d,client sock=%d,server fd=%d)", client->port, client->sock, comserver->fd );
	}
/*
	// detect the initial prompt
	if ( af_client_get_prompt( client, coms->connect_timo*1000 ) )	// Note: this is a macro for af_client_read_timeout (see appf.h)
	{
		af_log_print(LOG_ERR, "failed to detect prompt (\"%s\") within timeout (%d secs)", client->prompt, coms->connect_timo );
		return -1;
	}

	// Queue up the commands

	if ( strlen(tcli.opt.filename) )
	{
		if ( (fh=fopen( tcli.opt.filename, "r" )) == NULL )
		{
			af_log_print(LOG_ERR, "failed to open command file %s", tcli.opt.filename );
			myexit(1);
		}

		while ( !feof(fh) )
		{
			if ( fgets( buf, 2048, fh ) == NULL )
				break;

			// Remove new line.
			ptr = strchr( buf, '\n' );
			if ( !ptr )
				continue;
			*ptr = 0;

			// clear white space at the begining of any command
			ptr = buf;
			while ( isspace(*ptr) ) ptr++;

			// check for valid line.
			if ( strlen(ptr) < 1 )
				continue;
			if ( buf[0] == '#' )
				continue;
			if ( buf[0] == ';' )
				continue;

			tcli.cmd.part[tcli.cmd.num++] = strdup( buf );
		}
	}
*/

	_af_client_handle_new_connection( coms );

	// toss a nul down the pipe
//	char nulline[1000];
//	nulline[0] = '\000';	// leave room for the appf library to tack on a \r
//	if ( af_client_send( client, nulline ) ) myexit(1);

	buf = (char *)malloc(TEMPBUFSIZE);
	if ( buf != NULL ) {
		snprintf(buf, TEMPBUFSIZE, "\x1b[2J%s is connected to %s", coms->dev, coms->remote);
		af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "%s", buf );
		strcat(buf,(char*)"\r\n");
		write( coms->fd, buf, strlen(buf) );
		free(buf);
	}
//	af_poll_add( client->sock, POLLIN, handle_server_socket_event, coms );




	return 0;

}

void af_client_stop( af_client_t *client )
{
	af_client_disconnect( client );

	af_poll_rem( client->sock );

	close( client->sock );

	client->sock = -1;
}

void myexit(int status)
{
	int i;
	for ( i=0; i<numcoms; i++ ) {
		if (coms[i].inout) {
			// close connection to server
			af_client_delete( &(coms[i].comclient) );
		} else {
			// close connection to server
			af_server_stop( &(coms[i].comserver) );
		}
	}
	af_log_print(LOG_DEBUG, "exiting status %d", status);
	exit(status);
}

#define MAX_SOCK_READ_BUF 4096

void handle_server_socket_event( af_poll_t *af )
{
	int status;
	char buf[MAX_SOCK_READ_BUF];
	int len = MAX_SOCK_READ_BUF;
	comport *coms;
	coms = (comport*) (af->context);

	//af_log_print(LOG_DEBUG, "%s: revents %d", __func__, revents);

	// Read with 1 ms timeout since we are using the poll loop to get here.
	status = af_client_read_timeout( &(coms->comclient), buf, &len, 1 );


//	af_log_print( LOG_DEBUG, "%s: client_read() returned %d, len=%d", __func__, status, len );

	// Handle status
	switch ( status )
	{
	case AF_TIMEOUT:
		// Still going.. no PROMPT yet.
		break;
	case AF_BUFFER:
			// read buffer full, poll will fire us again immediately to read the rest
		break;
	case AF_SOCKET:
		//  DCLI server probably died...
		tcli.bailout = TRUE;
		break;
	case AF_OK:
		// tcli prompt was detected
		tcli.conn.busy = FALSE;
		tcli.cmd.current++;
		// reply with the appropriate command...
		send_client_command(&(coms->comclient), coms->prompt, coms->commands);
		break;
	default:
		af_log_print(LOG_ERR, "%s: oops, unexpected status %d from client_read", __func__, status);
		break;
	}

	// Handle any data we read from the sock
	if ( len > 0 )
	{
		buf[len] = 0;
		af_log_print(LOG_DEBUG, "%s: server rx %d bytes", __func__, len);

		if (tcli.opt.dump_hex == TRUE)
		{
			dump_as_hex( buf, len );
			fflush(stdout);
		}
		else
		{
			printf("%s",buf);
			fflush(stdout);
		}
	}
}

void handle_server_socket_raw_event( af_poll_t *af )
{
	int status;
	int i;
	char buf[MAX_SOCK_READ_BUF];
	int len = MAX_SOCK_READ_BUF;
	comport *coms;
	coms = (comport*) (af->context);

	//af_log_print(LOG_DEBUG, "%s: revents %d", __func__, revents);

	// Read with 1 ms timeout since we are using the poll loop to get here.
	status = af_client_read_raw_timeout( &(coms->comclient), buf, &len, 1 );


//	af_log_print( LOG_DEBUG, "%s: client_read() returned %d, len=%d", __func__, status, len );

	// Handle status
	switch ( status )
	{
	case AF_TIMEOUT:
		// Still going.. no PROMPT yet.
		break;
	case AF_BUFFER:
			// read buffer full, poll will fire us again immediately to read the rest
		break;
	case AF_SOCKET:
		//  DCLI server probably died...
		tcli.bailout = TRUE;
		len = 0;
		break;
	case AF_OK:
		// tcli prompt was detected
		tcli.conn.busy = FALSE;
		tcli.cmd.current++;
		// reply with the appropriate command...
//		send_client_command(&(coms->comclient), coms->prompt, coms->commands);
		break;
	default:
		af_log_print(LOG_ERR, "%s: oops, unexpected status %d from client_read", __func__, status);
		tcli.bailout = TRUE;
		len = 0;
		break;
	}

	// Handle any data we read from the sock
	if ( len > 0 ) {
		printf("received %i bytes : (0x)",len);
		for ( i = 0; i < len; i++) {
			printf(" %02x", (unsigned char)(*(buf+i)));
		}
		printf("\n");
		fflush(stdout);
		process_NetLink_message(&(coms->comclient), buf, &len);
	}
}

int send_client_command(af_client_t *fd, char * prompt, char * command)
{
	int status;
	int exitval;

//	af_log_print(LOG_DEBUG, "%s: sending tcli command \"%s\"", __func__, tcli.cmd.part[tcli.cmd.current] );
	af_log_print(LOG_DEBUG, "%s: sending tcli command \"%s\"", __func__, command );

	// show user the command we're sending unless suppressed
	if (tcli.opt.hide_prompt == FALSE)
	if(1)
	{
//		printf("%s%s\n", tcli.conn.client->prompt, tcli.cmd.part[tcli.cmd.current] );
		printf("%s%s\n", prompt, command );
		fflush(stdout);
	}

//	status = af_client_send( tcli.conn.client, tcli.cmd.part[tcli.cmd.current] );
	status = af_client_send(  fd, command );
	status = AF_OK;
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
//		af_log_print(LOG_ERR, "%s: client_send returned %d (cmd=%s)", __func__, status, tcli.cmd.part[tcli.cmd.current] );
		af_log_print(LOG_ERR, "%s: client_send returned %d (cmd=%s)", __func__, status, command );
		exitval = 1;
		break;
	}

	return exitval;
}

void client_com_handle_event( af_poll_t *ap )
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

			af_log_print( APPF_MASK_SERVER+LOG_DEBUG, "telnet sent %d characters to %s -> [%s]", len, comp->dev, buf );

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

/* print a line of data with hex dump */
void dump_hex_line( char *buf, int len )
{
	int j;

	// write line as ascii
	for (j=0; j<len; j++)
	{
		if ( (buf[j] < 32) || (buf[j] > 126) )
			printf(".");
		else
			fputc( buf[j], stdout );
	}

	// spacer
	for (j=0; j<(HEX_PRINT_WIDTH+4-len); j++)
	{
		printf(" ");
	}

	// hex
	for (j=0; j<len; j++)
	{
		printf("%02X ",buf[j] );
	}
	printf("\n");
}

/* dump socket data as ascii and hex */
void dump_as_hex(char *buf, int len)
{
	int done = FALSE;
	int i = 0;

	// format the data for hex code printing
	while (! done)
	{
		if (len > i+HEX_PRINT_WIDTH)
		{
			dump_hex_line(buf+i, HEX_PRINT_WIDTH);
			i += HEX_PRINT_WIDTH;
		}
		else
		{
			dump_hex_line(buf+i, len-i);
			done = TRUE;
		}
	}
}

void _af_client_handle_new_connection( comport *coms )
{
	af_server_t *serv;
	af_server_cnx_t *cnx;
	serv = (af_server_t *)&(coms->comserver);

	if ( ( cnx = _af_client_add_connection( coms ) ) != NULL )
	{
		af_log_print(APPF_MASK_SERVER+LOG_INFO,
                     "accepted new client connection (fd=%d)",
                     cnx->fd);

		/* add new client fd to the pollfd list */
		af_poll_add( cnx->fd, POLLIN, _af_server_cnx_handle_event, cnx );		////////////////////// do we need to do this???

		// Call the user's new connection callback
		if ( serv->new_connection_callback )
		{
			serv->new_connection_callback( cnx, serv->new_connection_context );
		}

		/* send dcli prompt to client */
//		af_server_prompt( cnx );
	}
	else
	{
        /* Mask this - its legal to refuse connections at boot time */
        /* We end up filling processes logs with this if we leave   */
        /* without the mask                                         */
		af_log_print(LOG_ERR,
                     "failed to accept new client connection");
	}
}

af_server_cnx_t *_af_client_add_connection( comport *coms )
{
	af_client_t *client;
	af_server_t *server;

	int                 s, fd_dup;
	af_server_cnx_t    *cnx = NULL;
	struct sockaddr_in  raddr = {};
	client = (af_client_t *)&(coms->comclient);
	server = (af_server_t *)&(coms->comserver);

	s = server->fd;

	if ( s < 0 )
	{
		af_log_print(LOG_ERR, "%s: connect() failed (%d) %s",\
			__func__, errno, strerror(errno) );
		return NULL;
	}

	/* quick check to deny before scanning list */
	if ( server->num_cnx >= server->max_cnx )
	{
		close( s );
		af_log_print(LOG_ERR, "%s: rejecting new client connection: max number of sessions (%d) already open",\
			__func__, server->max_cnx );

		return NULL;
	}

	cnx = (af_server_cnx_t *)calloc( 1, sizeof(af_server_cnx_t) );
	if ( cnx == NULL )
	{
		close(s);
		af_log_print( LOG_ERR, "Failed to allocate memory for new connection");
		return NULL;
	}

	/**
	 * Client socket
	 */
	if ( af_server_set_sockopts( s, 0 ) != 0 )
	{
		close(s);
		free(cnx);
		return NULL;
	}

	/**
	 * Dup client socket for file handle
	 */
	if ( ( fd_dup = dup( s ) ) < 0 )
	{
		close( s );
		free(cnx);
		af_log_print(LOG_ERR, "%s: dup() failed on fd=%d, errno=%d (%s)", __func__, s, errno, strerror(errno) );

		return NULL;
	}

	if ( af_server_set_sockopts( fd_dup, 0 ) != 0 )
	{
		close( s );
		free(cnx);
		return NULL;
	}

	if ( ( cnx->fh = fdopen( fd_dup, "w+"  ) ) == NULL )
	{
		close( s );
		close( fd_dup );
		free(cnx);

		af_log_print( LOG_CRIT, "%s: fdopen() failed for fd=%d", __func__, fd_dup );

		return NULL;
	}

	/* set handle to line buffered mode */
	setlinebuf( cnx->fh );

	cnx->fd = s;
	cnx->raddr = raddr;
	cnx->server = server;
	cnx->client = client;
	cnx->inout = coms->inout;

	// Add to the server list
	cnx->next = server->cnx;
	server->cnx = cnx;
	server->num_cnx++;

	return cnx;
}
