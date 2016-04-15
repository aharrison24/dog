/*
 *                  dog - better than cat
 *                 Copyright (C) 1999-2000
 *            Jason Cohen <dogboy@photodex.com>
 *         Jacob Leverich <leverich@photodex.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

// includes
#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "getopt.h"
#include <limits.h>
#include <netdb.h>

// this is fun
#if defined(__alpha)
#define uint16	unsigned short
#define int16	short
#define uint32	unsigned int
#define int32	int
#define uint64	unsigned long
#define int64	long
#else
#define uint16	unsigned short
#define int16	short
#define uint32	unsigned long
#define int32	long
#define uint64	unsigned long long
#define int64	long long
#endif
#if defined(__STDC__)
#define __dog_signed		signed
#else
#deifne __dog_signed
#endif

// config
#define MAJOR_VERSION			1
#define MINOR_VERSION			7

#define DATA_BUFFER_SIZE		(64*1024)

#define FS_WHITESPACE			0
#define FS_PUNCTUATION			1

#define LS_LEAVEIT				0
#define LS_UNIX					1
#define LS_DOS						2
#define LS_MAC						3

#define UNIX_TERM					0x0a
#define MAC_TERM					0x0d

// config based on operating system and compiler
#if defined(__linux) || defined(__linux__)
#define ALLOW_LINUX_EXTENSIONS		1
#else
#define ALLOW_LINUX_EXTENSIONS		0
#endif

#if ALLOW_LINUX_EXTENSIONS
#define ALLOW_STRFRY			1
extern char *strfry __P ((char *__string));
#else
#define ALLOW_STRFRY			0
#endif

// types
typedef struct LineRange {
	unsigned long			start,end;				// inclusive
	struct LineRange		*next;					// next range in the list
} LineRange;

typedef struct InternetSockAddressStruct {
	unsigned short	family;			/* type of connection - unused! */
	unsigned short	port;				/* connection port */
	uint32	addr;				/* host IP number */
	unsigned char 	padding[8];		/* padding */
} InetSockAddr;

// globals
static char data_buffer[DATA_BUFFER_SIZE];
static char output_buffer[DATA_BUFFER_SIZE*4];
//static char **line_buffer;
static unsigned long lines = 0;
static int line_count = 0;							// line counter
static int last_line_was_blank = 0;
static int emit_line = 1;							// general switch to emit a line
static int found_header;
static int byte_count = 0;

// preferences
static int straight_dump = -1;					// if not -1, dump input to this file descriptor without processing
static int print_line_numbers = 0;				// print the line number before each line
static int print_nonblank_line_numbers = 0;	// print line numbers even for non-blank lines
static int print_trailing_dollarsign = 0;		// print trailing dollar signs on each line
static int only_one_blank_line = 0;				// skip lines if more than one blank in a row
#if ALLOW_STRFRY
static int opt_strfry = 0;							// strfry() each line
#endif
static int opt_rotate = 0;							// character rotation
static int opt_tolower = 0;						// lowercase everything
static int opt_toupper = 0;						// uppercase everything
static int opt_krad = 0;							// change line to k-rad
static int show_nonprinting = 0;					// show non-printing characters
static int max_chars_per_line = 0;				// maximum number of characters per line, or 0 for unlimited
static int leave_tabs = 1;							// leave tabs be during a -v
//static int need_line_buffers = 0;				// set if, for any reason, we have to buffer lines
static int no_bind_header = 0;					// do not print the connection binding header
static int no_url_header = 0;						// do not print any URL header
static int use_udp = 0;								// use UDP instead of TCP on socket connections
static int hang_up_bind = 0;						// hand up immediatly during --bind
static int no_blanks = 0;							// don't print blank-only lines
static int sock_test = 0;							// just test for connection in --sock
static int http_links = 0;							// in http://, list links
static int image_links = 0;						// in http://, list images
static int raw_data = 1;							// data should be treated raw
static int hide_nonprinting = 0;					// hide non-printing chars
static int line_style = LS_LEAVEIT;				// output line style
static int skip_tags = 0;							// skip HTML tags in processing
static int hex = 0;									// output as a hex dump
static int oog = 0;									// OOG A STRING!!!
static char socket_domain_str[128]="";			// set if socket domain is specified
static char field_sep = FS_WHITESPACE;			// field separator
static LineRange *field_range = NULL;			// stack of field ranges
static LineRange *line_range = NULL;			// stack of line ranges
static char *usage_strs[] = {
			"-A, --show-all","equivalent to -vET",
			"-b, --number-nonblank","precede each non-blank line with its line number",
			"-B, --no-blanks","only print lines with non-whitespace characters",
			"    --bind=port","dump each connection to port, print connection output",
			"    --dos","convert line endings to DOS-style",
			"-e","equivalent to -vE",
			"-E, --show-ends","display $ at the end of each line",
/*			"-F=char","set field separator character (also 'white' or 'punct')",*/
			"    --hang-up","do not wait for socket input during --bind",
			"    --hide-nonprinting","hide non-printing characters",
			"    --help","display this help message and exit",
			"    --hex","display the data as a hex dump",
			"    --images","list unique, absolute image links from input data",
			"    --krad","convert lines to k-rad",
			"-l lines","list of lines to print, comma delimited, ranges allowed",
			"    --links","list unique, absolute URL links from input data",
			"    --lower","convert all upper-case characters to lower",
			"    --mac","convert line endings to Macintosh-style",
			"-n, --number","precede each line with its line number",
			"    --no-header","do not dump out the header on each connection in --bind,\n                        and don't dump header in URL data",
			"    --oog","OOG A STRING!!!",
			"    --rot=num","rotate character values (can be negative)",
			"-s, --squeeze-blank","never more than one single blank link",
			"    --strfry","stir-fry each line (Linux only)",
			"    --sock=domain:port","connect, dump input to socket, print socket output",
			"    --sock-test","with --sock, print if connection can be made",
			"-t","equivalent to -vT",
			"-T, --show-tabs","display TAB characters as ^I",
			"    --skip-tags","do not process HTML tags from input, and simply output them as-is",
			"    --translate","translate end-of-line characters",
			"-u","ignored",
			"    --udp","use UDP rather than TCP on socket activity",
			"    --unix","convert line endings to UNIX-style",
			"    --upper","convert all lower-case characters to upper",
			"-v, --show-nonprinting","use ^ and M- notation, except for TAB",
			"-w[cols]","print first 'cols' characters of each line (default=80)",
			"    --version","output version information and exit"
};
static char *optstring = "AbBeEF:hl:nstTuvw::";

