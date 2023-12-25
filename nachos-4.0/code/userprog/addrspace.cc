// addrspace.cc 
//	Routines to manage address spaces (executing user programs).
//
//	In order to run a user program, you must:
//
//	1. link with the -n -T 0 option 
//	2. run coff2noff to convert the object file to Nachos format
//		(Nachos object code format is essentially just a simpler
//		version of the UNIX executable object code format)
//	3. load the NOFF file into the Nachos file system
//		(if you haven't implemented the file system yet, you
//		don't need to do this last step)
//
// Copyright (c) 1992-1996 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "main.h"
#include "addrspace.h"
#include "machine.h"
#include "noff.h"

bool AddrSpace::usedPhyPage[NumPhysPages] = {0};

//----------------------------------------------------------------------
// SwapHeader
// 	Do little endian to big endian conversion on the bytes in the 
//	object file header, in case the file was generated on a little
//	endian machine, and we're now running on a big endian machine.
//----------------------------------------------------------------------

static void 
SwapHeader (NoffHeader *noffH)
{
    noffH->noffMagic = WordToHost(noffH->noffMagic);
    noffH->code.size = WordToHost(noffH->code.size);
    noffH->code.virtualAddr = WordToHost(noffH->code.virtualAddr);
    noffH->code.inFileAddr = WordToHost(noffH->code.inFileAddr);
    noffH->initData.size = WordToHost(noffH->initData.size);
    noffH->initData.virtualAddr = WordToHost(noffH->initData.virtualAddr);
    noffH->initData.inFileAddr = WordToHost(noffH->initData.inFileAddr);
    noffH->uninitData.size = WordToHost(noffH->uninitData.size);
    noffH->uninitData.virtualAddr = WordToHost(noffH->uninitData.virtualAddr);
    noffH->uninitData.inFileAddr = WordToHost(noffH->uninitData.inFileAddr);
}

//----------------------------------------------------------------------
// AddrSpace::AddrSpace
// 	Create an address space to run a user program.
//	Set up the translation from program memory to physical 
//	memory.  For now, this is really simple (1:1), since we are
//	only uniprogramming, and we have a single unsegmented page table
//----------------------------------------------------------------------

AddrSpace::AddrSpace()
{
    // MemoryManagement
    // The pages required is more than NumPhysPages(32), depending on noffH -> Initial when loading
    /*
    pageTable = new TranslationEntry[NumPhysPages];
    for (unsigned int i = 0; i < NumPhysPages; i++) {
	pageTable[i].virtualPage = i;	// for now, virt page # = phys page #
	pageTable[i].physicalPage = i;
//	pageTable[i].physicalPage = 0;
	pageTable[i].valid = TRUE;
//	pageTable[i].valid = FALSE;
	pageTable[i].use = FALSE;
	pageTable[i].dirty = FALSE;
	pageTable[i].readOnly = FALSE;  
    }
    */
    
    // zero out the entire address space
//    bzero(kernel->machine->mainMemory, MemorySize);
}

//----------------------------------------------------------------------
// AddrSpace::~AddrSpace
// 	Dealloate an address space.
//----------------------------------------------------------------------

AddrSpace::~AddrSpace()
{
   for(int i = 0; i < numPages; i++)
        AddrSpace::usedPhyPage[pageTable[i].physicalPage] = false;
   delete pageTable;
}


//----------------------------------------------------------------------
// AddrSpace::Load
// 	Load a user program into memory from a file.
//
//	Assumes that the page table has been initialized, and that
//	the object code file is in NOFF format.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

int AddrSpace::TranalateVir2Phys(int virAddr){
    int ret_physAddr = 0;
    int vpn = (unsigned) virAddr / PageSize;
    int offset = (unsigned) virAddr % PageSize;
    ret_physAddr = pageTable[vpn].physicalPage * PageSize + offset;
    return ret_physAddr;
}

