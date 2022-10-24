
#include <string>
#include <algorithm>
#include "TSU_Base.h"

#define TRTOSTR(TR) (TR->Type == Transaction_Type::READ ? "Read, " : (TR->Type == Transaction_Type::WRITE ? "Write, " : "Erase, ") )


namespace SSD_Components
{
	TSU_Base* TSU_Base::_my_instance = NULL;

	TSU_Base::TSU_Base(const sim_object_id_type& id, FTL* ftl, NVM_PHY_ONFI_NVDDR2* NVMController, Flash_Scheduling_Type Type,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
		bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled,
		sim_time_type WriteReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForRead,
		sim_time_type EraseReasonableSuspensionTimeForWrite)
		: Sim_Object(id), ftl(ftl), _NVMController(NVMController), type(Type),
		channel_count(ChannelCount), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(DieNoPerChip), plane_no_per_die(PlaneNoPerDie),
		eraseSuspensionEnabled(EraseSuspensionEnabled), programSuspensionEnabled(ProgramSuspensionEnabled),
		writeReasonableSuspensionTimeForRead(WriteReasonableSuspensionTimeForRead), eraseReasonableSuspensionTimeForRead(EraseReasonableSuspensionTimeForRead),
		eraseReasonableSuspensionTimeForWrite(EraseReasonableSuspensionTimeForWrite), opened_scheduling_reqs(0)
	{
		_my_instance = this;
		Round_robin_turn_of_channel = new flash_chip_ID_type[channel_count];
		for (unsigned int channelID = 0; channelID < channel_count; channelID++) {
			Round_robin_turn_of_channel[channelID] = 0;
		}
	}

	TSU_Base::~TSU_Base()
	{
		delete[] Round_robin_turn_of_channel;
	}

	void TSU_Base::Setup_triggers()
	{
		Sim_Object::Setup_triggers();
		_NVMController->ConnectToTransactionServicedSignal(handle_transaction_serviced_signal_from_PHY);
		_NVMController->ConnectToChannelIdleSignal(handle_channel_idle_signal);
		_NVMController->ConnectToChipIdleSignal(handle_chip_idle_signal);
	}

	void TSU_Base::handle_transaction_serviced_signal_from_PHY(NVM_Transaction_Flash* transaction)
	{
		//TSU does nothing. The generator of the transaction will handle it.
	}

	void TSU_Base::handle_channel_idle_signal(flash_channel_ID_type channelID)
	{
		for (unsigned int i = 0; i < _my_instance->chip_no_per_channel; i++) {
			//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
			_my_instance->process_chip_requests(_my_instance->_NVMController->Get_chip(channelID, _my_instance->Round_robin_turn_of_channel[channelID]));
			_my_instance->Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(_my_instance->Round_robin_turn_of_channel[channelID] + 1) % _my_instance->chip_no_per_channel;

			//A transaction has been started, so TSU should stop searching for another chip
			if (_my_instance->_NVMController->Get_channel_status(channelID) == BusChannelStatus::BUSY) {
				break;
			}
		}
	}
	
	void TSU_Base::handle_chip_idle_signal(NVM::FlashMemory::Flash_Chip* chip)
	{
		if (_my_instance->_NVMController->Get_channel_status(chip->ChannelID) == BusChannelStatus::IDLE) {
			_my_instance->process_chip_requests(chip);
		}
	}

