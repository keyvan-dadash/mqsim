#include "TSU_OutofOrder.h"
#include <assert.h>
namespace SSD_Components
{

TSU_OutOfOrder::TSU_OutOfOrder(const sim_object_id_type &id, FTL *ftl, NVM_PHY_ONFI_NVDDR2 *NVMController, unsigned int ChannelCount, unsigned int chip_no_per_channel,
							   unsigned int DieNoPerChip, unsigned int PlaneNoPerDie,
							   sim_time_type WriteReasonableSuspensionTimeForRead,
							   sim_time_type EraseReasonableSuspensionTimeForRead,
							   sim_time_type EraseReasonableSuspensionTimeForWrite,
							   bool EraseSuspensionEnabled, bool ProgramSuspensionEnabled)
	: TSU_Base(id, ftl, NVMController, Flash_Scheduling_Type::OUT_OF_ORDER, ChannelCount, chip_no_per_channel, DieNoPerChip, PlaneNoPerDie,
			   WriteReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForRead, EraseReasonableSuspensionTimeForWrite,
			   EraseSuspensionEnabled, ProgramSuspensionEnabled)
{
	UserReadTRQueue = new Flash_Transaction_Queue *[channel_count];
	UserWriteTRQueue = new Flash_Transaction_Queue *[channel_count];
	GCReadTRQueue = new Flash_Transaction_Queue *[channel_count];
	GCWriteTRQueue = new Flash_Transaction_Queue *[channel_count];
	GCEraseTRQueue = new Flash_Transaction_Queue *[channel_count];
	MappingReadTRQueue = new Flash_Transaction_Queue *[channel_count];
	MappingWriteTRQueue = new Flash_Transaction_Queue *[channel_count];
	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		UserReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		UserWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		GCReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		GCWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		GCEraseTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		MappingReadTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		MappingWriteTRQueue[channelID] = new Flash_Transaction_Queue[chip_no_per_channel];
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			UserReadTRQueue[channelID][chip_cntr].Set_id("User_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			UserWriteTRQueue[channelID][chip_cntr].Set_id("User_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			GCReadTRQueue[channelID][chip_cntr].Set_id("GC_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			MappingReadTRQueue[channelID][chip_cntr].Set_id("Mapping_Read_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			MappingWriteTRQueue[channelID][chip_cntr].Set_id("Mapping_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			GCWriteTRQueue[channelID][chip_cntr].Set_id("GC_Write_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
			GCEraseTRQueue[channelID][chip_cntr].Set_id("GC_Erase_TR_Queue@" + std::to_string(channelID) + "@" + std::to_string(chip_cntr));
		}
	}
}

TSU_OutOfOrder::~TSU_OutOfOrder()
{
	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		delete[] UserReadTRQueue[channelID];
		delete[] UserWriteTRQueue[channelID];
		delete[] GCReadTRQueue[channelID];
		delete[] GCWriteTRQueue[channelID];
		delete[] GCEraseTRQueue[channelID];
		delete[] MappingReadTRQueue[channelID];
		delete[] MappingWriteTRQueue[channelID];
	}
	delete[] UserReadTRQueue;
	delete[] UserWriteTRQueue;
	delete[] GCReadTRQueue;
	delete[] GCWriteTRQueue;
	delete[] GCEraseTRQueue;
	delete[] MappingReadTRQueue;
	delete[] MappingWriteTRQueue;
}

void TSU_OutOfOrder::Start_simulation()
{
}

void TSU_OutOfOrder::Validate_simulation_config()
{
}

void TSU_OutOfOrder::Execute_simulator_event(MQSimEngine::Sim_Event *event)
{
    // std::cout << "hello out" << std::endl;
    auto dieBKE = (DieBookKeepingEntry*)(event->Parameters);
    dieBKE->should_die_schedule_fast = true;
    dieBKE->should_die_wait = false;
    auto chip = dieBKE->releated_chip;
    waiting_chip.push_back(chip);

    auto current_write_queue = UserWriteTRQueue[chip->ChannelID][chip->ChipID];
    // TODO: should we push new tr?
    for (std::list<NVM_Transaction_Flash *>::iterator it = transaction_receive_slots.begin(); it != transaction_receive_slots.end(); it++)
	{
		switch ((*it)->Type)
		{
		case Transaction_Type::READ:
			break;
		case Transaction_Type::WRITE:
			switch ((*it)->Source)
			{
			case Transaction_Source_Type::CACHE:
			case Transaction_Source_Type::USERIO: {
                if ((*it)->Address.ChannelID == chip->ChannelID && (*it)->Address.ChipID == chip->ChipID)
				current_write_queue.push_back((*it));
				break;
            }
			case Transaction_Source_Type::MAPPING:
			case Transaction_Source_Type::GC_WL:
				break;
			default:
				PRINT_ERROR("TSU_OutOfOrder: unknown source type for a write transaction!")
			}
			break;
		case Transaction_Type::ERASE:
			break;
		default:
			break;
		}
	}

    Flash_Transaction_Queue *sourceQueue1 = &current_write_queue;

    // TODO: is this right?
    extract_and_save_intervals(sourceQueue1);
    bool found_desire_die = false;
    std::list<TransactionBound*> transaction_bounds = bound_transactions(sourceQueue1, false);
    assert(transaction_bounds.size() > 0);
    stream_id_type stream_id;
    NVM_Transaction_Flash* transaction;
    State init_state;
    State next_state;
    // TODO: what about rewards? should be hardcoded
    float reward = 1;

    // std::cout << dieBKE << "\n----------------------- " << transaction_bounds.size() << std::endl;
    for (std::list<TransactionBound*>::iterator bound = transaction_bounds.begin(); bound != transaction_bounds.end(); bound++) {
        auto tmp_dieBKE = _NVMController->GetDieBookKeepingEntryFromTransations((*bound)->transaction_dispatch_slots.front());
        // std::cout << tmp_dieBKE << std::endl;
        if (dieBKE != tmp_dieBKE)
            continue;
        found_desire_die = true;
        auto tmp_transaction_dispatch_slots = (*bound)->transaction_dispatch_slots;
        transaction = tmp_transaction_dispatch_slots.front();

        stream_id = transaction->Stream_id;
        auto user_intervals = dieBKE->users_intervals_map_.find(stream_id);

        auto current_interval = user_intervals->second->current_interval;
        auto pervious_interval = user_intervals->second->perivous_interval;

        // TODO: we didnt set init state
        init_state = transaction->init_state;
        // TODO: is next_state correct? or we should fetch again from the source queue?
        next_state = State::GetStateFrom(current_interval, pervious_interval);

        assert(tmp_transaction_dispatch_slots.size() >= transaction->number_of_tr_during_action);
        if (tmp_transaction_dispatch_slots.size() == transaction->number_of_tr_during_action)
        {
            // bad reward
            reward = -1;
        }
        else
        {
            // std::cout << "hoho" << std::endl;
        }
        for (const auto& tran : tmp_transaction_dispatch_slots)
        {
            tran->does_reward_calculated = true;
        }

        break;
    }
    assert(found_desire_die == true);

    auto user_agent = dieBKE->users_agent.find(stream_id);

    // TODO: Check whetever is this possible or not
    if (user_agent == dieBKE->users_agent.end())
    {
        dieBKE->users_agent[stream_id] = new Agent();
        user_agent = dieBKE->users_agent.find(stream_id);
    }

    user_agent->second->updateQ(init_state, next_state, reward, transaction->chosed_action);
}

void TSU_OutOfOrder::Report_results_in_XML(std::string name_prefix, Utils::XmlWriter &xmlwriter)
{
	name_prefix = name_prefix + ".TSU";
	xmlwriter.Write_open_tag(name_prefix);

	TSU_Base::Report_results_in_XML(name_prefix, xmlwriter);

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			UserReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Read_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			UserWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".User_Write_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			MappingReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Read_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			MappingWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".Mapping_Write_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			GCReadTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Read_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			GCWriteTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Write_TR_Queue", xmlwriter);
		}
	}

	for (unsigned int channelID = 0; channelID < channel_count; channelID++)
	{
		for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++)
		{
			GCEraseTRQueue[channelID][chip_cntr].Report_results_in_XML(name_prefix + ".GC_Erase_TR_Queue", xmlwriter);
		}
	}

	xmlwriter.Write_close_tag();
}