bool 
AddrSpace::Load(char *fileName) 
{
    executable = kernel->fileSystem->Open(fileName);
    // NoffHeader noffH;
    unsigned int size;

    if (executable == NULL) {
	cerr << "Unable to open file " << fileName << "\n";
	return FALSE;
    }
    executable->ReadAt((char *)&noffH, sizeof(noffH), 0);
    if ((noffH.noffMagic != NOFFMAGIC) && 
		(WordToHost(noffH.noffMagic) == NOFFMAGIC))
    	SwapHeader(&noffH);
    ASSERT(noffH.noffMagic == NOFFMAGIC);

// how big is address space?
    size = noffH.code.size + noffH.initData.size + noffH.uninitData.size 
			+ UserStackSize;	// we need to increase the size
						// to leave room for the stack
    numPages = divRoundUp(size, PageSize);
    size = numPages * PageSize;
    DEBUG(dbgAddr, "Initializing address space: " << numPages << ", " << size);
//	cout << "number of pages of " << fileName<< " is "<<numPages<<endl;
    pageTable = new TranslationEntry[numPages];

    // Memory management: 
    // Indexing: always i for pageTable, j for frameTable, k for swapTable
    /*
    for(unsigned int i = 0, j = 0; i < numPages; i++) {
        pageTable[i].virtualPage = i;
        while(j < NumPhysPages && AddrSpace::usedPhyPage[j] == true)
            j++;
        AddrSpace::usedPhyPage[j] = true;
        pageTable[i].physicalPage = j;
        pageTable[i].valid = true;
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
    }
    */

    int i, j, k;
    for(i = 0, j = 0; i < numPages && j < NumPhysPages; i++, j++){
        // use physical frame
        while(kernel -> frameTable[j].valid == false) j++;
        AddrSpace::usedPhyPage[j] = true;
        pageTable[i].virtualPage = 1024; // VM no need
        pageTable[i].physicalPage = j; // in physical memory
        pageTable[i].valid = true; // can be used
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
        // update frameTable
        kernel -> frameTable[j].valid = false; // occupied
        kernel -> frameTable[j].addrspace = this;
        kernel -> frameTable[j].vpn = i;
    }
    for(k = 0; i < numPages && k < 1024; i++, k++){
        // use VM: find an available disk segment
        while(kernel -> swapTable[k].valid == false) k++;

        // update swapTable
        kernel-> swapTable[k].valid = false;
        kernel-> swapTable[k].addrspace = this;
        kernel-> swapTable[k].vpn = i;

        // update pageTable
        AddrSpace::usedPhyPage[NumPhysPages] = true;
        pageTable[i].valid = false; // can't be used
        pageTable[i].virtualPage = k; // in VM
        pageTable[i].physicalPage = NumPhysPages; // not in physical frame
        pageTable[i].use = false;
        pageTable[i].dirty = false;
        pageTable[i].readOnly = false;
    }
    /*
    ASSERT(numPages <= NumPhysPages);		// check we're not trying
						// to run anything too big --
						// at least until we have
						// virtual memory

    */

// then, copy in the code and data segments into memory
// Memory management: or disk, if memory are all filled
    i = 0;
    if (noffH.code.size > 0) {
        DEBUG(dbgAddr, "Initializing code segment.");
	DEBUG(dbgAddr, noffH.code.virtualAddr << ", " << noffH.code.size);

        // load it one by one
        // since the code segment always start with viraddr = 0
        int codePages = divRoundUp(noffH.code.size, PageSize);
        for(i = 0; i < codePages; i++){
            // For each page i, find physical or vm
            if(pageTable[i].valid){
                // in physical frame 
                int physAddr_code = kernel -> memoryManager -> TransAddr(this, i* PageSize);
                executable -> ReadAt(&(kernel->machine->mainMemory[physAddr_code]), PageSize, noffH.code.inFileAddr+i*PageSize);
            }
            else{
                // not valid -> in vm
                int k = pageTable[i].virtualPage;
                char *outBuffer = new char[PageSize];
                executable -> ReadAt(outBuffer, PageSize, noffH.code.inFileAddr+i*PageSize);
                kernel -> swap -> WriteSector(k, outBuffer); 
            }
        }



    }
    if (noffH.initData.size > 0) {
        DEBUG(dbgAddr, "Initializing data segment.");
	DEBUG(dbgAddr, noffH.initData.virtualAddr << ", " << noffH.initData.size);
        for(; i < numPages; i++){
            if(pageTable[i].valid){
                // in physical frame 
                int physAddr_data = kernel -> memoryManager -> TransAddr(this, i* PageSize);
                executable -> ReadAt(&(kernel->machine->mainMemory[physAddr_data]), PageSize, noffH.initData.inFileAddr+i*PageSize);
            }
            else{
                // not valid -> in vm
                int k = pageTable[i].virtualPage;
                char *outBuffer = new char[PageSize];
                executable -> ReadAt(outBuffer, PageSize, noffH.initData.inFileAddr+i*PageSize);
                kernel -> swap -> WriteSector(k, outBuffer); 
            }
        }
        int initData_physAddr = TranalateVir2Phys(noffH.initData.virtualAddr);
        executable -> ReadAt(&(kernel->machine->mainMemory[initData_physAddr]), noffH.initData.size, noffH.initData.inFileAddr+i*PageSize);
    }
    delete executable;			// close file
    return TRUE;			// success
}

