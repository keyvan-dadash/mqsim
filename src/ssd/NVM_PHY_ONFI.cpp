#include "NVM_PHY_ONFI.h"

#include <iostream>
//#include "NVM_Transation.h"
#include <string>

namespace SSD_Components {
	void NVM_PHY_ONFI::ConnectToTransactionServicedSignal(TransactionServicedHandlerType function)
	{
		connectedTransactionServicedHandlers.push_back(function);
	}

    std::string get_transation_type_str(Transaction_Type Type)
    {
        switch (Type)
        {
        case Transaction_Type::READ:
            return std::string("READ");
        case Transaction_Type::WRITE:
            return std::string("WRITE");
        case Transaction_Type::ERASE:
            return std::string("ERASE");
        default:
            return std::string("UNKOWN");;
        }
    }

	/*
	* Different FTL components maybe waiting for a transaction to be finished:
	* HostInterface: For user reads and writes
	* Address_Mapping_Unit: For mapping reads and writes
	* TSU: For the reads that must be finished for partial writes (first read non updated parts of page data and then merge and write them into the new page)
	* GarbageCollector: For gc reads, writes, and erases
	*/
	void NVM_PHY_ONFI::broadcastTransactionServicedSignal(NVM_Transaction_Flash* transaction)
	{
        std::cout << get_transation_type_str(transaction->Type) << std::endl;
		for (std::vector<TransactionServicedHandlerType>::iterator it = connectedTransactionServicedHandlers.begin();
			it != connectedTransactionServicedHandlers.end(); it++) {
			(*it)(transaction);
		}
		delete transaction;//This transaction has been consumed and no more needed
	}

	void NVM_PHY_ONFI::ConnectToChannelIdleSignal(ChannelIdleHandlerType function)
	{
		connectedChannelIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChannelIdleSignal(flash_channel_ID_type channelID)
	{
		for (std::vector<ChannelIdleHandlerType>::iterator it = connectedChannelIdleHandlers.begin();
			it != connectedChannelIdleHandlers.end(); it++) {
			(*it)(channelID);
		}
	}

	void NVM_PHY_ONFI::ConnectToChipIdleSignal(ChipIdleHandlerType function)
	{
		connectedChipIdleHandlers.push_back(function);
	}

	void NVM_PHY_ONFI::broadcastChipIdleSignal(NVM::FlashMemory::Flash_Chip* chip)
	{
		for (std::vector<ChipIdleHandlerType>::iterator it = connectedChipIdleHandlers.begin();
			it != connectedChipIdleHandlers.end(); it++) {
			(*it)(chip);
		}
	}
}