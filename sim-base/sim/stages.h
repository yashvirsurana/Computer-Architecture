#ifndef _LATCH_H_
#define _LATCH_H_
#include "instruction.h"
#include <bitset>

class latch {
public:
	byte opcode;
	byte Rdest, Rsrc1, Rsrc2;	
	latch ()
	{
		opcode = 0;     //initial default op to nop
		
	}	
	
	inline const instruction *control()
	{
		return &instructions[opcode];
	}
	
	virtual bool isReady()
	{
		return true;
	}
	
	virtual void reset()
	{
		Rdest = 0;
		Rsrc1 = 0;
		Rsrc2 = 0;
		opcode = 0;
	}
};

class IDl : public latch {
public:
	uint32_t immediate;
	bool predict_taken;
	uint32_t PC;
	uint32_t recoveryPC; // Added a recovery Program Counter here
	uint32_t bpHistory;
	uint32_t state2bit;  // State of Finite state machine
	uint32_t address2bit;// address of finite state machine in table for 2 bit predictor

	std::bitset<10> index2level_c = 0b0000000000; // initialise a global history buffer copy , incase we mispredict
};

class DEl : public latch {
public:
	uint32_t immediate;
	int32_t  Rsrc1Val, Rsrc2Val;
	uint32_t recoveryPC;   // Added a recovery Program Counter here
	uint32_t state2bit;	   // State of Finite state machine
	uint32_t address2bit;  // address of finite state machine in table for 2 bit predictor

	std::bitset<10> index2level_c = 0b0000000000; // initialise a global history buffer copy , incase we mispredict

	bool predict_taken;
	bool ready;				//indicate whether the data in the latch is ready or not	
	
	void setRsrc1Ready(bool _ready)
	{
		Rsrc1Ready=_ready;
		ready = (Rsrc1Ready && Rsrc2Ready);
	}
	
	void setRsrc2Ready(bool _ready)
	{
		Rsrc2Ready=_ready;
		ready = (Rsrc1Ready && Rsrc2Ready);		
	}
	
	virtual bool isReady()
	{
		return ready;
	}
	
	virtual void reset()
	{
		Rdest = 0;
		Rsrc1 = 0;
		Rsrc2 = 0;
		opcode = 0;
		ready = false;
	}	
	
private:
	bool Rsrc1Ready, Rsrc2Ready;
};

class EMl : public latch {
public:
	uint32_t aluresult;
	int32_t  Rsrc1Val, Rsrc2Val;
};

class MWl : public latch {
public:
	uint32_t aluresult, mem_data;
	int32_t  Rsrc2Val, Rsrc1Val;
};

class PipelineStage {
public:
	//bool busy;
	bool OBF;	//output buffer full flag
	bool IBF;	//input buffer full flag	
	PipelineStage()
	{
		OBF=false;
		IBF=false;
	}
};

class InstructionFetchStage: public PipelineStage {
public:
	cpu_core *core;
	IDl right;
	InstructionFetchStage(cpu_core *c)
	{
		core = c; make_nop();
	}


	void Execute();
	void make_nop()
	{
		right.reset();
	}
};

class InstructionDecodeStage: public PipelineStage {
public:
	cpu_core *core;
	IDl left;
	DEl right;
	InstructionDecodeStage(cpu_core *c)
	{
		core = c; make_nop();
	}


	void Execute();
	void Shift();
	void make_nop();
};

class ExecuteStage: public PipelineStage {
public:
	cpu_core *core;
	DEl left;
	EMl right;
	int busyCycles;

	ExecuteStage(cpu_core *c)
	{
		core = c; make_nop();
		busyCycles=0;
	}


	void Execute();
	void Shift();
	void DoForwarding();
	void make_nop()
	{
		busyCycles=0;
		right.reset();
		left.reset();
	}
};

class MemoryStage: public PipelineStage {
public:
	cpu_core *core;
	EMl left;
	MWl right;
	MemoryStage(cpu_core *c)
	{
		core = c; make_nop();
	}


	void Execute();
	void Shift();
	void DoForwarding();
	void make_nop()
	{
		right.reset();
		left.reset();
	}
};

class WriteBackStage: public PipelineStage {
public:
	cpu_core *core;
	MWl left;
	WriteBackStage(cpu_core *c)
	{
		core = c; make_nop();
	}


	void Execute();
	void Shift();
	void make_nop()
	{
		left.reset();	
	}
};

#endif /* _LATCH_H_ */
