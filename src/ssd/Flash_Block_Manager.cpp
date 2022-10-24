
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "Flash_Block_Manager.h"
#include "Stats.h"

namespace SSD_Components
{
	Flash_Block_Manager::Flash_Block_Manager(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: Flash_Block_Manager_Base(gc_and_wl_unit, max_allowed_block_erase_count, total_concurrent_streams_no, channel_count, chip_no_per_channel, die_no_per_chip,
			plane_no_per_die, block_no_per_plane, page_no_per_block)
	{
	}

	Flash_Block_Manager::~Flash_Block_Manager()
	{
	}

	//modified to allocate plane, block and page for user write with superblock concept
	//-------------------------------------- MODIFIED BY MAEE ---------------------------------------------------------------------------------
	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		DieBookKeepingType* dbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID];

		page_address.PlaneID = dbke->Data_swf[stream_id]->Current_Plane_write_index;
		page_address.BlockID = dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->BlockID;
		page_address.PageID = dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index;
		
		//----------------------------------------------- DEBUG_BY_MAEE ------------------------------------------------------
		bool target_plane_debug_enabled = (page_address.ChannelID == target_debug_channel && page_address.ChipID == target_debug_chip && page_address.DieID == target_debug_die);
		if (target_plane_debug_enabled) {
			_debug_by_maee(
				std::ofstream debug_file;
			debug_file.open("debug_file.txt", std::ios_base::app);
			debug_file << "Current plane write index: " << dbke->Data_swf[stream_id]->Current_Plane_write_index << std::endl;
			debug_file << "Data_SWF of user " << stream_id << " in Die " << page_address.ChannelID << ", " << page_address.ChipID << ", " <<
				page_address.DieID << " is: " << dbke->Data_swf[stream_id]->SuperblockID << "." << std::endl;
			debug_file << "target block: " << page_address.BlockID << std::endl;
			for (int i = 0; i < plane_no_per_die; i++) {
				debug_file << "Blocks of Data_SWF are: " << dbke->Data_swf[stream_id]->Blocks[i]->BlockID << ", ";
			}
			debug_file << std::endl;
			debug_file << "Current page write index in target block: " << dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index << std::endl;
			debug_file.close();
			)
		}
		//--------------------------------------------------------------------------------------------------------------------

		if (dbke->Data_swf[stream_id]->Current_Plane_write_index < dbke->plane_no_per_die - 1) {
			dbke->Data_swf[stream_id]->Current_Plane_write_index++;
		}
		else if (dbke->Data_swf[stream_id]->Current_Plane_write_index == dbke->plane_no_per_die - 1) {
			if (dbke->Data_swf[stream_id]->Blocks[0]->Current_page_write_index < dbke->Data_swf[stream_id]->Blocks[0]->pages_no_per_block - 1) {
				dbke->Data_swf[stream_id]->Current_Plane_write_index = 0;
			}
		}

		if (dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index < dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->pages_no_per_block - 1) {
			dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index++;
		}

		//----------------------------------------------- DEBUG_BY_MAEE ------------------------------------------------------
		if (target_plane_debug_enabled) {
			_debug_by_maee(
				std::ofstream debug_file;
			debug_file.open("debug_file.txt", std::ios_base::app);
			debug_file << "Current plane write index after increasing: " << dbke->Data_swf[stream_id]->Current_Plane_write_index << std::endl;
			debug_file << "Current page write index in target block after increasing: " << dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index << std::endl;
			debug_file << "Ongoing program transaction count: " << dbke->plane_manager_die[page_address.PlaneID].Blocks[page_address.BlockID].Ongoing_user_program_count << std::endl;
			debug_file << "Ongoing program transaction count: " << dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Ongoing_user_program_count << std::endl;
			debug_file.close();
			)
		}
		//--------------------------------------------------------------------------------------------------------------------

		PlaneBookKeepingType* pbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		pbke->Valid_pages_count++;
		pbke->Free_pages_count--;
		program_transaction_issued(page_address);