	void TSU_Base::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter& xmlwriter)
	{
	}

	//--------------------------------------------------- ADDED BY MAEE ---------------------------------------------------------------------
	unsigned int get_multiplane_command_length(unsigned int planeVector) {
		unsigned int count = 0;
		while (planeVector) {
			count += planeVector & 1;
			planeVector >>= 1;
		}
		return count;
	}
	bool RL_decide_to_wait() {
		return false;
	}

	std::list<TransactionBound*> TSU_Base::bound_transactions(Flash_Transaction_Queue* sourceQueue1, bool suspensionRequired) {
		Flash_Transaction_Queue sourceQueue1_copy;
		transaction_dispatch_slots.clear();

		if (sourceQueue1 != NULL) {
			sourceQueue1_copy = *sourceQueue1;
		}

		std::list<TransactionBound*> transaction_bounds;
		unsigned int block_size = 16;

		NVM::FlashMemory::Physical_Page_Address addr;
		while (true) {
			if (sourceQueue1_copy.size() > 0) {
				addr = sourceQueue1_copy.front()->Address;
			}
			else {
				break;
			}
			PPA_type offset = (PPA_type)addr.BlockID * block_size + addr.PageID;
			sim_time_type minimum_initiate_time_of_tr_in_bound = MAXIMUM_TIME;

			if (sourceQueue1_copy.front()->UserIORequest != NULL) {
				minimum_initiate_time_of_tr_in_bound = (sim_time_type) sourceQueue1_copy.front()->UserIORequest->STAT_InitiationTime;
			}

			TransactionBound* tr_bound = new TransactionBound(offset, 0);
			for (Flash_Transaction_Queue::iterator it = sourceQueue1_copy.begin(); it != sourceQueue1_copy.end();) {
				if (transaction_is_ready(*it) && (*it)->Address.DieID == addr.DieID &&
					(PPA_type)(*it)->Address.BlockID * block_size + (*it)->Address.PageID == offset &&
					!(tr_bound->planeVector & 1 << (*it)->Address.PlaneID)) {

					if ((*it)->UserIORequest != NULL && (*it)->UserIORequest->STAT_InitiationTime < minimum_initiate_time_of_tr_in_bound) {
						minimum_initiate_time_of_tr_in_bound = (*it)->UserIORequest->STAT_InitiationTime;
					}

					(*it)->SuspendRequired = suspensionRequired;
					tr_bound->planeVector |= 1 << (*it)->Address.PlaneID;
					tr_bound->transaction_dispatch_slots.push_back(*it);
					sourceQueue1_copy.remove(it++);
					if (sourceQueue1_copy.size() < 1) {
						break;
					}
					continue;
				}
				it++;
			}

			tr_bound->minimum_initiate_time = minimum_initiate_time_of_tr_in_bound;
			transaction_bounds.push_back(tr_bound);
		}
		return transaction_bounds;
	}

	bool TSU_Base::issue_command_to_chip(Flash_Transaction_Queue* sourceQueue1, Flash_Transaction_Queue* sourceQueue2, Transaction_Type transactionType, bool suspensionRequired)
	{
		SSD_Components::Transaction_Source_Type source_type = sourceQueue1->front()->Source;
		switch (source_type)
		{
		case SSD_Components::Transaction_Source_Type::MAPPING:
		case SSD_Components::Transaction_Source_Type::GC_WL:
			return issue_command_to_chip_with_bounding(sourceQueue1, sourceQueue2, transactionType, suspensionRequired);
		case SSD_Components::Transaction_Source_Type::USERIO:
			//return issue_command_to_chip_using_RL(sourceQueue1, sourceQueue2, transactionType, suspensionRequired);
			if (transactionType == SSD_Components::Transaction_Type::READ) {
				return issue_command_to_chip_with_bounding(sourceQueue1, sourceQueue2, transactionType, suspensionRequired);
			}
			else {
				return issue_command_to_chip_using_RL(sourceQueue1, sourceQueue2, transactionType, suspensionRequired);
			}
		default:
			break;
		}

        // TODO: what is this?
        return false;

	}

	bool TSU_Base::issue_command_to_chip_using_RL(Flash_Transaction_Queue* sourceQueue1, Flash_Transaction_Queue* sourceQueue2, Transaction_Type transactionType, bool suspensionRequired)
	{
		std::list<TransactionBound*> transaction_bounds = bound_transactions(sourceQueue1, suspensionRequired);

		bool large_multiplane_found = false;
		
		sim_time_type minimum_initiate_time = MAXIMUM_TIME;

		if (transaction_bounds.size() > 0) {
			for (int i = 0; i < plane_no_per_die; i++) {
				for (std::list<TransactionBound*>::iterator bound = transaction_bounds.begin(); bound != transaction_bounds.end(); bound++) {
					if (get_multiplane_command_length((*bound)->planeVector) == plane_no_per_die - i) {
						if (minimum_initiate_time > (*bound)->minimum_initiate_time) {
							minimum_initiate_time = (*bound)->minimum_initiate_time;
							large_multiplane_found = true;
							transaction_dispatch_slots = (*bound)->transaction_dispatch_slots;
						}
					}
				}
			}

            for (auto& transaction_bound: transaction_bounds)
            {
                auto tr = transaction_bound->transaction_dispatch_slots.front()->Address;
                auto dieBKE = ftl->BlockManager->Get_die_bookkeeping_entry(tr);

                //dieBKE->users_intervals_map_
            }
			
			if (!large_multiplane_found) {
				SSD_Components::NVM_Transaction_Flash* tr = transaction_bounds.front()->transaction_dispatch_slots.front();
				if (tr->Address.BlockID == ftl->BlockManager->Get_die_bookkeeping_entry(tr->Address)->plane_manager_die->Data_wf[tr->Stream_id]->BlockID) {
					if (RL_decide_to_wait()) {
						//Simulator->Register_sim_event(Simulator->Time() + ftl)
					}
					else {
						transaction_dispatch_slots = transaction_bounds.front()->transaction_dispatch_slots;
					}
				}
				else {
					transaction_dispatch_slots = transaction_bounds.front()->transaction_dispatch_slots;
				}
			}
		}

		if (transaction_dispatch_slots.size() > 0) {
			for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_dispatch_slots.begin(); it != transaction_dispatch_slots.end(); it++) {
				std::list<NVM_Transaction_Flash*>::iterator tr_in_Q1;
				
				if (sourceQueue1 != NULL) {
					tr_in_Q1 = std::find(sourceQueue1->begin(), sourceQueue1->end(), (*it));
					if (tr_in_Q1 != sourceQueue1->end()) {
						sourceQueue1->remove(*it);
					}
				}
			}

			_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			transaction_dispatch_slots.clear();
			return true;
		}
		else {
			transaction_dispatch_slots.clear();
			return false;
		}
		return false;
	}

	bool TSU_Base::issue_command_to_chip_with_bounding(Flash_Transaction_Queue* sourceQueue1, Flash_Transaction_Queue* sourceQueue2, Transaction_Type transactionType, bool suspensionRequired)
	{
		
		std::list<TransactionBound*> transaction_bounds = bound_transactions(sourceQueue1, suspensionRequired);

		bool complete_multiplane_found = false;
		
		if (transaction_bounds.size() > 0) {
			for (std::list<TransactionBound*>::iterator bound = transaction_bounds.begin(); bound != transaction_bounds.end(); bound++) {
				if (get_multiplane_command_length((*bound)->planeVector) == plane_no_per_die) {
					complete_multiplane_found = true;
					transaction_dispatch_slots = (*bound)->transaction_dispatch_slots;
					break;
				}
			}
			if (!complete_multiplane_found) {
				transaction_dispatch_slots = transaction_bounds.front()->transaction_dispatch_slots;
			}
		}
		if (transaction_dispatch_slots.size() > 0) {
			for (std::list<NVM_Transaction_Flash*>::iterator it = transaction_dispatch_slots.begin(); it != transaction_dispatch_slots.end(); it++) {
				std::list<NVM_Transaction_Flash*>::iterator tr_in_Q1;
				
				if (sourceQueue1 != NULL) {
					tr_in_Q1 = std::find(sourceQueue1->begin(), sourceQueue1->end(), (*it));
					if (tr_in_Q1 != sourceQueue1->end()) {
						sourceQueue1->remove(*it);
					}
				}
			}

			_NVMController->Send_command_to_chip(transaction_dispatch_slots);
			transaction_dispatch_slots.clear();
			return true;
		}
		else {
			transaction_dispatch_slots.clear();
			return false;
		}
		return false;

	}

	//---------------------------------------------------------------------------------------------------------------------------
	bool TSU_Base::basic_issue_command_to_chip(Flash_Transaction_Queue *sourceQueue1, Flash_Transaction_Queue *sourceQueue2, Transaction_Type transactionType, bool suspensionRequired)
	{
		flash_die_ID_type dieID = sourceQueue1->front()->Address.DieID;
		flash_page_ID_type pageID = sourceQueue1->front()->Address.PageID;
		unsigned int planeVector = 0;
		static int issueCntr = 0;
		
		for (unsigned int i = 0; i < die_no_per_chip; i++)
		{
			transaction_dispatch_slots.clear();
			planeVector = 0;

			for (Flash_Transaction_Queue::iterator it = sourceQueue1->begin(); it != sourceQueue1->end();)
			{
				if (transaction_is_ready(*it) && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
				{
					//Check for identical pages when running multiplane command
					if (planeVector == 0 || (*it)->Address.PageID == pageID)
					{
						(*it)->SuspendRequired = suspensionRequired;
						planeVector |= 1 << (*it)->Address.PlaneID;
						transaction_dispatch_slots.push_back(*it);
						DEBUG(issueCntr++ << ": " << Simulator->Time() <<" Issueing Transaction - Type:" << TRTOSTR((*it)) << ", PPA:" << (*it)->PPA << ", LPA:" << (*it)->LPA << ", Channel: " << (*it)->Address.ChannelID << ", Chip: " << (*it)->Address.ChipID);
						sourceQueue1->remove(it++);
						continue;
					}
				}
				it++;
			}

			if (sourceQueue2 != NULL && transaction_dispatch_slots.size() < plane_no_per_die)
			{
				for (Flash_Transaction_Queue::iterator it = sourceQueue2->begin(); it != sourceQueue2->end();)
				{
					if (transaction_is_ready(*it) && (*it)->Address.DieID == dieID && !(planeVector & 1 << (*it)->Address.PlaneID))
					{
						//Check for identical pages when running multiplane command
						if (planeVector == 0 || (*it)->Address.PageID == pageID)
						{
							(*it)->SuspendRequired = suspensionRequired;
							planeVector |= 1 << (*it)->Address.PlaneID;
							transaction_dispatch_slots.push_back(*it);
							DEBUG(issueCntr++ << ": " << Simulator->Time() << " Issueing Transaction - Type:" << TRTOSTR((*it)) << ", PPA:" << (*it)->PPA << ", LPA:" << (*it)->LPA << ", Channel: " << (*it)->Address.ChannelID << ", Chip: " << (*it)->Address.ChipID);
							sourceQueue2->remove(it++);
							continue;
						}
					}
					it++;
				}
			}

			if (transaction_dispatch_slots.size() > 0)
			{
				_NVMController->Send_command_to_chip(transaction_dispatch_slots);
				transaction_dispatch_slots.clear();
				dieID = (dieID + 1) % die_no_per_chip;
				return true;
			}
			else
			{
				transaction_dispatch_slots.clear();
				dieID = (dieID + 1) % die_no_per_chip;
				return false;
			}			
		}

		return false;
	}
}