#define OPT_STRFRY		-66
#define OPT_ROT			-67
#define OPT_LOWER			-68
#define OPT_UPPER			-69
#define OPT_VERSION		-70
#define OPT_HELP			-71
#define OPT_KRAD			-72
#define OPT_SOCK			-73
#define OPT_BIND			-74
#define OPT_NOHEADER		-75
#define OPT_UDP			-76
#define OPT_HANGUP		-77
#define OPT_SOCKTEST		-78
#define OPT_LINKS			-79
#define OPT_IMAGES		-80
#define OPT_UNIX			-81
#define OPT_MAC			-82
#define OPT_DOS			-83
#define OPT_RAW			-84
#define OPT_HIDENONP		-85
#define OPT_SKIPTAGS		-86
#define OPT_HEXDUMP		-87
#define OPT_OOG			-88
static struct option options[] = {
#if ALLOW_STRFRY
			{	"strfry",		no_argument,NULL,OPT_STRFRY },
#endif
			{	"rot",			required_argument,NULL,OPT_ROT },
			{	"lower",			no_argument,NULL,OPT_LOWER },
			{	"upper",			no_argument,NULL,OPT_UPPER },
			{	"version",		no_argument,NULL,OPT_VERSION },
			{	"help",			no_argument,NULL,OPT_HELP },
			{	"krad",			no_argument,NULL,OPT_KRAD },
			{	"sock",			required_argument,NULL,OPT_SOCK },
			{	"bind",			required_argument,NULL,OPT_BIND },
			{	"hang-up",		no_argument,NULL,OPT_HANGUP },
			{	"no-header",	no_argument,NULL,OPT_NOHEADER },
			{	"udp",			no_argument,NULL,OPT_UDP },
			{	"sock-test",	no_argument,NULL,OPT_SOCKTEST },
			{	"links",			no_argument,NULL,OPT_LINKS },
			{	"images",		no_argument,NULL,OPT_IMAGES },
			{	"unix",			no_argument,NULL,OPT_UNIX },
			{	"dos",			no_argument,NULL,OPT_DOS },
			{	"mac",			no_argument,NULL,OPT_MAC },
			{	"translate",	no_argument,NULL,OPT_RAW },
			{	"hide-nonprinting",no_argument,NULL,OPT_HIDENONP },
			{	"show-all",		no_argument,NULL,'A' },
			{	"number-nonblank",no_argument,NULL,'b' },
			{	"no-blanks",	no_argument,NULL,'B' },
			{	"show-ends",	no_argument,NULL,'E' },
			{	"number",		no_argument,NULL,'n' },
			{	"squeeze-blank",no_argument,NULL,'s' },
			{	"show-tabs",	no_argument,NULL,'T' },
			{	"show-nonprinting",no_argument,NULL,'v' },
			{	"skip-tags",no_argument,NULL,OPT_SKIPTAGS },
			{	"hex",no_argument,NULL,OPT_HEXDUMP },
			{	"oog",no_argument,NULL,OPT_OOG },
			{	NULL,				no_argument,NULL,0 },
};

static char *krad_conversion[] = {
			"0","o",
			"1","l",
			"a","4",
			"ate","8",
			"e","3",
			"b","6",
			"l","1",
			"o","0",
			"s","5",
			"see","C",
			"t","7"
};

static char* stristr(const char *data,const char *target)
{
	int ch,k;

	ch = tolower(*target);
	while (*data) {
		if (tolower(data[0]) == ch) {
			for(k=1;data[k]&&target[k];k++) {
				if (tolower(data[k]) != tolower(target[k])) {
					goto next_char;
				}
			}
			return (char*)data;
		}
next_char:
		data++;
	}

	return (char*)NULL;
}

static void strnins(char *str,const char *data,int len)
{
	memmove(str+len,str,strlen(str)+1);
	memcpy(str,data,len);
}

static void strins(char *str,const char *data)
{
	strnins(str,data,strlen(data));
}

static const char *OOG_NO_SAY[] = {
	"IS","ARE","AM","A","AN","THAT","WHICH","THE","CAN","OUR","ANY","HIS","HERS"
};