void TSU_OutOfOrder::Schedule(bool should_skip_validation)
{
    if (!should_skip_validation) {
        opened_scheduling_reqs--;
        if (opened_scheduling_reqs > 0)
        {
            return;
        }

        if (opened_scheduling_reqs < 0)
        {
            PRINT_ERROR("TSU_OutOfOrder: Illegal status!");
        }

        if (transaction_receive_slots.size() == 0)
        {
            return;
        }
    }

    // bool is_unvalid = false;
    // for (auto it = transaction_receive_slots.begin(); it != transaction_receive_slots.end(); it++)
    // {
    //     std::cout << (*it)->Submit_time << " " << last << std::endl;
    //     if ((*it)->Submit_time < last)
    //         assert(false);
    //         // is_unvalid = true;
    //     last = (*it)->Submit_time;
    // }
    // // if (is_unvalid)
    // //     std::cout << "HERE";
    // std::cout << "\n---------------------------------------------" << std::endl;

	for (std::list<NVM_Transaction_Flash *>::iterator it = transaction_receive_slots.begin(); it != transaction_receive_slots.end(); it++)
	{
		switch ((*it)->Type)
		{
		case Transaction_Type::READ:
			switch ((*it)->Source)
			{
			case Transaction_Source_Type::CACHE:
			case Transaction_Source_Type::USERIO:
				UserReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			case Transaction_Source_Type::MAPPING:
				MappingReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			case Transaction_Source_Type::GC_WL:
				GCReadTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			default:
				PRINT_ERROR("TSU_OutOfOrder: unknown source type for a read transaction!")
			}
			break;
		case Transaction_Type::WRITE:
			switch ((*it)->Source)
			{
			case Transaction_Source_Type::CACHE:
			case Transaction_Source_Type::USERIO:
				UserWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			case Transaction_Source_Type::MAPPING:
				MappingWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			case Transaction_Source_Type::GC_WL:
				GCWriteTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
				break;
			default:
				PRINT_ERROR("TSU_OutOfOrder: unknown source type for a write transaction!")
			}
			break;
		case Transaction_Type::ERASE:
			GCEraseTRQueue[(*it)->Address.ChannelID][(*it)->Address.ChipID].push_back((*it));
			break;
		default:
			break;
		}
	}

    for (auto it = waiting_chip.begin(); it != waiting_chip.end(); it++) {
        if (_NVMController->Get_channel_status((*it)->ChannelID) == BusChannelStatus::IDLE) {
            process_chip_requests((*it));
            waiting_chip.erase(it);
            return;
        }
    }

	for (flash_channel_ID_type channelID = 0; channelID < channel_count; channelID++)
	{
		if (_NVMController->Get_channel_status(channelID) == BusChannelStatus::IDLE)
		{
			for (unsigned int i = 0; i < chip_no_per_channel; i++)
			{
				NVM::FlashMemory::Flash_Chip *chip = _NVMController->Get_chip(channelID, Round_robin_turn_of_channel[channelID]);
				//The TSU does not check if the chip is idle or not since it is possible to suspend a busy chip and issue a new command
				process_chip_requests(chip);
				Round_robin_turn_of_channel[channelID] = (flash_chip_ID_type)(Round_robin_turn_of_channel[channelID] + 1) % chip_no_per_channel;
				if (_NVMController->Get_channel_status(chip->ChannelID) != BusChannelStatus::IDLE)
				{
					break;
				}
			}
		}
	}
}

