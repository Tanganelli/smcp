
#include <smcp/assert_macros.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/errno.h>
#include "help.h"
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <libgen.h>
#include <time.h>

#if !HAVE_FGETLN
#include <missing/fgetln.h>
#endif

#if HAS_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif
#include <poll.h>

#include <smcp/smcp.h>

#include "cmd_list.h"
#include "cmd_test.h"
#include "cmd_get.h"
#include "cmd_post.h"
#include "cmd_pair.h"
#include "cmd_repeat.h"
#include "cmd_delete.h"
#include "cmd_monitor.h"

#include "smcpctl.h"

#include <smcp/url-helpers.h>

bool show_headers = 0;
static int gRet = 0;
static smcp_t smcp;
static bool istty = true;

static arg_list_item_t option_list[] = {
	{ 'h', "help",	NULL, "Print Help"				},
	{ 'd', "debug", NULL, "Enable debugging mode"	},
	{ 'p', "port",	NULL, "Port number"				},
	{ 'f', NULL,	NULL, "Read commands from file" },
	{ 0 }
};

void print_commands();

static int
tool_cmd_help(
	smcp_t smcp, int argc, char* argv[]
) {
	if((2 == argc) && (0 == strcmp(argv[1], "--help"))) {
		printf("Help not yet implemented for this command.\n");
		return ERRORCODE_HELP;
	}

	if((argc == 2) && argv[1][0] != '-') {
		const char *argv2[2] = {
			argv[1],
			"--help"
		};
		return exec_command(smcp, 2, (char**)argv2);
	} else {
		print_commands();
	}
	return ERRORCODE_HELP;
}


static int
tool_cmd_cd(
	smcp_t smcp, int argc, char* argv[]
) {
	if((2 == argc) && (0 == strcmp(argv[1], "--help"))) {
		printf("Help not yet implemented for this command.\n");
		return ERRORCODE_HELP;
	}

	if(argc == 1) {
		printf("%s\n", getenv("SMCP_CURRENT_PATH"));
		return 0;
	}

	if(argc == 2) {
		char url[1000];
		strcpy(url, getenv("SMCP_CURRENT_PATH"));
		if(url_change(url, argv[1]))
			setenv("SMCP_CURRENT_PATH", url, 1);
		else
			return ERRORCODE_BADARG;
	}

	return 0;
}


struct {
	const char* name;
	const char* desc;
	int			(*entrypoint)(
		smcp_t smcp, int argc, char* argv[]);
	int			isHidden;
} commandList[] = {
	{
		"get",
		"Fetches the value of a variable.",
		&tool_cmd_get
	},
	{ "cat", NULL,
	  &tool_cmd_get,
	  1 },
	{
		"post",
		"Triggers an event.",
		&tool_cmd_post
	},
	{
		"put",
		"Overwrites a resource.",
		&tool_cmd_post
	},
	{
		"pair",
		"Pairs an event to an action.",
		&tool_cmd_pair
	},
	{
		"delete",
		"Deletes a resource.",
		&tool_cmd_delete
	},
	{
		"list",
		"Displays the contents of a folder.",
		&tool_cmd_list
	},
	{ "ls",	 NULL,
	  &tool_cmd_list,
	  1 },
	{ "rm",	 NULL,
	  &tool_cmd_delete,
	  1 },
	{
		"test",
		"Self test mode.",
		&tool_cmd_test
	},
	{
		"repeat",
		"Repeat the specified command",
		&tool_cmd_repeat
	},
	{
		"monitor",
		"Monitor a given URL for events and changes",
		&tool_cmd_monitor
	},
	{
		"help",
		"Display this help.",
		&tool_cmd_help
	},
	{ "?",	 NULL,
	  &tool_cmd_help,
	  1 },

	{ "cd",	 "Change current directory or URL (command mode)",
	  &tool_cmd_cd },

	{ NULL }
};

void
print_commands() {
	int i;

	printf("Commands:\n");
	for(i = 0; commandList[i].name; ++i) {
		if(commandList[i].isHidden)
			continue;
		printf(
			"   %s %s%s\n",
			commandList[i].name,
			"                     " + strlen(commandList[i].name),
			commandList[i].desc
		);
	}
}