static struct { const char *A,*B; } OOG_SAY_DIFFERENT[] = {
	{ "I","OOG" },
	{ "IM","OOG" },
	{ "ME","OOG" },
	{ "MY","OOG'S" },
	{ "MINE","OOG'S" },
	{ "ANOTHER","OTHER" },
	{ "HAS","HAVE" },
	{ "HAD","HAVE" },
	{ "CANNOT","NOT" }
};

// OOG A STRING!!!
static void OOG(char *IN,FILE *OUT)
{
	char *A,CH;
	int K;
	int NEED_WHITE,CONTRACTION;
	
	// OOG START THINGS OUT RIGHT!!!
	NEED_WHITE = 0;
	
	// OOG LOOK AT STRING!!!
	while (IN[0]) {
	
		// OOG NO CARE ABOUT WHITESPACE!!!
		if (isspace(IN[0])) {
			if (NEED_WHITE || IN[0]=='\n' || IN[0]=='\r') {
				NEED_WHITE = 0;
				fputc(*IN,OUT);
			}
			IN++;
			continue;
		}
		
		// OOG TERMINATE SENTENCES IN OOG'S OWN WAY!!!
		if (IN[0] == '.' || IN[0] == ';' || IN[0] == ':') {
			IN++;
			fprintf(OUT,"!!!");
			NEED_WHITE = 1;
			continue;
		}
		
		// SOMETIMES OOG NEED MORE PUNCTUATION!!!
		if (IN[0] == '!' || IN[0] == '?') {
			fprintf(OUT,"%c%c%c",IN[0],IN[0],IN[0]);
			NEED_WHITE = 1;
			IN++;
			continue;
		}
		
		// OOG NO CARE ABOUT SOME PUNCTUATION!!!
		if (IN[0] == ',' || IN[0] == '\'') {
			IN++;
			NEED_WHITE=1;
			continue;
		}
		
		// OOG OK WITH OTHER PUNCTUATION!!!
		if (!isalpha(IN[0])) {
			fputc(*IN++,OUT);
			NEED_WHITE=1;
			continue;
		}
		
		// OOG WANT KNOW NEXT TOKEN!!!
		CONTRACTION = 0;
		for(A=IN;isalpha(A[0]) || A[0]=='\'';A++) {
			if (isalpha(A[0])) {
				A[0] = toupper(A[0]);	// OOG NO LIKE LOWERCASE!!!
			} else {
				CONTRACTION = 1;
				memmove(A,A+1,strlen(A));
				A--;
			}
		}
		CH = A[0];		// OOG MAKE TOKEN THE WHOLE STRING TEMPORARILY!!!
		A[0] = 0;
		
		// OOG SEE IF CONTRACTION NEED CHANGE!!!
		if (CONTRACTION) {
			if (!strcmp(A-1,"S")) {
				A[-1] = 0;
			}
			if (!strcmp(A-2,"RE")) {
				A[-2] = 0;
			}
			if (!strcmp(A-2,"VE")) {
				A[-2] = 0;
			}
			if (!strcmp(A-2,"NT")) {
				strcpy(IN,"NOT");
			}
		}
		
		// OOG SEE IF NOT IN OOG'S VOCABULARY!!!
		for(K=sizeof(OOG_NO_SAY)/sizeof(char*);K--;) {
			if (!strcmp(OOG_NO_SAY[K],IN)) {
				goto next_token;
			}
		}
		
		// OOG SEE IF OOG SAY DIFFERENT!!!
		for(K=sizeof(OOG_SAY_DIFFERENT)/(sizeof(char*)*2);K--;) {
			if (!strcmp(OOG_SAY_DIFFERENT[K].A,IN)) {
				fprintf(OUT,"%s",OOG_SAY_DIFFERENT[K].B);
				NEED_WHITE = 1;
				goto next_token;
			}
		}
		
		// OOG CONSIDER SELF!!!
		if (!strcmp(IN+strlen(IN)-4,"SELF") && strcmp(IN,"ITSELF")) {
			strcpy(IN,"SELF");
		}
		if (!strcmp(IN+strlen(IN)-6,"SELVES")) {
			strcpy(IN,"SELF");
		}
		
		// OOG HAPPY WITH TOKEN
		fprintf(OUT,"%s",IN);
		NEED_WHITE = 1;
		
		// OOG WANT PUT BACK STRING WAY OOG FOUND IT!!!
next_token:
		A[0] = CH;
		IN = A;
	}
}

