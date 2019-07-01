#ifndef _CPU_H_
#define _CPU_H_
#include "memory.h"
#include "instruction.h"
#include "stages.h"

struct RegisterStruct
{
	int lockRefCount;
	uint32_t value;
	RegisterStruct()
	{
		lockRefCount=0;
		value=0;
	}
};

// Register machine core state.
class cpu_core {
public:
	cpu_core() : ifs(this), ids(this), exs(this), mys(this), wbs(this) {}

	uint32_t PC;
	uint32_t cycles;
	uint32_t BPHits; 
	uint32_t BPMisses; 
	int branchPredictor;
	int table2bit[1024] = {0}; // initialise table which contains all finite state machine states, initially all 0
	std::bitset<10> index2level = 0b0000000000; // initialise global history buffer called index2level

	bool usermode, verbose;
	memory  *mem;
	//uint32_t registers[32];
	RegisterStruct registers[32];
	InstructionFetchStage  ifs;
	InstructionDecodeStage ids;
	ExecuteStage   exs;
	MemoryStage    mys;
	WriteBackStage wbs;

};

void run_cpu(memory *m, const bool verbose, const int branchPredictor); // added variable for branch predictor #

#endif /* _CPU_H_ */