		//----------------------------------------------- DEBUG_BY_MAEE ------------------------------------------------------
		if (target_plane_debug_enabled) {
			_debug_by_maee(
				std::ofstream debug_file;
			debug_file.open("debug_file.txt", std::ios_base::app);
			debug_file << "new Ongoing program transaction count: " << dbke->plane_manager_die[page_address.PlaneID].Blocks[page_address.BlockID].Ongoing_user_program_count << std::endl;
			debug_file << "new Ongoing program transaction count: " << dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Ongoing_user_program_count << std::endl;
			debug_file.close();
			)
		}
		//--------------------------------------------------------------------------------------------------------------------

		if (dbke->Data_swf[stream_id]->Written_to_end()) {
			dbke->Data_swf[stream_id] = dbke->Get_a_free_superblock(stream_id, false);

			//----------------------------------------------- DEBUG_BY_MAEE ------------------------------------------------------
			if (target_plane_debug_enabled) {
				_debug_by_maee(
					std::ofstream debug_file;
				debug_file.open("debug_file.txt", std::ios_base::app);
				debug_file << "Data_SWF of user " << stream_id << " has written to end. Getting a new Data_SWF for this user." << std::endl;
				debug_file << "SuperBlock " << dbke->Data_swf[stream_id]->SuperblockID << " allocated as Data_SWF for user " << stream_id << ". " << std::endl;
				debug_file << "Current plane write index: " << dbke->Data_swf[stream_id]->Current_Plane_write_index << std::endl;
				debug_file << "Current page write index in target block: " << dbke->Data_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index << std::endl;
				debug_file.close();
				)
			}
			//--------------------------------------------------------------------------------------------------------------------

			gc_and_wl_unit->Check_gc_required_die(dbke->Get_free_superblock_pool_size(), page_address);
		}

		pbke->Check_bookkeeping_correctness(page_address);

			//PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
			//plane_record->Valid_pages_count++;
			//plane_record->Free_pages_count--;
			//page_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
			//page_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			//program_transaction_issued(page_address);

			////The current write frontier block is written to the end
			//if(plane_record->Data_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
			//	//Assign a new write frontier block
			//	plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
			//	gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
			//}

			//plane_record->Check_bookkeeping_correctness(page_address);