#define WRITE(_buf,_len)		if (fwrite((_buf),(_len),1,stdout) < 0) return -1
static int ProcessLine(char *str,int str_len,unsigned long line,int skip_newline,int apply_start)
{
	char *m,*s,ch;
	int n,max_chars_per_line_local;
	
	// check for amps
	if (skip_tags) {
next_amp:
		m = memchr(str,'&',str_len);
		if (m) {
			m[0] = 0;
			ProcessLine(str,m-str,line,1,apply_start);
			m[0] = '&';
			apply_start = 0;
			s = memchr(m,';',str_len-(m-str));
			if (s && s-m < 10) {
				WRITE(m,s-m+1);
			} else {
				s = m;
			}
			str = s+1;
			str_len -= (s+1)-str;
			goto next_amp;
		}
	}

	// update line number
	lines++;
reprocess:
	max_chars_per_line_local = max_chars_per_line - print_trailing_dollarsign;
	
	// skip lines trivially
	if (!emit_line) {
		return 0;
	}

	// skip lines out-of-range
	if (apply_start && line_range) {
		if (lines < line_range->start) {
			return 0;
		}
		if (lines > line_range->end) {
			LineRange *p;

			p = line_range->next;
			free(line_range);
			line_range = p;
			if (!line_range) {
				emit_line = 0;
			}
			goto reprocess;
		}
	}

	// skip consecutive blank lines
	if (!str_len && last_line_was_blank && only_one_blank_line) {
		return 0;
	}

	// skip header
	if (apply_start && no_url_header && !found_header) {
		if (!str_len) {
			found_header = 1;
		}
		return 0;
	}
	
	// skip blank lines
	if (no_blanks) {
		for(m=str;*m;m++) {
			if (!isspace(*m)) {
				break;
			}
		}
		if (!*m) {
			return 0;
		}
	}
	
	// update line counter
	if (apply_start) {
		line_count++;
	}

	// precede with the line number
	if (apply_start && print_line_numbers && (str_len || print_nonblank_line_numbers)) {
		char buf[16];

		WRITE(buf,sprintf(buf,"%6d  ",line_count));
      max_chars_per_line_local -= strlen(buf);
	}
	
	// check for tags
	if (skip_tags) {
		char *tag;
		
		while ((tag=strchr(str,'<')) != NULL) {
			tag[0] = 0;
			ProcessLine(str,tag-str,line,1,0);
			str = tag+1;
			tag = strchr(str,'>');
			
		}
	}

	// strip non-printing characters
	if (hide_nonprinting) {
		for(m=str;*m;m++) {
			if (!isprint(*m)) {
				for(s=m;s[1];s++) {
					s[0] = s[1];
				}
				s[0] = 0;
				str_len--;
				m--;
			}
		}
	}
	
	// OOG!!!
	if (oog) {
		OOG(str,stdout);
		goto end_the_line;
		return 0;
	}
	
	// strfry()
#if ALLOW_STRFRY
	if (opt_strfry) {
		strfry(str);
	}
#endif

	// munge characters individually in the string
	if (opt_rotate || opt_tolower || opt_toupper) {
#define MOD_26(_n)			((((_n) % 26) + 26) % 26)
		for(m=str;*m;m++) {
			ch = m[0];
			if (islower(ch)) {
				if (opt_rotate) {
					ch = MOD_26(((ch-'a') + opt_rotate)) + 'a';
				}
				if (opt_toupper) {
					ch = toupper(ch);
				}
			} else if (isupper(ch)) {
				if (opt_rotate) {
					ch = MOD_26(((ch-'A') + opt_rotate)) + 'A';
				}
				if (opt_tolower) {
					ch = tolower(ch);
				}
			}
			m[0] = ch;
		}
	}
	
	// display non-printing characters
	if (show_nonprinting) {
		for(m=str,s=output_buffer,n=str_len;n--;) {
			ch = *m++;
			if (ch & 0x80) {
				*s++ = 'M';
				*s++ = '-';
				ch &= 0x7f;
			}
		        if (ch == 127) {
			   	*s++ = '^';
			   	ch = '?';
			}
			if (ch < 0x20) {
				if (ch != 0x09 || !leave_tabs) {
					*s++ = '^';
					ch += 'A'-1;
				}
			}
			*s++ = (char)ch;
		}
		*s = 0;
		str = output_buffer;
		str_len = s - str;
	}

	// k-rad-ify lines
	if (opt_krad && str != output_buffer) {
		for(m=str,s=output_buffer;*m;) {
			if (rand() & 0x1) {
				*s++ = *m++;
				goto krad_next_char;
			}
			for(n=(sizeof(krad_conversion)/sizeof(char*))/2;n--;) {
				if (!strncmp(krad_conversion[n*2],m,strlen(krad_conversion[n*2]))) {
					strcpy(s,krad_conversion[n*2+1]);
					while (*s) {
						s++;
					}
					m += strlen(krad_conversion[n*2]);
					goto krad_next_char;
				}
			}
			ch = *m++;
			if (isupper(ch)) {
				ch = tolower(ch);
			} else {
				ch = toupper(ch);
			}	
			*s++ = ch;
krad_next_char:
			;
		}
		*s = 0;
		str = output_buffer;
		str_len = s - str;
	}

	// write the line itself
	if (str_len) {
		if (max_chars_per_line) {
			for(m=str;*m;m++) {
				if (m[0] == '\t') {
					max_chars_per_line_local -= 7;
				}
			}
			if (max_chars_per_line_local && str_len > max_chars_per_line_local) {
				str_len = max_chars_per_line_local;
			}
		}
		if (str_len > 0) {
			WRITE(str,str_len);
		}
	}
end_the_line:

	// trail the line with a '$'
	if (print_trailing_dollarsign && !skip_newline) {
		WRITE("$",1);
	}

	// end the line
	if (!skip_newline) {
		switch (line_style) {

			case LS_LEAVEIT:
			case LS_UNIX:
				WRITE("\n",1);
				break;

			case LS_DOS:
				WRITE("\r\n",2);
				break;

			case LS_MAC:
				WRITE("\r",1);
				break;
		}
	}

	// update state information
   last_line_was_blank = (str_len < 1);
	return 0;
}