bool TSU_OutOfOrder::service_read_transaction(NVM::FlashMemory::Flash_Chip *chip)
{
	Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

	//Flash transactions that are related to FTL mapping data have the highest priority
	if (MappingReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
	{
		sourceQueue1 = &MappingReadTRQueue[chip->ChannelID][chip->ChipID];
		if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip) && GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
		}
		else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
		}
	}
	else if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))
	{
		//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first

		if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue2 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			}
		}
		else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			return false;
		}
		else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			return false;
		}
		else if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
		}
		else
		{
			return false;
		}
	}
	else
	{
		//If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first

		if (UserReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &UserReadTRQueue[chip->ChannelID][chip->ChipID];
			if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue2 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
			}
		}
		else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			return false;
		}
		else if (GCReadTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &GCReadTRQueue[chip->ChannelID][chip->ChipID];
		}
		else
		{
			return false;
		}
	}

	bool suspensionRequired = false;
	ChipStatus cs = _NVMController->GetChipStatus(chip);
	switch (cs)
	{
	case ChipStatus::IDLE:
		break;
	case ChipStatus::WRITING:
		if (!programSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
		{
			return false;
		}
		if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < writeReasonableSuspensionTimeForRead)
		{
			return false;
		}
		suspensionRequired = true;
	case ChipStatus::ERASING:
		if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
		{
			return false;
		}
		if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForRead)
		{
			return false;
		}
		suspensionRequired = true;
	default:
		return false;
	}

	issue_command_to_chip(sourceQueue1, sourceQueue2, Transaction_Type::READ, suspensionRequired);

	return true;
}

