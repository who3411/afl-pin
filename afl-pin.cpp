#include "pin.H"
#include <asm/unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <sstream>
#include <sys/types.h>
#include <sys/shm.h>
#include "afl/config.h"

#if PIN_PRODUCT_VERSION_MAJOR < 3
	#if PIN_PRODUCT_VERSION_MINOR < 6
		#warn "WARNING: you should use pintool >= 3.6!"
	#endif
#endif

typedef uint16_t map_t;

static int libs = 0, doit = 0, imageload = 0;
static ADDRINT exe_start = 0, exe_end = 0;
static map_t prev_id;
RTN entrypoint, exitpoint;
static ADDRINT forkserver = 0, exitfunc = 0;
static bool forkserver_installed = false;
#ifndef DEBUG
static uint8_t *trace_bits = NULL;
#endif

KNOB<BOOL> KnobAlt(KNOB_MODE_WRITEONCE, "pintool", "alternative", "0", "use alternative mode for bb reporting");
KNOB<BOOL> KnobLibs(KNOB_MODE_WRITEONCE, "pintool", "libs", "0", "also report basic bocks of dynamic libraries");
KNOB<BOOL> KnobForkserver(KNOB_MODE_WRITEONCE, "pintool", "forkserver", "0", "install a fork server into main");
KNOB<string> KnobEntrypoint(KNOB_MODE_WRITEONCE, "pintool", "entrypoint", "main", "install a fork server into this function (name or 0xaddr)");
KNOB<string> KnobExitpoint(KNOB_MODE_WRITEONCE, "pintool", "exitpoint", "", "exit the program when this function or address is reached");

VOID Instruction_Node(ADDRINT addr, uint32_t c){
#ifndef DEBUG
	*(uint32_t *)(trace_bits + MAP_SIZE) += c;
#endif
}

VOID Instruction_Edge(ADDRINT target_addr){
	if (doit == 0)
		return;
	map_t id = (map_t)(((uintptr_t)target_addr) >> 1);
#ifdef DEBUG
	std::cerr << "BB: 0x" << hex << target_addr << " and id 0x" << (prev_id ^ id) << "(0x" << prev_id << " ^ 0x" << id << ")" << std::endl;
#else
	trace_bits[prev_id ^ id]++;
#endif
	prev_id = id >> 1;
}

VOID Trace(TRACE trace, VOID *v){
	if (exe_start != 0 && !(exe_start <= TRACE_Address(trace) && TRACE_Address(trace) <= exe_end))
		return;

	for(BBL bbl = TRACE_BblHead(trace);BBL_Valid(bbl);bbl = BBL_Next(bbl)){
		INS ihead = BBL_InsHead(bbl);
		INS itail = BBL_InsTail(bbl);

		INS_InsertCall(ihead, IPOINT_BEFORE, AFUNPTR(Instruction_Node), IARG_FAST_ANALYSIS_CALL, IARG_ADDRINT, BBL_Address(bbl), IARG_UINT32, BBL_NumIns(bbl), IARG_END);
		if (INS_IsRet(itail) == false && (INS_Category(itail) == XED_CATEGORY_COND_BR || INS_IsIndirectBranchOrCall(itail) == true)){
			INS_InsertCall(itail, IPOINT_BEFORE, AFUNPTR(Instruction_Edge), IARG_BRANCH_TARGET_ADDR, IARG_END);
		}
	}
}

static VOID startForkServer(CONTEXT *ctxt, THREADID tid) {
  if (forkserver_installed == true)
    return;
  forkserver_installed = true;
#ifdef DEBUG
  fprintf(stderr, "DEBUG: starting forkserver()\n");
#endif
  PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(forkserver), NULL, PIN_PARG_END());
}

static VOID DTearly() { PIN_Detach(); }

static VOID exitFunction(CONTEXT *ctxt, THREADID tid) {
	PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, AFUNPTR(exitfunc), NULL, PIN_PARG_END());
}