static int ProcessFile(int fd,const char *file_id)
{
	int sz=0,tot,last_time;
	unsigned long line;
	char *eol,*data,*tag;
	int did_start;

   last_time = 0;
   found_header = 0;
	line = 0;
	tot = 0;
	data = data_buffer;
	did_start = 0;
	while (emit_line && (sz=read(fd,data+tot,DATA_BUFFER_SIZE-tot-1-(data-data_buffer))) > 0) {
		tot += sz;

		// check for straight dump
		if (straight_dump != -1) {
			if (write(straight_dump,data_buffer,tot) < 0) {
				return -1;
			}
			tot = 0;
			continue;
		}
		
		// check for hex dump
dump_rest_of_hex:
		if (hex) {
			char buf[32];
			
			while (tot >= 16) {
				buf[16] = 0;
				for(sz=16;sz--;) {
					if (isprint(data[sz]) && (!isspace(data[sz]) || data[sz]==' ')) {
						buf[sz] = data[sz];
					} else {
						buf[sz] = '.';
					}
				}
				printf("%08X: %02X%02X %02X%02X - %02X%02X %02X%02X -- %02X%02X %02X%02X - %02X%02X %02X%02X  %s\n",byte_count,data[0],data[1],data[2],data[3],data[4],data[5],data[6],data[7],data[8],data[9],data[10],data[11],data[12],data[13],data[14],data[15],buf);
				byte_count += 16;
				data += 16;
				tot -= 16;
			}
			if (last_time) {
				if (tot) {
					while (tot < 16) {
						data[tot++] = 0;
					}
					goto dump_rest_of_hex;
				}
				return 0;
			}
			continue;
		}
		
next_tag:
		// check for tags
		if (skip_tags) {
			tag = (char*)memchr(data,'<',tot);
			if (tag) {
				sz = 0;
				for(eol=tag;eol<data+tot;eol++) {
					if (eol[0] == '<') {	
						sz++;
					} else if (eol[0] == '>') {
						sz--; 
						if (!sz) break;
					}
				}
				if (sz) {
					continue;
				}
				tag[0] = 0;
				sz = tag-data;
				if (sz) {
					if (ProcessLine(data,sz,line,0,!did_start)) {
						return -1;
					}
				}
				sz++;
				did_start = 1;
				data += sz;
				tot -= sz;
				sz = eol-data+1;

				WRITE("<",1);
				WRITE(data,sz);
				data += sz;
				tot -= sz;
				goto next_tag;
			}
#if 0
			tag = (char*)memchr(data,'&',tot);
			if (tag) {
				eol = memchr(tag,';',tot-(tag-data));
				if (eol && eol-tag <= 5) {
					tag[0] = 0;
					sz = tag-data;
   				if (sz) {
   					if (ProcessLine(data,sz,line,0,!did_start)) {
   						return -1;
   					}
   				}
   				sz++;
   				did_start = 1;
   				data += sz;
   				tot -= sz;
   				sz = eol-data+1;
					
   				WRITE("&",1);
   				WRITE(data,sz);
   				data += sz;
   				tot -= sz;
   				goto next_tag;
				}
			}
#endif
		}
		
		// process full lines
		while ((eol=(char*)memchr(data,UNIX_TERM,tot)) != NULL) {
have_eol:
			eol[0] = 0;
			sz = eol-data;
			if (ProcessLine(data,sz,line,0,!did_start) < 0) {
				return -1;
			}
			line++;
			sz++;
			if ((!raw_data || (no_url_header && !found_header)) && (eol-data < tot-1) && eol[1] == MAC_TERM) {
				sz++;
			}
			data += sz;
			tot -= sz;
			did_start = 0;
		}
		if (!raw_data || (no_url_header && !found_header)) {
			if ((eol=(char*)memchr(data,MAC_TERM,tot)) != NULL) {
				goto have_eol;
			}
 		}

		// check for buffer back-scoot
		if (data > data_buffer) {
			if (tot) {
				memmove(data_buffer,data,tot);
			}
			data = data_buffer;
		}
		
		// process full buffer
		if (tot == DATA_BUFFER_SIZE) {
dump_entire_buffer:
			data[tot] = 0;
			if (ProcessLine(data,tot,line,1,!did_start) < 0) {
				return -1;
			}
			did_start = 1;
			tot = 0;
			line++;
			data = data_buffer;
		}
	}
	if (tot) {
	   last_time = 1;
	   if (hex) {
	   	goto dump_rest_of_hex;
	   }
		goto dump_entire_buffer;
	}
	if (sz == -1) {
		return -1;
	}
	return 0;
}

static int ProcessAndCloseFile(int fd,const char *file_id)
{
	int result;

	result = ProcessFile(fd,file_id);
	close(fd);
	return result;
}

static void LoadEntireFileAndClose(int fd,char **buffer,int *buffer_len)
{
	char *buf;
	int buf_len,buf_size,sz;

	buf_len = 0;
	buf_size = 4096;
	buf = (char*)malloc(buf_size+1);
	while ((sz=read(fd,buf+buf_len,buf_size-buf_len)) > 0) {
		buf_len += sz;
		if (buf_size - buf_len < 1024) {
			buf_size *= 2;
			buf = (char*)realloc(buf,buf_size+1);
		}
	}
	buf[buf_size] = 0;

	*buffer = buf;
	*buffer_len = buf_len;
	close(fd);
}

