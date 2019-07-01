#include "cpu.h"
#include "stages.h"
#include <assert.h>
#include <stdio.h>
#include <bitset>

// the three operands are encoded inside a single uint16_t. This method cracks them open
static void inline decode_ops(uint16_t input, byte *dest, byte *src1, byte *src2)
{
	*dest = input & 0x001F;
	*src1 = (input & 0x03E0) >> 5;
	*src2 = (input & 0x7C00) >> 10;
}

static bool inline isBranch(IDl latch)
{
	return latch.control()->branch;
}

/**
   The execute implementations. These actually perform the action of the stage.
 **/

// Instructions are fetched from memory in this stage, and passed into the CPU's ID latch
void InstructionFetchStage::Execute()
{
	if (OBF) {
		return;
	}
	IBF=false;
	
	right.opcode = core->mem->get<byte>(core->PC);
	uint16_t operands = core->mem->get<uint16_t>(core->PC + 1);
	decode_ops(operands, &right.Rdest, &right.Rsrc1, &right.Rsrc2);
	right.immediate = core->mem->get<uint32_t>(core->PC + 3);

	// <CAR_PA1_HOOK2> start
	// !!! Assume an oracle BTB; i.e, we know if the current instruction is a branch or not and its target address.!!!
	// !!! Taken branch target address is stored in "right.immediate"!!!	

	switch(core->branchPredictor) { // Using switch for the branch predictor number
	// 0 = Always not taken
	// 1 = Always taken
	// 2 = 2 Bit Predictor
	// 3 = 2 Level Predictor (also known as 2 Bit history predictor)

 	case 0: // Always Not Taken
		if (isBranch(right)) { 				// check if its a branch
			right.predict_taken = false; 	// right.predict_taken is set to false
		} else {
			right.predict_taken = false;
		}
		core->PC += 8; 						// Update program counter to move on
		break;
	case 1: // Always Taken
		if (isBranch(right)) { 					// check if its a branch
			right.predict_taken = true; 		// right.predict_taken set to true, because we are predicting it true for every case
			right.recoveryPC = core->PC += 8; 	// store the program counter in a recovery program counter
			core->PC= right.immediate;			// set the program counter to the branch target address
		} else {
			right.predict_taken = false; 		// if its not a branch continue as usual
			core->PC+=8; 						// update the program counter and move on
		}
		break;

	case 2: // 2 bit predictor

	int index2bit; 								// initialise an index as an integer that we'll use to access the table of states
		if (isBranch(right)) { 					// check if its a branch
			index2bit = ( core->PC >> 3 ) % 1024;//use the counter shifted by 3 to ignore least significant and mod 1024 to access states
			right.state2bit = core->table2bit[index2bit];// save the state in a variable
			right.address2bit = core->PC;
			int state = right.state2bit; 		// save the state of the finite state machine of 2bit predictor in a variable

			//WEAK NOT TAKEN AND STRONG NOT TAKEN
			if (state<2) {						// State 0 = Strong not taken, State 1 = Weak not taken
				right.predict_taken = false;	// Set predict to false i.e. not taken
				core->PC += 8;					// Update program counter
			}
			//WEAK TAKEN AND STRONG TAKEN
			if (state>1) {						// State 2 = Weak Taken, State 3 = Strong Taken
				right.predict_taken = true;		// Predict taken
				right.recoveryPC = core->PC += 8; // Store Program counter as a recovery program coutner
				core->PC=right.immediate; 		// Set program counter the target branch address
			}
		}
		else {
			right.predict_taken=false;			// Again, if its not a branch continue as usual
			core->PC+=8;						// Update program counter
		}
		break;

	case 3: // 2 level history predictor

		if (isBranch(right)) {								// Check if its a branch
			int idx = (int)(core->index2level.to_ulong());	// Convert index which was a 10 bit binary representing last 10 branch outcomes

			right.state2bit = core->table2bit[idx];  	// Access the table of states at that index converted ^
			right.address2bit = core->PC;
			right.index2level_c = core->index2level;	// copy index to a backup if in execute our prediction is wrong
			std::bitset<10>hit = 0b0000000001;			// another 10 bitset with last bit as 1, to help change our index later on

			int state = right.state2bit;				// state stored in variable again

			//WEAK NOT TAKEN AND STRONG NOT TAKEN
			if (state<2) {								// State 0 = Strong not taken, State 1 = Weak not taken
				right.predict_taken = false;			// Set Predict to false i.e. not predict
				core->index2level <<= 1;				// shift in by 1
				core->PC += 8;							// Update program counter
			}

			//WEAK TAKEN AND STRONG TAKEN
			if (state>1) {
				right.predict_taken = true;				// State 2 = Weak Taken, State 3 = Strong Taken
				right.recoveryPC = core->PC += 8;		// Store updated program counter in a recovery PC variable
				core->index2level <<= 1;  				// Shift in
				core->index2level |= hit;				// Add 1 for the predicted taken
				core->PC=right.immediate;				// Update PC to target branch address
			}
		}
		else {
			right.predict_taken=false;					// otherwise continue as usual
			core->PC+=8;
		}
		break;
	// <CAR_PA1_HOOK2> end
	}
	OBF=true;
}