			// DieBookKeepingType *die_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID];
			// Superblock_Slot_Type *superblock_record = die_record->Data_swf[stream_id];
			// superblock_record->Valid_pages_count++;
			// superblock_record->Free_pages_count--;
			// تو بلاک های سوپربلاک با چه سیاستی قراره که نوشتن صورت بگیره
		//-----------------------------------------------------------------------------------------------------------------------------------
	}

	//modified for allocating plane, block and page for gc write with superblock concept
	//----------------------------------------- MODIFIED BY MAEE ------------------------------------------------------------------------
	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		DieBookKeepingType* dbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID];

		page_address.PlaneID = dbke->GC_swf[stream_id]->Current_Plane_write_index;
		page_address.BlockID = dbke->GC_swf[stream_id]->Blocks[page_address.PlaneID]->BlockID;
		page_address.PageID = dbke->GC_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index;

		if (dbke->GC_swf[stream_id]->Current_Plane_write_index < dbke->plane_no_per_die - 1) {
			dbke->GC_swf[stream_id]->Current_Plane_write_index++;
		}
		else if (dbke->GC_swf[stream_id]->Current_Plane_write_index == dbke->plane_no_per_die - 1) {
			if (dbke->GC_swf[stream_id]->Blocks[0]->Current_page_write_index < dbke->GC_swf[stream_id]->Blocks[0]->pages_no_per_block - 1) {
				dbke->GC_swf[stream_id]->Current_Plane_write_index = 0;
			}
		}

		if (dbke->GC_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index < dbke->GC_swf[stream_id]->Blocks[page_address.PlaneID]->pages_no_per_block - 1) {
			dbke->GC_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index++;
		}

		PlaneBookKeepingType* pbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		pbke->Valid_pages_count++;
		pbke->Free_pages_count--;

		if (dbke->GC_swf[stream_id]->Written_to_end()) {
			dbke->GC_swf[stream_id] = dbke->Get_a_free_superblock(stream_id, false);
			gc_and_wl_unit->Check_gc_required_die(dbke->Get_free_superblock_pool_size(), page_address);
		}

		pbke->Check_bookkeeping_correctness(page_address);


		//PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		//plane_record->Valid_pages_count++;
		//plane_record->Free_pages_count--;		
		//page_address.BlockID = plane_record->GC_wf[stream_id]->BlockID;
		//page_address.PageID = plane_record->GC_wf[stream_id]->Current_page_write_index++;

		//
		////The current write frontier block is written to the end
		//if (plane_record->GC_wf[stream_id]->Current_page_write_index == pages_no_per_block) {
		//	//Assign a new write frontier block
		//	plane_record->GC_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
		//	gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		//}
		//plane_record->Check_bookkeeping_correctness(page_address);
	}
	//-------------------------------------------------------------------------------------------------------------------------------------

	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses)
	{
		if(page_addresses.size() > pages_no_per_block) {
			PRINT_ERROR("Error while precondition a physical block: the size of the address list is larger than the pages_no_per_block!")
		}
			
		PlaneBookKeepingType *plane_record = &die_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID].plane_manager_die[plane_address.PlaneID];
		if (plane_record->Data_wf[stream_id]->Current_page_write_index > 0) {
			PRINT_ERROR("Illegal operation: the Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning function should be executed for an erased block!")
		}

		//Assign physical addresses
		for (int i = 0; i < page_addresses.size(); i++) {
			plane_record->Valid_pages_count++;
			plane_record->Free_pages_count--;
			page_addresses[i].BlockID = plane_record->Data_wf[stream_id]->BlockID;
			page_addresses[i].PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			plane_record->Check_bookkeeping_correctness(page_addresses[i]);
		}

		//Invalidate the remaining pages in the block
		NVM::FlashMemory::Physical_Page_Address target_address(plane_address);
		while (plane_record->Data_wf[stream_id]->Current_page_write_index < pages_no_per_block) {
			plane_record->Free_pages_count--;
			target_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
			target_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
			plane_record->Check_bookkeeping_correctness(plane_address);
		}

		//Update the write frontier
		plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
		DieBookKeepingType* dbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID];

		page_address.PlaneID = dbke->Translation_swf[stream_id]->Current_Plane_write_index;
		page_address.BlockID = dbke->Translation_swf[stream_id]->Blocks[page_address.PlaneID]->BlockID;
		page_address.PageID = dbke->Translation_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index;

		if (dbke->Translation_swf[stream_id]->Current_Plane_write_index < dbke->plane_no_per_die - 1) {
			dbke->Translation_swf[stream_id]->Current_Plane_write_index++;
		}
		else if (dbke->Translation_swf[stream_id]->Current_Plane_write_index == dbke->plane_no_per_die - 1) {
			if (dbke->Translation_swf[stream_id]->Blocks[0]->Current_page_write_index < dbke->Translation_swf[stream_id]->Blocks[0]->pages_no_per_block - 1) {
				dbke->Translation_swf[stream_id]->Current_Plane_write_index = 0;
			}
		}

		if (dbke->Translation_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index < dbke->Translation_swf[stream_id]->Blocks[page_address.PlaneID]->pages_no_per_block - 1) {
			dbke->Translation_swf[stream_id]->Blocks[page_address.PlaneID]->Current_page_write_index++;
		}

		PlaneBookKeepingType* pbke = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		pbke->Valid_pages_count++;
		pbke->Free_pages_count--;
		program_transaction_issued(page_address);

		if (dbke->Translation_swf[stream_id]->Written_to_end()) {
			dbke->Translation_swf[stream_id] = dbke->Get_a_free_superblock(stream_id, true);
			if (!is_for_gc) {
				gc_and_wl_unit->Check_gc_required_die(dbke->Get_free_superblock_pool_size(), page_address);
			}
		}

		pbke->Check_bookkeeping_correctness(page_address);

		//PlaneBookKeepingType *plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		//plane_record->Valid_pages_count++;
		//plane_record->Free_pages_count--;
		//page_address.BlockID = plane_record->Translation_wf[streamID]->BlockID;
		//page_address.PageID = plane_record->Translation_wf[streamID]->Current_page_write_index++;
		//program_transaction_issued(page_address);

		////The current write frontier block for translation pages is written to the end
		//if (plane_record->Translation_wf[streamID]->Current_page_write_index == pages_no_per_block) {
		//	//Assign a new write frontier block
		//	plane_record->Translation_wf[streamID] = plane_record->Get_a_free_block(streamID, true);
		//	if (!is_for_gc) {
		//		gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		//	}
		//}
		//plane_record->Check_bookkeeping_correctness(page_address);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		plane_record->Valid_pages_count--;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &die_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID].plane_manager_die[page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	//------------------------------------ ADDED BY MAEE --------------------------------------------------------------
	void Flash_Block_Manager::check_if_a_superblock_erased(const NVM::FlashMemory::Physical_Page_Address& block_address) {
		DieBookKeepingType* die_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID];
		PlaneBookKeepingType* plane_record;

		for (int i = 0; i < plane_no_per_die; i++) {
			plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[i];
			int flag = 0;

			for (std::multimap<unsigned int, SSD_Components::Block_Pool_Slot_Type*>::iterator it = plane_record->Free_block_pool.begin(); it != plane_record->Free_block_pool.end(); it++) {
				if (it->second->BlockID == block_address.BlockID) {
					flag = 1;
					break;
				}
			}
			if (flag == 0) {
				return;
			}
		}
		die_record->Add_to_free_superblock_pool(&die_record->Superblocks[block_address.BlockID], gc_and_wl_unit->Use_dynamic_wearleveling());
		die_record->Ongoing_erase_operations.erase(die_record->Ongoing_erase_operations.find(block_address.BlockID));
		GC_WL_finished_in_die(block_address);
		//--------------------------------------------- DEBUG BY MAEE ----------------------------------------------------
		bool target_plane_debug_enabled = (block_address.ChannelID == target_debug_channel && block_address.ChipID == target_debug_chip && block_address.DieID == target_debug_die);
		if (target_plane_debug_enabled) {
			_debug_by_maee(
				std::ofstream debug_file;
				debug_file.open("debug_file.txt", std::ios_base::app);
				debug_file << "Super Block : " << die_record->Superblocks[block_address.BlockID].SuperblockID << " of die " << block_address.ChannelID << ", " << block_address.ChipID << ", "
					<< block_address.DieID << " has been erased and added to free super block pool." << std::endl;
				debug_file.close();
			)
		}
		//-----------------------------------------------------------------------------------------------------------------

	}

	//----------------------------------------------------------------------------------------------------------------

	void Flash_Block_Manager::Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &die_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID].plane_manager_die[block_address.PlaneID];
		Block_Pool_Slot_Type* block = &(plane_record->Blocks[block_address.BlockID]);
		plane_record->Free_pages_count += block->Invalid_page_count;
		plane_record->Invalid_pages_count -= block->Invalid_page_count;

		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]--;
		block->Erase();
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]++;
		plane_record->Add_to_free_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());

		//--------------------------------------------- DEBUG BY MAEE ----------------------------------------------------
		bool target_plane_debug_enabled = (block_address.ChannelID == target_debug_channel && block_address.ChipID == target_debug_chip && block_address.DieID == target_debug_die);
		if (target_plane_debug_enabled) {
			_debug_by_maee(
				std::ofstream debug_file;
			debug_file.open("debug_file.txt", std::ios_base::app);
			debug_file << "Block : " << block_address.BlockID << " of plane " << block_address.ChannelID << ", " << block_address.ChipID << ", "
				<< block_address.DieID << ", " << block_address.PlaneID << " has been erased and added to free block pool." << std::endl;
			debug_file.close();
			)
		}
		//-----------------------------------------------------------------------------------------------------------------

		plane_record->Check_bookkeeping_correctness(block_address);
	}

	inline unsigned int Flash_Block_Manager::Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return (unsigned int) die_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID].plane_manager_die[plane_address.PlaneID].Free_block_pool.size();
	}
}