static void ListLabeledStringInTag(const char *data,const char *tag,const char *label,const char *url)
{
	const char *m,*s;
	char buf[1024],*p;
	int quotes,k;
	int label_len;
	char **list;
	int n;

	// init
   label_len = strlen(label);
	m = data;
	n = 0;
	list = (char**)NULL;

	// find next label occurance
find_next:
	m = (const char*)stristr(m,label);
	if (!m) {
	
		// dump the result
		while (n--) {
			printf("%s\n",list[n]);
			free(list[n]);
		}
		if (list) {
			free(list);
		}
		return;
	}
	
	// make sure we have an equal sign following
	for(s=m+label_len;*s;s++) {
		if (!isspace(*s)) {
			if (*s != '=') {
advance_and_find:
				m = s;
				goto find_next;
			}
			s++;
			break;
		}
	}
	if (!*s) {
		goto advance_and_find;
	}
	while (isspace(*s)) {
		s++;
	}
	quotes = (*s == '\"');
	if (quotes) {
		s++;
	}
	
	// make sure we are in a tag
	for(p=(char*)m-1;p>=data;p--) {
		if (*p == '>') {
			goto advance_and_find;
		}
		if (*p == '<') {
			break;
		}
	}
	if (p < data) {
		goto advance_and_find;
	}
	if (tag) {
		do {
			p++;
		} while (isspace(*p));
		for(k=0;p[k] && tag[k];k++) {
			if (tolower(p[k]) != tolower(tag[k])) {
				goto advance_and_find;
			}
		}
		if (!isspace(p[k])) {
			goto advance_and_find;
		}
	}
	
	// pluck out the string
	for(p=buf;*s && (!quotes||*s!='\"') && (quotes||!isspace(*s));) {
		*p++ = *s++;
	}
	*p = 0;

	// make absolute
	if (url) {
		if (!strstr(buf,"://")) {
			for(p=(char*)url+strlen(url)-1;p>url;p--) {
				if (*p == '/') {
					break;
				}
			}
			if (buf[0] == '/') {
				memmove(buf,buf+1,strlen(buf));
			}
			strnins(buf,url,(p+1)-url);
		}
	}

	// see if it's already in the list
	for(k=n;k--;) {
		if (!strcmp(buf,list[k])) {
			goto advance_and_find;
		}
	}

	// append in the list
	if (!n) {
		list = (char**)malloc(sizeof(char*)*(n+1));
	} else {
		list = (char**)realloc(list,sizeof(char*)*(n+1));
	}
	list[n] = (char*)malloc(strlen(buf)+1);
	strcpy(list[n],buf);
	n++;

	// do it again
	goto advance_and_find;
}

static int CreateSocket()
{
	if (use_udp) {
		return socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	} else {
		return socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
	}
}

static int CreateConnectedSocket(const char *domain_port)
{
	char domain[128],*m;
	uint32 ipAddr;
	int port;
	int sock;
	InetSockAddr sa;
	unsigned long a,b,c,d;
	struct hostent *ent;

	// get domain-port information
	strcpy(domain,domain_port);
	for(m=domain;*m;m++) {
		if (*m == ':') {
			*m = 0;
			port = atoi(m+1);
			goto got_domain_port;
		}
	}
	errno = EINVAL;
	return -1;

got_domain_port:

	// translate domain name into ip address
	if (sscanf(domain,"%ld.%ld.%ld.%ld",&a,&b,&c,&d) == 4) {
		ipAddr = (a << 24) | (b << 16) | (c << 8) | d;
	} else {
		ent = gethostbyname(domain);
		if (!ent) {
			return -1;
		}
		ipAddr = ntohl(**((uint32**)ent->h_addr_list));
	}

	// create socket
	sock = CreateSocket();
	if (sock == -1) {
		return -1;
	}

	// connect socket
	sa.family = AF_INET;				/* an internet address */
	sa.addr = htonl(ipAddr);	/* the address (NBO) */
	sa.port = htons(port);		/* the port (NBO) */
	if (connect(sock,(struct sockaddr*)&sa,sizeof(sa)) == -1) {
		close(sock);
		return -1;
	}

	// done!
	return sock;
}

static int CreateBoundSocket(int port)
{
	InetSockAddr sa;
	int sock;

	// create socket
	sock = CreateSocket();
	if (sock == -1) {
		return -1;
	}

	// bind socket
	sa.family = AF_INET;				/* an internet address */
	sa.addr = htonl(INADDR_ANY);	/* the address (NBO) */
	sa.port = htons(port);		/* the port (NBO) */
	if (bind(sock,(struct sockaddr*)&sa,sizeof(sa)) == -1) {
		close(sock);
		return -1;
	}

	// start listening
	if (listen(sock,128) == -1) {
		close(sock);
		return -1;
	}

	// done!
	return sock;
}