VOID Image(IMG img, VOID *v){
#ifdef DEBUG
	std::cerr << "DEBUG: image load no " << imageload << " for " << IMG_Name(img) << " from " << hex << IMG_LowAddress(img) << " to " << IMG_HighAddress(img) << std::endl ;
#endif
	if (imageload == 0) {
		if (libs == 0) {
			exe_start = IMG_LowAddress(img);
			exe_end = IMG_HighAddress(img);
		}
#ifdef DEBUG
		entrypoint = RTN_FindByName(img, KnobEntrypoint.Value().c_str());
#endif
		if (KnobForkserver.Value()) {
			entrypoint = RTN_FindByName(img, KnobEntrypoint.Value().c_str());
			if (entrypoint == RTN_Invalid()) {
				entrypoint = RTN_FindByAddress(strtoul(KnobEntrypoint.Value().c_str(), NULL, 16));
				if (entrypoint == RTN_Invalid()) {
					entrypoint = RTN_FindByName(img, "__libc_start_main");
					if (entrypoint == RTN_Invalid()) {
						fprintf(stderr, "Error: could not find entrypoint %s\n", KnobEntrypoint.Value().c_str());
						exit(-1);
					}
				}
			}
		}
		if (KnobExitpoint.Value().length() > 0) {
			exitpoint = RTN_FindByName(img, KnobExitpoint.Value().c_str());
			if (exitpoint == RTN_Invalid()) {
				exitpoint = RTN_FindByAddress(strtoul(KnobExitpoint.Value().c_str(), NULL, 16));
				if (exitpoint == RTN_Invalid()) {
					fprintf(stderr, "Warning: could not find exitpoint %s\n", KnobExitpoint.Value().c_str());
				}
			}
		}
	}
	if (exitpoint != RTN_Invalid() && exitfunc == 0) {
		RTN rtn = RTN_FindByName(img, "_exit");
		if (rtn != RTN_Invalid()) {
			exitfunc = RTN_Address(rtn);
			if (exitfunc != 0) {
				RTN_Open(exitpoint);
				RTN_InsertCall(exitpoint, IPOINT_BEFORE, (AFUNPTR)exitFunction, IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
				RTN_Close(exitpoint);
			}
		}
	}
	if (entrypoint != RTN_Invalid() && IMG_Name(img).find("forkserver.so") != string::npos) {
		RTN rtn = RTN_FindByName(img, "startForkServer");
		if (rtn == RTN_Invalid()) {
			fprintf(stderr, "Error: could not find startForkServer in forkserver.so\n");
			exit(-1);
		}
		forkserver = RTN_Address(rtn);
		RTN_Open(entrypoint);
		RTN_InsertCall(entrypoint, IPOINT_BEFORE, (AFUNPTR)startForkServer, IARG_CONTEXT, IARG_THREAD_ID, IARG_END);
		RTN_InsertCall(entrypoint, IPOINT_AFTER, (AFUNPTR)DTearly, IARG_END);
		RTN_Close(entrypoint);
	}
	++imageload;
}

VOID Fini(INT32 code, VOID *v){
	return;
}

INT32 Usage(){
	std::cerr << "This tool counts the number of dynamic instructions executed" << std::endl;
	std::cerr << std::endl << KNOB_BASE::StringKnobSummary() << std::endl;
	return -1; 
}

VOID AfterForkInChild(THREADID threadid, const CONTEXT* ctxt, VOID * arg) {
#ifdef DEBUG
	cerr << "DEBUG: After fork in child " << threadid << endl;
#endif
	doit = 1;
	prev_id = 0;
}

int main(int argc, char * argv[])
{
#ifndef DEBUG
	char *shmenv;
	int shm_id;
#endif

	PIN_InitSymbols();
	if (PIN_Init(argc, argv))
		return Usage();
	PIN_SetSyntaxIntel();

#ifndef DEBUG
	if ((shmenv = getenv(SHM_ENV_VAR)) == NULL) {
		fprintf(stderr, "Error: AFL environment variable " SHM_ENV_VAR " not set\n");
		exit(-1);
	}
	if ((shm_id = atoi(shmenv)) < 0) {
		fprintf(stderr, "Error: invalid " SHM_ENV_VAR " contents\n");
		exit(-1);
	}
	if ((trace_bits = (u8 *) shmat(shm_id, NULL, 0)) == (void*) -1 || trace_bits == NULL) {
		fprintf(stderr, "Error: " SHM_ENV_VAR " attach failed\n");
		exit(-1);
	}
	if (fcntl(FORKSRV_FD, F_GETFL) == -1 || fcntl(FORKSRV_FD + 1, F_GETFL) == -1) {
		fprintf(stderr, "Error: AFL fork server file descriptors are not open\n");
		exit(-1);
	}
#endif

	entrypoint = RTN_Invalid();
	exitpoint = RTN_Invalid();
	if (libs == 0 || KnobForkserver.Value() || KnobExitpoint.Value().length() > 0)
		IMG_AddInstrumentFunction(Image, 0);
#ifdef DEBUG
	doit = 1;
#endif

	PIN_AddForkFunction(FPOINT_AFTER_IN_CHILD, AfterForkInChild, 0);
	PIN_AddFiniFunction(Fini, 0);
	TRACE_AddInstrumentFunction(Trace, NULL);
	PIN_StartProgram();

	return 0;
}