void InstructionDecodeStage::Execute()
{
	if (!IBF || OBF) {	//no input or output is not read by the next stage
		return;
	}
	IBF=false;
	
	core->registers[0].value = 0; // wire register 0 to zero for all register reads
	right.Rsrc1Val = *(int32_t*) &core->registers[left.Rsrc1].value;
	right.Rsrc2Val = *(int32_t*) &core->registers[left.Rsrc2].value;
	right.immediate = left.immediate;
	right.Rsrc1 = left.Rsrc1;
	right.Rsrc2 = left.Rsrc2;
	right.Rdest = left.Rdest;
	right.opcode = left.opcode;
	right.predict_taken = left.predict_taken;
	right.recoveryPC = left.recoveryPC;							// Carry over the recovery PC
	right.state2bit = left.state2bit;							// Carry over the states of finite state machine for 2 bit predictor
	right.address2bit = left.address2bit;						// Carry over address/index for 2 bit predictor
	right.index2level_c = left.index2level_c;					// carry over index for 2level predictor

	if (right.Rsrc1 && core->registers[right.Rsrc1].lockRefCount) {
		//printf("Rsrc1 (%d) is locked, stall ?\n", right.Rsrc1);
		right.setRsrc1Ready(false);

	} else {
		right.setRsrc1Ready(true);
	}

	if (right.Rsrc2 && core->registers[right.Rsrc2].lockRefCount) {
		//printf("Rsrc2 (%d) is locked, stall ?\n", right.Rsrc2);
		right.setRsrc2Ready(false);
	} else {
		right.setRsrc2Ready(true);
	}

	//lock destination register
	if (right.Rdest && right.control()->register_write) {
		core->registers[right.Rdest].lockRefCount++;
	}

	OBF=true;
}