static int ProcessFileFromPath(const char *path)
{
	int fd;
	char buf[1024];

	// set up input stream based on URL
	if (!strncmp(path,"file://",7)) {
		fd = open(path+6,O_RDONLY);
		goto have_fd;
	} else if (!strncmp(path,"raw://",6)) {
		fd = CreateConnectedSocket(path+6);
		goto have_fd;
	} else if (!strncmp(path,"http://",7)) {
		char hostport[128],*m;
		const char *s;
		
		s = path+7;
		for(m=hostport;s[0]!=':'&&s[0]!='/'&&s[0];) {
			*m++ = *s++;
		}
		if (s[0] == ':') {
			do {
				*m++ = *s++;
			} while (isdigit(s[0]));
			*m = 0;
		} else {
			strcpy(m,":80");
		}
		fd = CreateConnectedSocket(hostport);
		if (fd != -1) {
			if (write(fd,buf,sprintf(buf,"GET %s HTTP/1.0\r\n",(*s) ? s : "/")) == -1) {
err1:
				close(fd);
				return -1;
			}
			if (write(fd,buf,sprintf(buf,"Connection: close\r\nHost: %s\r\nAccept: */*\r\n\r\n",hostport)) == -1) {
				goto err1;
			}
		}
		if (!*s) {
			strcpy(buf,path);
			strcat(buf,"/");
			path = buf;
		}
		goto have_fd;
	}
	
	// open file
	fd = open(path,O_RDONLY);
have_fd:
	if (fd == -1) {
		return -1;
	}
	
	// list things
	if (http_links || image_links) {
		char *buf;
		int buf_len;

		LoadEntireFileAndClose(fd,&buf,&buf_len);
		if (http_links) {
			ListLabeledStringInTag(buf,NULL,"href",path);
		}
		if (image_links) {
			if (http_links && image_links) {
				printf("#\n");
			}
			ListLabeledStringInTag(buf,"img","src",path);
		}
		free(buf);
		return 0;
	}
	
	// process the file normally
	return ProcessAndCloseFile(fd,path);
}

static void ErrorDuringRun(const char *prog_name,const char *file_id)
{
	fprintf(stderr,"%s: %s: %s\n",prog_name,file_id,strerror(errno));
}

static void PrintUsage(const char *prog_name)
{
	int k;

	printf("Usage: %s [OPTION | FILE | URL]...\n",prog_name);
	printf("Concatenates files, URL's, or standard input, and sends the result to stdout.\n");
	printf("Like 'cat', but with more options.  All of the 'cat' switches are preserved for\n");
	printf("compatibility.  Supported URL types: file, http, raw.\n");
	printf("\n");
	for(k=0;k<sizeof(usage_strs)/sizeof(char*);k+=2) {
		printf(" %-23s%s\n",usage_strs[k],usage_strs[k+1]);
	}
	printf("\n");
	printf("With no FILE, or with - in the file list, read standard input.\n");
	printf("\n");
	printf("Report bugs to dog-bugs@fastscheduler.com\n");
}

static void PrintVersion(const char *prog_name)
{
	printf("%s -- better than cat\n\n",prog_name);
	printf("version:                    %d.%d\n",MAJOR_VERSION,MINOR_VERSION);
	printf("compile date:               %s\n",__DATE__);
	printf("author:                     Jason Cohen (dogboy@fastscheduler.com)\n");
	printf("man page and distribution:  Jacob Leverich (leverich@photodex.com)\n");
}

static void AddRanges(const char *str,LineRange **range_stack)
{
	LineRange *r,*q;
	unsigned long x;
	char *res;

add_another_range:
	if (str[0] == '-') {
		x = 1;
		res = (char*)str;
		goto start_range;
	} else {
		x = strtoul(str,&res,10);
		if (res != str) {
start_range:
			r = (LineRange*)malloc(sizeof(LineRange));
			r->next = NULL;
			if (*range_stack) {
				q = *range_stack;
				while (q->next) {
					q = q->next;
				}
				q->next = r;
			} else {
				*range_stack = r;
			}
			r->end = r->start = x;
			str = res;
			if (str[0] == '-') {
				str++;
				x = strtoul(str,&res,10);
				if (res != str) {
					if (x > r->start) {
						r->end = x;
					} else {
						r->end = r->start;
						r->start = x;
					}
				} else {
					r->end = UINT_MAX;
				}
				str = res;
			}
			if (str[0] == ',') {
				str++;
				goto add_another_range;
			}
		}
	}
}

static void DumpThings(int argc,char **argv,int sock)
{
	int k,files_done;

	// process all listed files
   files_done = 0;
	for(k=optind;k<argc;k++) {
		if (argv[k][0] != '-') {
	      files_done++;
			if (ProcessFileFromPath(argv[k]) < 0) {
				ErrorDuringRun(argv[0],argv[k]);
			}
		} else {
			ProcessFile(0,"standard input");
			files_done++;
		}
	}
	
	// process stdin if we have to
	if (!files_done && (sock == -1 || !isatty(0))) {
		ProcessFile(0,"standard input");
	}
	
	// dump the result from the socket
	if (sock != -1) {
	   straight_dump = -1;
		if (ProcessFile(sock,socket_domain_str)) {
			if (errno != ECONNRESET) {
				ErrorDuringRun(argv[0],socket_domain_str);
			}
		}
  	}

	// close socket
	if (sock != -1) {
		close(sock);
	}
}

static void WaitAndDump(int argc,char **argv,int sock)
{
  	InetSockAddr sa;
	int newsock;
	uint32 ipAddr;
	int k;
	time_t curr_time;
	char curr_date[64];

	k = sizeof(sa);
	while ((newsock=accept(sock,(struct sockaddr*)&sa,&k)) != -1) {
		if (!no_bind_header) {
			ipAddr = ntohl(sa.addr);
			time(&curr_time);
         strcpy(curr_date,ctime(&curr_time));
			while (!isalnum(curr_date[strlen(curr_date)-1])) {
	         curr_date[strlen(curr_date)-1] = 0;
			}
			printf("# %s from %d.%d.%d.%d:%d\n",curr_date,(int)ipAddr>>24,(int)(ipAddr>>16)&0xff,(int)(ipAddr>>8)&0xff,(int)ipAddr&0xff,ntohs(sa.port));
 		}
		straight_dump = newsock;
		DumpThings(argc,argv,hang_up_bind ? -1 : newsock);
		if (hang_up_bind) {
			close(newsock);
		}
	}
}