int
exec_command(
	smcp_t smcp, int argc, char * argv[]
) {
	int ret = 0;
	int j;

	require(argc, bail);

	if((strcmp(argv[0],
				"quit") == 0) ||
	        (strcmp(argv[0],
				"exit") == 0) || (strcmp(argv[0], "q") == 0)) {
		ret = ERRORCODE_QUIT;
		goto bail;
	}

	for(j = 0; commandList[j].name; ++j) {
		if(strcmp(argv[0], commandList[j].name) == 0) {
			if(commandList[j].entrypoint) {
				ret = commandList[j].entrypoint(smcp, argc, argv);
				goto bail;
			} else {
				fprintf(stderr,
					"The command \"%s\" is not yet implemented.\n",
					commandList[j].name);
				ret = ERRORCODE_NOCOMMAND;
				goto bail;
			}
		}
	}

	fprintf(stderr, "The command \"%s\" is not recognised.\n", argv[0]);

	ret = ERRORCODE_BADCOMMAND;

bail:
	return ret;
}

static char* get_next_arg(char *buf, char **rest) {
	char* ret = NULL;

	while(*buf && isspace(*buf)) buf++;

	if(!*buf || *buf == '#')
		goto bail;

	ret = buf;

	char quote_type = 0;
	char* write_iter = ret;

	while(*buf) {
		if(quote_type && *buf==quote_type) {
			quote_type = 0;
			buf++;
			continue;
		}
		if(*buf == '"' || *buf == '\'') {
			quote_type = *buf++;
			continue;
		}

		if(!quote_type && isspace(*buf)) {
			buf++;
			break;
		}

		if(buf[0]=='\\' && buf[1]) buf++;

		*write_iter++ = *buf++;
	}

	*write_iter = 0;

bail:
	if(rest)
		*rest = buf;
	return ret;
}

void process_input_line(char *l) {
	char *inputstring;
	char *argv2[100];
	char **ap = argv2;
	int argc2 = 0;

	if(!l[0])
		goto bail;
	l = strdup(l);
#if HAS_LIBREADLINE
	add_history(l);
#endif

	inputstring = l;

	while((*ap = get_next_arg(inputstring,&inputstring))) {
		if(**ap != '\0') {
			ap++;
			argc2++;
		}
	}
	if(argc2 > 0) {
		gRet = exec_command(smcp, argc2, argv2);
		if(gRet == ERRORCODE_QUIT)
			goto bail;
		else if(gRet && (gRet != ERRORCODE_HELP))
			fprintf(stderr,"Error %d\n", gRet);

#if HAS_LIBREADLINE
		write_history(getenv("SMCP_HISTORY_FILE"));
#endif
	}

bail:
	free(l);
	return;
}

static char* get_current_prompt() {
	static char prompt[MAX_URL_SIZE+40] = {};
	char* current_smcp_path = getenv("SMCP_CURRENT_PATH");
	snprintf(prompt,
		sizeof(prompt),
//		"\1\033[1;37;40m\2"
		"%s"
//		"\1\033[0;37;40m\2"
		"> ",
		current_smcp_path ? current_smcp_path : ""
	);
	return prompt;
}

#if HAS_LIBREADLINE
void process_input_readline(char *l) {
	process_input_line(l);
	if(istty) {
		if(gRet==ERRORCODE_QUIT) {
			rl_set_prompt("");
		} else {
			rl_callback_handler_install(get_current_prompt(), &process_input_readline);
			//rl_set_prompt(get_current_prompt());
		}
	}
}
#endif


#pragma mark -

#if HAS_LIBREADLINE

char *
smcp_command_generator(
	const char *text,
	int state
) {
	static int list_index, len;
	const char *name;

	/* If this is a new word to complete, initialize now.  This includes
	 saving the length of TEXT for efficiency, and initializing the index
	 variable to 0. */
	if (!state)
	{
		list_index = 0;
		len = strlen (text);
	}

	/* Return the next name which partially matches from the command list. */
	while ((name = commandList[list_index].name))
	{
		list_index++;

		if (strncmp (name, text, len) == 0)
			return (strdup(name));
	}

	/* If no names matched, then return NULL. */
	return ((char *)NULL);
}