void ExecuteStage::Execute()
{
	if (!IBF || OBF) {	//no input or output is not read by the next stage
		return;
	} else {
		if (busyCycles==0)
		{
			// copy forward from previous latch
			right.opcode = left.opcode;
			right.Rsrc1 = left.Rsrc1;
			right.Rsrc2 = left.Rsrc2;
			right.Rsrc1Val = left.Rsrc1Val;
			right.Rsrc2Val = left.Rsrc2Val;
			right.Rdest = left.Rdest;	
			//load execution cycles
			busyCycles = left.control()->exe_cycles;
		}
	}
	
// execution stage start
	--busyCycles;
	if (busyCycles) {		//return if we need to wait for more cycles
		//printf("Exe Stage Stall: Wait %d cycles\n",busyCycles);	
		return;
	} 
	//set IBF to 0;
	IBF=false;
		
	uint32_t param;
	
	switch (left.control()->alu_source) {
	case 0: // source from register
		param = left.Rsrc2Val;
		break;

	case 1: // immediate add/sub
		param = left.immediate;
		break;

	case 2: // address calculation
		param = left.immediate + *(uint32_t *)&left.Rsrc1Val;
		break;
	}
	// for those operands which require signed arithmatic.
	int32_t svalue = left.Rsrc1Val;
	int32_t sparam = *(int32_t *)&param;
	int32_t result = sparam;

	if (left.control()->branch) {
		bool taken = (left.opcode == 2 && left.Rsrc1Val == 0) ||
		             (left.opcode == 3 && left.Rsrc1Val >= left.Rsrc2Val) ||
		             (left.opcode == 4 && left.Rsrc1Val != left.Rsrc2Val);
		// if mispredict, nop out IF and ID. (mispredict == prediction and taken differ)
		if (core->verbose) printf(taken ? "taken %d  %d\n" : "nottaken %d  %d\n", left.Rsrc1Val, left.Rsrc2Val);
		if (left.predict_taken != taken) {
			if (core->verbose) printf("\033[32m*** MISPREDICT!\033[0m\n");
			core->ifs.make_nop();
			core->ids.make_nop();
			core->BPMisses++;
			// CASES FOR EXECUTION STAGE In the event a branch is mispredicted
			switch(core->branchPredictor) {
			case 0:
				core->PC=left.immediate;
				break;
			case 1:
				core->PC=left.recoveryPC;
				break;
			case 2: // 2 bit predictor

				//STRONG NOT TAKEN - MISS
				if (left.state2bit==0) {
					core->PC=left.immediate;				// If strong not taken was mispredicted then branch was taken
					core->table2bit[(left.address2bit >> 3) % 1024]+=1; // update the state of the finite state machine
				}
				//WEAK NOT TAKEN - MISS
				if (left.state2bit==1) {
					core->PC=left.immediate;				// if weak not taken was mis-predicted then branch was taken
					core->table2bit[(left.address2bit >> 3) % 1024]+=1; // update the state of the finite state machine
				}
				//WEAK TAKEN - MISS
				if (left.state2bit==2) {
					core->PC=left.recoveryPC;				// if weak taken was mis predicted then branch was not taken
					core->table2bit[(left.address2bit >> 3) % 1024]-=1; // update the state of the finite state machine
				}
				//STRONG TAKEN - MISS
				if (left.state2bit==3) {
					core->PC=left.recoveryPC;				// if strong taken was mis predicted then branch was taken
					core->table2bit[(left.address2bit >> 3) % 1024]-=1; // update the state of the finite state machine
				}

				break;

			case 3: // 2 level predictor
				std::bitset<10>hit = 0b0000000001;   		// initialise a bitset to update global history buffer by 1
				core->index2level = left.index2level_c; 	// copy index back as our prediction was wrong and we need to fix that
				int idx = (int)(core->index2level.to_ulong()); // convert to int


				//STRONG NOT TAKEN - MISS
				if (left.state2bit==0) {
					core->PC=left.immediate;				// set Program counter to target branch address
					core->index2level <<= 1;				// shift in
					core->index2level |= hit;				// if strong not taken was mispredicted then branch was taken
															// update the state of the finite state machine
					core->table2bit[idx]+=1;
				}
				//WEAK NOT TAKEN - MISS
				if (left.state2bit==1) {
					core->PC=left.immediate;				// set Program counter to target branch address
					core->index2level <<= 1;				// shift in
					core->index2level |= hit;				// if weak not taken was mispredicted then branch was taken
															// update the state of the finite state machine
					core->table2bit[idx]+=1;
				}
				//WEAK TAKEN - MISS
				if (left.state2bit==2) {
					core->PC=left.recoveryPC;				// set Program counter to recovery program counter
					core->index2level <<= 1;				// shift in
					core->table2bit[idx]-=1;				// update the state of the finite state machine
				}
				//STRONG TAKEN - MISS
				if (left.state2bit==3) {
					core->PC=left.recoveryPC;				// set Program counter to recovery program counter
					core->index2level <<= 1;				// shift in
					core->table2bit[idx]-=1; 				// update the state of the finite state machine
				}
				break;
			}

		} else {
			// CASES FOR EXECUTION STAGE In the event a branch is predicted correctly.
			core->BPHits++;
			// 2bit predictor
			if (core->branchPredictor==2) {

				//STRONG NOT TAKEN
				if (left.state2bit==0) {
					core->table2bit[(left.address2bit >> 3) % 1024]+=0; // Finite state machine stays the same
				}
				//WEAK NOT TAKEN
				if (left.state2bit==1) {
					core->table2bit[(left.address2bit >> 3) % 1024]-=1; // Finite state machine goes from 1 to 0
				}
				//WEAK TAKEN
				if (left.state2bit==2) {
					core->table2bit[(left.address2bit >> 3) % 1024]+=1;	// Finite state machine goes from 2 to 3
				}
				//STRONG TAKEN
				if (left.state2bit==3) {
					core->table2bit[(left.address2bit >> 3) % 1024]+=0; // Finite state machine stays the same
				}

			}
			//2level
			if (core->branchPredictor==3) {
				int idx = (int)(left.index2level_c.to_ulong());
				//STRONG NOT TAKEN
				if (left.state2bit==0) {
					core->table2bit[idx]+=0;		// Finite state machine stays the same
				}
				//WEAK NOT TAKEN
				if (left.state2bit==1) {
					core->table2bit[idx]-=1;		// Finite state machine goes from 1 to 0
				}
				//WEAK TAKEN
				if (left.state2bit==2) {
					core->table2bit[idx]+=1;		// Finite state machine goes from 2 to 3
				}
				//STRONG TAKEN
				if (left.state2bit==3) {
					core->table2bit[idx]+=0;		// Finite state machine stays the same
				}
			}
		}
	}

	switch (left.control()->alu_operation) {
	case 0:
		// do nothing, this operation does not require an alu op (copy forward)
		break;

	case 1:
		// do a signed add of reg1 to param
		result = svalue + sparam;
		break;

	case 2:
		// do a signed subtract of param from reg1
		result = svalue - sparam;
		break;
	}
	right.aluresult = *(uint32_t *)&result;
	OBF=true;	
}


