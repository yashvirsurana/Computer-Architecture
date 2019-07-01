#include "codegen.h"
#include <unistd.h>
#include <fstream>
#include <stdlib.h>
#include "../sim/types.h"
#include "../sim/memory.h"

using namespace std;

int32_t yyparse();
extern FILE *yyin;
codegen *cgen;

// this is the generic error handler. we skip it in final builds
void yyerror(const char *s)
{
	cout << s << ": ";
}


extern char   *optarg;
extern int32_t optind;
extern int32_t optopt;
extern int32_t opterr;
extern int32_t optreset;

// will replace an extension in an existing string. must be nonconst (ie,
// dont do this: replaceExtension("bla.txt","pal")).
// if you pass a string with multiple extensions, it will replace the last.
// if you pass a string without an extension, this method will add one to the end (which may exceed the buffer)
// NOTE:  This function should only be used on memory blocks which are long enough
//        to accomidate the new string. It *will* overstep a buffer.
static void replaceExtension(char *name, const char *newext)
{
	char *iter = name, *lastdot = 0;

	while (*iter++ != 0) if (*iter == '.') lastdot = iter + 1;
	if (lastdot) *lastdot = 0;
	strcat(name, newext);
}


// Prints a nice useful usage message
static void usage(char *name)
{
	cout << name << " usage:\n" <<
	     "\t-f source_file: read ascii assembly code from file\n" <<
	     "\t-t text_stream_file: [optional] output .text to specified file\n" <<
	     "\t-d data_stream_file: [optional] output .data to specified file\n";
}


// *****************************
//           entry point
// *****************************
int32_t main(int32_t argc, char **argv)
{
	int32_t ch;
	char *source = NULL;
	char *textdest = NULL, *datadest = NULL;
	ofstream text_stream;
	ofstream data_stream;

	while ((ch = getopt(argc, argv, "t:d:f:")) != -1) {
		switch (ch) {
		case 't':
			textdest = optarg;
			break;

		case 'd':
			datadest = optarg;
			break;

		case 'f':
			yyin = fopen(optarg, "r");
			source = optarg;
			break;

		default:
			usage(*argv);
			exit(10);
			break;
		}
	}

	if (!source) {
		usage(*argv);
		exit(10);
	}

	if (!textdest) {
		textdest = strdup(source);
		replaceExtension(textdest, "t");
	}

	if (!datadest) {
		datadest = strdup(source);
		replaceExtension(datadest, "d");
	}
	data_stream.open(datadest, ios::binary);
	text_stream.open(textdest, ios::binary);

	bool success = false;
	cgen = new codegen(writeback_position_t(text_segment, &text_stream)
	                  ,writeback_position_t(data_segment, &data_stream));

	// Perform the lexing and grammar
	success = 0 == yyparse();

	// Write enough nops to clear the pipeline (which simplifies the simulator a touch)
	cgen->in_text_section();
	cgen->emit<byte>(0);
	cgen->emit<byte>(0);
	cgen->emit<byte>(0);
	cgen->emit<byte>(0);
	cgen->emit<byte>(0);

	if (!cgen->balanced_labels()) {
		delete cgen;
		cerr << *argv << ": a label was used, but not defined" << endl;
		success = false;
	}

	delete cgen;

	text_stream.close();
	data_stream.close();

	if (success) {
		cout << *argv <<
		        ": finished assembling \"" << source << "\"" << endl <<
		        "files created: * \"" << textdest << "\"" << endl <<
		        "               * \"" << datadest << "\"" << endl;
		return 0;
	}
	else {
		// TODO: delete the output files!
		cout << *argv << ": there were errors while parsing" << endl;
		return 10;
	}
}
