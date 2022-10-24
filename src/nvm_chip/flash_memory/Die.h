#ifndef DIE_H
#define DIE_H

#include "../../sim/Sim_Event.h"
#include "FlashTypes.h"
#include "Plane.h"
#include "Superblock.h" // including Superblock.h

namespace NVM
{
	namespace FlashMemory
	{
		enum class DieStatus { BUSY, IDLE };
		class Die
		{
		public:
			Die(unsigned int PlanesNoPerDie, unsigned int BlocksNoPerPlane, unsigned int PagesNoPerBlock);
			~Die();
			Plane** Planes;
			Superblock** Superblocks; // array of superblocks
			Block*** BlocksOfDie; // array of arrays for blocks
			int* SuperblocksAllocation;
			unsigned int Plane_no;
			unsigned int Superblock_no;
			unsigned int BlocksPerPlane_no;
			DieStatus Status;
			MQSimEngine::Sim_Event* CommandFinishEvent;
			sim_time_type Expected_finish_time;
			sim_time_type RemainingSuspendedExecTime;//used to support suspend command
			Flash_Command* CurrentCMD, *SuspendedCMD;
			bool Suspended;

			sim_time_type STAT_TotalProgramTime, STAT_TotalReadTime, STAT_TotalEraseTime, STAT_TotalXferTime;
		};
	}
}
#endif // !DIE_H