char *
smcp_directory_generator(
	const char *text,
	int state
) {
	char *ret = NULL;
	static size_t len;
	static FILE* temp_file = NULL;
	const char *name;
	static char* prefix;
	static char* fragment;
	size_t namelen = 0;

	rl_filename_completion_desired = 1;

	/* If this is a new word to complete, initialize now.  This includes
	 saving the length of TEXT for efficiency, and initializing the index
	 variable to 0. */
	if (!state)
	{
		int i;

		if(temp_file)
			fclose(temp_file);

		temp_file = tmpfile();

		require(temp_file!=NULL,bail);

		free(prefix);
		free(fragment);

		prefix = strdup(text);

		// Figure out where the last path component starts.
		for(i=strlen(prefix);i && prefix[i]!='/';i--);

		if(prefix[i]=='/') {
			prefix[i] = 0;
			if(i==0) {
				prefix = strdup("/");
			}
			fragment = strdup(prefix+i+1);
		} else {
			fragment = strdup(prefix);
			free(prefix);
			prefix = strdup(".");
		}
		char* cmdline = NULL;
		FILE* real_stdout = stdout;

		asprintf(&cmdline, "list --filename-only --timeout 1000 \"%s\"",prefix);
		require(cmdline,bail);
		//fprintf(stderr,"\n[cmd=\"%s\"] ",cmdline);

		stdout = temp_file;
		if(strequal_const(fragment, "."))
			fprintf(temp_file,"../\n");
		process_input_line(cmdline);
		stdout = real_stdout;
		free(cmdline);

		if(url_is_root(getenv("SMCP_CURRENT_PATH")) && !url_is_root(prefix)) {
			if(!i) {
				asprintf(&cmdline, "list --filename-only --timeout 750 /.well-known/core");
				require(cmdline,bail);
				//fprintf(stderr,"\n[cmd=\"%s\"] ",cmdline);

				fprintf(temp_file,".well-known/\n");

				stdout = temp_file;
				process_input_line(cmdline);
				stdout = real_stdout;
				free(cmdline);
			} else {
				if(strequal_const(prefix, ".well-known"))
					fprintf(temp_file,"core/\n");
			}
		}

		rewind(temp_file);
		len = strlen(fragment);
	}

	require(temp_file!=NULL,bail);

	while ((name = fgetln(temp_file, &namelen)))
	{
		if(namelen<len)
			continue;
		//fprintf(stderr,"\n[candidate=\"%s\" namelen=%d] ",name,namelen);
		if(url_is_root(getenv("SMCP_CURRENT_PATH")) && strequal_const(prefix,".")) {
			while(name[0]=='/') {
				name++;
				namelen--;
			}
		}
		if (strncmp (name, fragment, len) == 0) {
			while(namelen && isspace(name[namelen-1])) { namelen--; }
			//namelen--;
			if(name[namelen-1]=='/')
				rl_completion_append_character = 0;

			if(	strequal_const(prefix, "/")
				|| strequal_const(prefix, ".")
				|| name[0]=='/'
				||(url_is_root(getenv("SMCP_CURRENT_PATH")) && strequal_const(prefix,"."))
			) {
				ret = strndup(name,namelen);
			} else {
				char* tmp = strndup(name,namelen);
				if(prefix[strlen(prefix)-1]=='/')
					asprintf(&ret, "%s%s",prefix,tmp);
				else
					asprintf(&ret, "%s/%s",prefix,tmp);
				free(tmp);
			}
			break;
		}
	}


bail:
	//fprintf(stderr,"\n[prefix=\"%s\" ret=\"%s\"] ",prefix,ret);

	return ret;
}

char **
smcp_attempted_completion (
	char *text,
	int start,
	int end
) {
	char **matches;

	matches = (char **)NULL;

	/* If this word is at the start of the line, then it is a command
	 to complete.  Otherwise it is the name of a file in the current
	 directory. */
	if(start == 0) {
		matches = completion_matches (text, &smcp_command_generator);
	} else {
		if(text[0]=='-') {
			// Argument Completion.
			// TODO: Writeme!
			rl_attempted_completion_over = 1;
			//fprintf(stderr,"\nrl_line_buffer=\"%s\"\n",rl_line_buffer);
		}
	}

	return (matches);
}

static int
initialize_readline() {
	int ret = 0;

	require_action(NULL != readline, bail, ret = ERRORCODE_NOREADLINE);
	rl_initialize();

	rl_readline_name = "smcp";
	rl_completer_word_break_characters = " \t\n\"\\'`@$><|&{("; // Removed '=' ';'
	/* Tell the completer that we want a crack first. */
	rl_attempted_completion_function = (CPPFunction *)smcp_attempted_completion;
	rl_completion_entry_function = (Function*)&smcp_directory_generator;

	using_history();
	read_history(getenv("SMCP_HISTORY_FILE"));
	rl_instream = stdin;

	rl_callback_handler_install(get_current_prompt(), &process_input_readline);

bail:
	return ret;
}
#endif