int main(int argc,char **argv)
{
	int bind_port,sock;
	__dog_signed char opt;
	time_t t;

	// init system
	time(&t);
	srand((unsigned int)t);
	
	// process switches
	bind_port = -1;
	while ((opt=getopt_long(argc,argv,optstring,options,NULL)) != EOF) {
		switch (opt) {

			case 'A':
				show_nonprinting = 1;
				print_trailing_dollarsign = 1;
				leave_tabs = 0;
				break;

			case 'b':
				print_line_numbers = 1;
            print_nonblank_line_numbers = 0;
				break;

			case 'B':
				no_blanks = 1;
				break;

			case 'e':
				show_nonprinting = 1;
				print_trailing_dollarsign = 1;
				break;

			case 'E':
				print_trailing_dollarsign = 1;
				break;

			case 'f':
				AddRanges(optarg,&field_range);
				break;

			case 'F':
				if (!strcmp(optarg,"white")) {
					field_sep = FS_WHITESPACE;
				} else if (!strcmp(optarg,"punct")) {
					field_sep = FS_PUNCTUATION;
				} else {
					field_sep = optarg[0];
				}
				break;

			case 'l':
				AddRanges(optarg,&line_range);
				break;

			case 'n':
				print_line_numbers = 1;
            print_nonblank_line_numbers = 1;
				break;

			case 's':
				only_one_blank_line = 1;
				return 0;

			case 't':
				show_nonprinting = 1;
				leave_tabs = 0;
				break;

			case 'T':
				leave_tabs = 0;
				break;

			case 'u':
				break;

			case 'w':
				max_chars_per_line = optarg ? atoi(optarg) : 0;
				if (!max_chars_per_line) {
	            max_chars_per_line = 80;
				}
				break;

			case 'v':
				show_nonprinting = 1;
				break;

			case OPT_HELP:
				PrintUsage(argv[0]);
				return 0;

			case OPT_VERSION:
				PrintVersion(argv[0]);
				return 0;

#if ALLOW_STRFRY
			case OPT_STRFRY:
				opt_strfry = 1;
				break;
#endif

			case OPT_ROT:
				opt_rotate = atoi(optarg);
				break;

			case OPT_UPPER:
				opt_toupper = 1;
				break;

			case OPT_LOWER:
				opt_tolower = 1;
				break;

			case OPT_KRAD:
				opt_tolower = 1;
				opt_krad = 1;
				break;

			case OPT_SOCK:
				strcpy(socket_domain_str,optarg);
				break;

			case OPT_BIND:
				bind_port = atoi(optarg);
				break;

			case OPT_NOHEADER:
				no_bind_header = 1;
            no_url_header = 1;
				break;

			case OPT_UDP:
				use_udp = 1;
				break;

			case OPT_HANGUP:
				hang_up_bind = 1;
				break;

			case OPT_SOCKTEST:
				sock_test = 1;
				break;

			case OPT_LINKS:
				http_links = 1;
				break;

			case OPT_IMAGES:
				image_links = 1;
				break;

			case OPT_UNIX:
				line_style = LS_UNIX;
				break;

			case OPT_DOS:
				line_style = LS_DOS;
				break;

			case OPT_MAC:
				line_style = LS_MAC;
				break;

			case OPT_RAW:
				raw_data = 0;
				break;

			case OPT_HIDENONP:
				hide_nonprinting = 1;
				break;

			case OPT_SKIPTAGS:
				skip_tags = 1;
				break;
			
			case OPT_HEXDUMP:
				hex = 1;
				break;
			
			case OPT_OOG:
				oog = 1;
				break;
			
			default:
				fprintf(stderr,"Try '%s --help' for more information.\n",argv[0]);
				return 1;
		}
	}
	if (no_bind_header && bind_port == -1) {
		no_bind_header = 0;
	}

	// open the socket
	sock = -1;
	if (socket_domain_str[0]) {
		sock = CreateConnectedSocket(socket_domain_str);
		if (sock == -1 && !sock_test) {
			ErrorDuringRun(argv[0],socket_domain_str);
			return 1;
		}
		if (sock_test) {
			printf("%s:%s\n",socket_domain_str,(sock != -1) ? "available" : "unavailable");
			if (sock != -1) {
				close(sock);
			}
			return 0;
		}
      straight_dump = sock;
	}
	if (bind_port != -1) {
		sock = CreateBoundSocket(bind_port);
		if (sock == -1) {
			ErrorDuringRun(argv[0],"bound port");
			return 1;
		}
		WaitAndDump(argc,argv,sock);
		return 0;
   }

	DumpThings(argc,argv,sock);

	// finished
	return 0;
}

/*
	Ideas:

	-- hex dump
	-- ftp://
	-- auto-wrap (maybe even word-wrap)
	-- pluck out fields with field separaters
	-- uniqify (with count?)
	-- line/lines reversal
*/