//----------------------------------------------------------------------
// AddrSpace::Execute
// 	Run a user program.  Load the executable into memory, then
//	(for now) use our own thread to run it.
//
//	"fileName" is the file containing the object code to load into memory
//----------------------------------------------------------------------

void 
AddrSpace::Execute(char *fileName) 
{
    if (!Load(fileName)) {
	cout << "inside !Load(FileName)" << endl;
	return;				// executable not found
    }

    //kernel->currentThread->space = this;
    this->InitRegisters();		// set the initial register values
    this->RestoreState();		// load page table register

    kernel->machine->Run();		// jump to the user progam

    ASSERTNOTREACHED();			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}


//----------------------------------------------------------------------
// AddrSpace::InitRegisters
// 	Set the initial values for the user-level register set.
//
// 	We write these directly into the "machine" registers, so
//	that we can immediately jump to user code.  Note that these
//	will be saved/restored into the currentThread->userRegisters
//	when this thread is context switched out.
//----------------------------------------------------------------------

void
AddrSpace::InitRegisters()
{
    Machine *machine = kernel->machine;
    int i;

    for (i = 0; i < NumTotalRegs; i++)
	machine->WriteRegister(i, 0);

    // Initial program counter -- must be location of "Start"
    machine->WriteRegister(PCReg, 0);	

    // Need to also tell MIPS where next instruction is, because
    // of branch delay possibility
    machine->WriteRegister(NextPCReg, 4);

   // Set the stack register to the end of the address space, where we
   // allocated the stack; but subtract off a bit, to make sure we don't
   // accidentally reference off the end!
    machine->WriteRegister(StackReg, numPages * PageSize - 16);
    DEBUG(dbgAddr, "Initializing stack pointer: " << numPages * PageSize - 16);
}

//----------------------------------------------------------------------
// AddrSpace::SaveState
// 	On a context switch, save any machine state, specific
//	to this address space, that needs saving.
//
//	For now, don't need to save anything!
//----------------------------------------------------------------------

void AddrSpace::SaveState() 
{
        pageTable=kernel->machine->pageTable;
        numPages=kernel->machine->pageTableSize;
}

//----------------------------------------------------------------------
// AddrSpace::RestoreState
// 	On a context switch, restore the machine state so that
//	this address space can run.
//
//      For now, tell the machine where to find the page table.
//----------------------------------------------------------------------

void AddrSpace::RestoreState() 
{
    kernel->machine->pageTable = pageTable;
    kernel->machine->pageTableSize = numPages;
}

int MemoryManager::TransAddr(AddrSpace *space, int virAddr){
    int physAddr = 0;
    int vpn = (unsigned) virAddr / PageSize;
    int offset = (unsigned) virAddr % PageSize;
    ASSERT(space -> pageTable[vpn].valid); // should be valid before translation
    physAddr = space -> pageTable[vpn].physicalPage * PageSize + offset;
    return physAddr;
}
