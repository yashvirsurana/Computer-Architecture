#include <algorithm>
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <unistd.h>
#include "memory.h"
#include "cpu.h"

extern char   *optarg;
extern int32_t optind;
extern int32_t optopt;
extern int32_t opterr;
extern int32_t optreset;

using namespace std;

// Usage of the program
static void usage(char *name)
{
	cout << name << " usage:\n" <<
	        "\t-t text_stream_file: load .text with the contents of file\n" <<
	        "\t-d data_stream_file: [optional] load .data with contents of file\n" << endl;
}


// *****************************
//           entry point
// *****************************
int32_t main(int32_t argc, char **argv)
{
	memory   mem;
	bool     verb = false;
	bool     text_loaded = false;
	int32_t  ch;
	uint32_t text_ptr = text_segment;
	uint32_t data_ptr = data_segment;
	int branchPredictor = 0; // I added this line

	while ((ch = getopt(argc, argv, "t:d:vb:")) != -1) {
		switch (ch) {
		case 't': {
			ifstream input(optarg, ios::binary);
			if (!(input.good() && input.is_open())) {
				cout << *argv << ": " << optarg << " does not exist" << endl;
				exit(20);
			}
			byte  c;
			char *pc = (char *)&c;
			while (!input.eof()) {
				input.read(pc, sizeof(byte));
				mem.set<byte>(text_ptr++, c);
			}
			input.close();
			text_loaded = true;
		}
		break;
		case 'd': {
			ifstream input(optarg, ios::binary);
			if (!(input.good() && input.is_open())) {
				cout << *argv << ": " << optarg << " does not exist" << endl;
				exit(20);
			}
			byte c;
			while (!input.eof()) {
				input.read((char *)&c, sizeof(byte));
				mem.set<byte>(data_ptr++, c);
			}
			input.close();
		}
		break;

		case 'b': 
			//<CAR_PA1_HOOK1>
			branchPredictor = atoi(optarg); // Read in the predictor to use assigned by a #
		break;

		case 'v':
			verb = true;
			break;

		default:
			usage(*argv);
			exit(10);
			break;
		}
	}

	if (!text_loaded) {
		usage(*argv);
		exit(10);
	}
	cout << *argv << ": Starting CPU..." << endl;
	run_cpu(&mem, verb, branchPredictor); // Added argument for the branch predictor number
	cout << *argv << ": CPU Finished" << endl;

	if (mem.is_collecting()) mem.display_memory_stats();
}