void MemoryStage::Execute()
{
	if (!IBF || OBF) {	//no input or output is not read by the next stage
		return;
	}
	IBF=false;	

	const instruction *control = left.control();

	right.aluresult = left.aluresult;
	right.mem_data = 0;
	if (control->mem_read) {
		if (control->mem_read == 1) {
			right.mem_data = core->mem->get<byte>(left.aluresult);
		}
		else if (control->mem_read == 4) {
			right.mem_data = core->mem->get<uint32_t>(left.aluresult);
		}
	}
	else if (control->mem_write) {
		if (control->mem_write == 1) {
			core->mem->set<byte>(left.aluresult, left.Rsrc2Val);
		}
		else if (control->mem_write == 4) {
			core->mem->set<uint32_t>(left.aluresult, left.Rsrc2Val);
		}
	}

	right.opcode = left.opcode;
	right.Rsrc1Val = left.Rsrc1Val;
	right.Rsrc2Val = left.Rsrc2Val;
	right.Rdest = left.Rdest;
	OBF=true;
}


void WriteBackStage::Execute()
{
	if (!IBF) {	//no input 
		return;
	}
	IBF=false;
	
	const instruction *control = left.control();

	if (control->special_case != NULL) {
		control->special_case(core);
	}
	if (control->register_write) {
		if (control->mem_to_register) {
			// write the mem_data into register Rdest
			core->registers[left.Rdest].value = left.mem_data;
		}
		else {
			// write the alu result into register Rdest
			core->registers[left.Rdest].value = left.aluresult;
		}
		core->registers[0].value = 0; // wire back to zero
		assert(core->registers[left.Rdest].lockRefCount>0);
		core->registers[left.Rdest].lockRefCount--;	
	}
}


void InstructionDecodeStage::Shift()
{
	if (IBF || OBF || !right.isReady())		//if OBF is set or data is not ready in the right latch, we cannot fetch from the previous stage
		return;

	if (core->ifs.OBF && core->ifs.right.isReady()) {
		left = core->ifs.right;
		core->ifs.OBF=false;
	} else {
		//create a nop if the previous stage is not ready
		left.reset();
	}
	IBF=true;	
}