#pragma mark -


int
main(
	int argc, char * argv[]
) {
	int i, debug_mode = 0;
	int port = 61616;

	srandom(time(NULL));

	BEGIN_LONG_ARGUMENTS(gRet)
	HANDLE_LONG_ARGUMENT("port") port = strtol(argv[++i], NULL, 0);
	HANDLE_LONG_ARGUMENT("debug") debug_mode++;

	HANDLE_LONG_ARGUMENT("help") {
		print_arg_list_help(option_list,
			argv[0],
			"[options] <sub-command> [args]");
		print_commands();
		gRet = ERRORCODE_HELP;
		goto bail;
	}
	BEGIN_SHORT_ARGUMENTS(gRet)
	HANDLE_SHORT_ARGUMENT('p') port = strtol(argv[++i], NULL, 0);
	HANDLE_SHORT_ARGUMENT('d') debug_mode++;
#if HAS_LIBREADLINE
	HANDLE_SHORT_ARGUMENT('f') {
		stdin = fopen(argv[++i], "r");
		if(!stdin) {
			fprintf(stderr,
				"%s: error: Unable to open file \"%s\".\n",
				argv[0],
				argv[i - 1]);
			return ERRORCODE_BADARG;
		}
	}
#endif
	HANDLE_SHORT_ARGUMENT2('h', '?') {
		print_arg_list_help(option_list,
			argv[0],
			"[options] <sub-command> [args]");
		print_commands();
		gRet = ERRORCODE_HELP;
		goto bail;
	}
	HANDLE_OTHER_ARGUMENT() {
		break;
	}
	END_ARGUMENTS

	show_headers = debug_mode;
	istty = isatty(fileno(stdin));

	smcp = smcp_create(port);
	setenv("SMCP_CURRENT_PATH", "coap://localhost/", 0);

	smcp_set_proxy_url(smcp,getenv("COAP_PROXY_URL"));

	if(i < argc) {
		if(((i + 1) == argc) && (0 == strcmp(argv[i], "help")))
			print_arg_list_help(option_list,
				argv[0],
				"[options] <sub-command> [args]");

		if((0 !=
		        strncmp(argv[i], "smcp:",
					5)) && (0 != strncmp(argv[i], "coap:", 5))) {
			gRet = exec_command(smcp, argc - i, argv + i);
#if HAS_LIBREADLINE
			if(gRet || (0 != strcmp(argv[i], "cd")))
#endif
			goto bail;
		} else {
			setenv("SMCP_CURRENT_PATH", argv[i], 1);
		}
	}

	if(istty) {
		fprintf(stderr,"Listening on port %d.\n",smcp_get_port(smcp));
#if !HAS_LIBREADLINE
		print_arg_list_help(option_list,
			argv[0],
			"[options] <sub-command> [args]");
		print_commands();
		gRet = ERRORCODE_NOCOMMAND;
		goto bail;
#else   // HAS_LIBREADLINE
		setenv("SMCP_HISTORY_FILE", tilde_expand("~/.smcp_history"), 0);

		require_noerr(gRet = initialize_readline(),bail);

#endif  // HAS_LIBREADLINE
	}

	// Command mode.
	while((gRet != ERRORCODE_QUIT) && !feof(stdin)) {
#if HAS_LIBREADLINE
		if(istty) {
			struct pollfd polltable[2] = {
				{ fileno(stdin),				   POLLIN | POLLHUP,
				  0 },
				{ smcp_get_fd(smcp), POLLIN | POLLHUP,
				  0 },
			};

			if(poll(polltable, 2,
					smcp_get_timeout(smcp)) < 0)
				break;

			if(polltable[0].revents)
				rl_callback_read_char();
		} else
#endif  // HAS_LIBREADLINE
		{
			char linebuffer[200];
			fgets(linebuffer, sizeof(linebuffer), stdin);
			process_input_line(linebuffer);
		}

		smcp_process(smcp, 0);
	}

bail:
#if HAS_LIBREADLINE
	rl_callback_handler_remove();
#endif  // HAS_LIBREADLINE
	if(gRet == ERRORCODE_QUIT)
		gRet = 0;

	if(smcp)
		smcp_release(smcp);
	return gRet;
}