bool TSU_OutOfOrder::service_write_transaction(NVM::FlashMemory::Flash_Chip *chip)
{
	Flash_Transaction_Queue *sourceQueue1 = NULL, *sourceQueue2 = NULL;

	//If flash transactions related to GC are prioritzed (non-preemptive execution mode of GC), then GC queues are checked first
	if (ftl->GC_and_WL_Unit->GC_is_in_urgent_mode(chip))
	{
		if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue2 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
		}
		else if (GCEraseTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			return false;
		}
		else if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
		}
		else
		{
			return false;
		}
	}
	else
	{
		//If GC is currently executed in the preemptive mode, then user IO transaction queues are checked first
		if (UserWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &UserWriteTRQueue[chip->ChannelID][chip->ChipID];
			if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
			{
				sourceQueue2 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
			}
		}
		else if (GCWriteTRQueue[chip->ChannelID][chip->ChipID].size() > 0)
		{
			sourceQueue1 = &GCWriteTRQueue[chip->ChannelID][chip->ChipID];
		}
		else
		{
			return false;
		}
	}

	bool suspensionRequired = false;
	ChipStatus cs = _NVMController->GetChipStatus(chip);
	switch (cs)
	{
	case ChipStatus::IDLE:
		break;
	case ChipStatus::ERASING:
		if (!eraseSuspensionEnabled || _NVMController->HasSuspendedCommand(chip))
			return false;
		if (_NVMController->Expected_finish_time(chip) - Simulator->Time() < eraseReasonableSuspensionTimeForWrite)
			return false;
		suspensionRequired = true;
	default:
		return false;
	}

	issue_command_to_chip(sourceQueue1, sourceQueue2, Transaction_Type::WRITE, suspensionRequired);

	return true;
}

bool TSU_OutOfOrder::service_erase_transaction(NVM::FlashMemory::Flash_Chip *chip)
{
	if (_NVMController->GetChipStatus(chip) != ChipStatus::IDLE)
	{
		return false;
	}

	Flash_Transaction_Queue *source_queue = &GCEraseTRQueue[chip->ChannelID][chip->ChipID];
	if (source_queue->size() == 0)
	{
		return false;
	}

	issue_command_to_chip(source_queue, NULL, Transaction_Type::ERASE, false);

	return true;
}

/// ADDED BY S.O.D ///
Flash_Transaction_Queue* TSU_OutOfOrder::GetSourceQueue(NVM::FlashMemory::Flash_Chip *chip, Flash_Transaction_Queue& sourceQueue1)
{
    sourceQueue1 = UserWriteTRQueue[chip->ChannelID][chip->ChipID];

    for (std::list<NVM_Transaction_Flash *>::iterator it = transaction_receive_slots.begin(); it != transaction_receive_slots.end(); it++)
	{
		switch ((*it)->Type)
		{
		case Transaction_Type::READ:
			break;
		case Transaction_Type::WRITE:
			switch ((*it)->Source)
			{
			case Transaction_Source_Type::CACHE:
			case Transaction_Source_Type::USERIO: {
                if ((*it)->Address.ChannelID == chip->ChannelID && (*it)->Address.ChipID == chip->ChipID)
				sourceQueue1.push_back((*it));
				break;
            }
			case Transaction_Source_Type::MAPPING:
			case Transaction_Source_Type::GC_WL:
				break;
			default:
				PRINT_ERROR("TSU_OutOfOrder: unknown source type for a write transaction!")
			}
			break;
		case Transaction_Type::ERASE:
			break;
		default:
			break;
		}
	}

    return NULL;
}

} // namespace SSD_Components