void ExecuteStage::Shift()
{
	//if (busyCycles>0)
	//	return;

	if (IBF || OBF || !right.isReady())		//if OBF is set or data is not ready in the right latch, we cannot fetch from the previous stage
		return;
	
	if (core->ids.OBF && core->ids.right.isReady()) {
		left = core->ids.right;
		core->ids.OBF=false;
	} else {
		//create a nop if the previous stage is not ready
		left.reset();
	}
	IBF=true;	
}


void MemoryStage::Shift()
{
	if (IBF || OBF || !right.isReady())		//if OBF is set or data is not ready in the right latch, we cannot fetch from the previous stage
		return;
	
	if (core->exs.OBF && core->exs.right.isReady()) {
		left = core->exs.right;
		core->exs.OBF=false;
	} else {
		//create a nop if the previous stage is not ready		
		left.reset();
	}
	IBF=true;

}


void WriteBackStage::Shift()
{
	if (IBF)
		return;
	
	if (core->mys.OBF && core->mys.right.isReady()) {
		left = core->mys.right;
		core->mys.OBF=false;
		IBF=true;		
	} else {
		//create a nop if the previous stage is not ready
		left.reset();
	}
	IBF=true;	
}

void ExecuteStage::DoForwarding()
{
	if (busyCycles>0)
		return;
	
	// If the previous instruction is attempting a READ of the same register the instruction
	// in this stage is supposed to WRITE, then here, update the previous stage's right latch
	// with the value coming out of the ALU. (also, not reading zero reg)
	if (!core->exs.right.control()->mem_read && core->exs.right.Rdest != 0 && core->exs.right.control()->register_write) {
		if (core->exs.right.Rdest == core->ids.right.Rsrc1) {
			core->ids.right.Rsrc1Val = core->exs.right.aluresult;
			core->ids.right.setRsrc1Ready(true);
			if (core->verbose) printf("\033[34m*** FORWARDex1\033[0m:  %08x going to ID/EX's Rsrc1Val \n",
				                       core->exs.right.aluresult);
		}
		if (core->exs.right.Rdest == core->ids.right.Rsrc2) {
			core->ids.right.Rsrc2Val = core->exs.right.aluresult;
			core->ids.right.setRsrc2Ready(true);
			if (core->verbose) printf("\033[34m*** FORWARDex2\033[0m:  %08x going to ID/EX's Rsrc2Val \n",
				                       core->exs.right.aluresult);
		}
	}
}


void MemoryStage::DoForwarding()
{
	// If the next to previous instruction (IDS) is attempting a READ of the same register the instruction
	// in this stage is supposed to WRITE, then here, update the next-to-previous stage's right latch
	// with the value coming out of the this stage. (also, the read is not on zero)
	if (core->mys.right.Rdest != 0 && core->mys.right.control()->register_write) {
		if (core->exs.right.Rdest != core->ids.right.Rsrc1 &&
		    core->mys.right.Rdest == core->ids.right.Rsrc1) {
			core->ids.right.Rsrc1Val =
			   core->mys.right.control()->mem_read ? core->mys.right.mem_data : core->mys.right.aluresult;
			core->ids.right.setRsrc1Ready(true);
		
			if (core->verbose) printf("\033[34m*** FORWARDmem1\033[0m: %08x going to ID/EX's Rsrc1Val \n",
				                       core->ids.right.Rsrc1Val);
		}
		if (core->exs.right.Rdest != core->ids.right.Rsrc2 &&
		    core->mys.right.Rdest == core->ids.right.Rsrc2) {
			core->ids.right.Rsrc2Val =
			   core->mys.right.control()->mem_read ? core->mys.right.mem_data : core->mys.right.aluresult;
			core->ids.right.setRsrc2Ready(true);
		
			if (core->verbose) printf("\033[34m*** FORWARDmem2\033[0m: %08x going to ID/EX's Rsrc2Val \n",
				                       core->ids.right.Rsrc2Val);
		}
	}
}

void InstructionDecodeStage::make_nop()
{
	if (right.Rdest && right.control()->register_write) {
		core->registers[right.Rdest].lockRefCount--;
		//printf("Lock dest register:R%d ref:%d",right.Rdest,core->registers[right.Rdest].lockRefCount)
	}

	left.reset();
	right.reset();
	right.setRsrc1Ready(true);
	right.setRsrc2Ready(true);
}

