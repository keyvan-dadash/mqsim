#ifndef NVM_TRANSACTION_H
#define NVM_TRANSACTION_H

#include <list>
#include <memory>
#include "../sim/Sim_Defs.h"
#include "../sim/Engine.h"
#include "../rl/RL.h"
#include "User_Request.h"

namespace SSD_Components
{
	class User_Request;
	
	enum class Transaction_Type { READ, WRITE, ERASE, UNKOWN };
	enum class Transaction_Source_Type { USERIO, CACHE, GC_WL, MAPPING };

    /// ADDED BY S.O.D ///
    class GlobalCounter
    {
      public:
        static GlobalCounter* getInstance()
        {
            if (!instance_)
                instance_ = new GlobalCounter();
            
            return instance_;
        }

        int64_t getId()
        {
            return id++;
        }

      private:
        GlobalCounter()
        {

        }

        static GlobalCounter* instance_;

        int64_t id = 0;
    };

    struct NVM_Transaction_Infos
    {
        int64_t Id;
        stream_id_type Stream_id;
        sim_time_type Issue_time;
        sim_time_type STAT_execution_time, STAT_transfer_time;
        sim_time_type Submit_time;
        int64_t number_of_transaction_in_same_bound;
        int64_t bound_id;
        State init_state;
    };

	class NVM_Transaction
	{
	public:
		NVM_Transaction(stream_id_type stream_id, Transaction_Source_Type source, Transaction_Type type, User_Request* user_request, IO_Flow_Priority_Class::Priority priority_class) :
			Stream_id(stream_id), Source(source), Type(type), UserIORequest(user_request), Priority_class(priority_class), Issue_time(Simulator->Time()), STAT_execution_time(INVALID_TIME), STAT_transfer_time(INVALID_TIME) {}
		stream_id_type Stream_id;
		Transaction_Source_Type Source;
		Transaction_Type Type;
		User_Request* UserIORequest;
		IO_Flow_Priority_Class::Priority Priority_class;
		//std::list<NVM_Transaction*>::iterator RelatedNodeInQueue;//Just used for high performance linkedlist insertion/deletion

		sim_time_type Issue_time;
		/* Used to calculate service time and transfer time for a normal read/program operation used to respond to the host IORequests.
		In other words, these variables are not important if FlashTransactions is used for garbage collection.*/
		sim_time_type STAT_execution_time, STAT_transfer_time;

        /// ADDED BY S.O.D ///
        sim_time_type Submit_time = 0;

        int64_t number_of_transaction_in_same_bound = -1;

        int64_t bound_id = -1;

        int64_t id_in_bound = -1;

        State init_state;

        Action chosed_action;

        inline NVM_Transaction_Infos make_necessery_info()
        {
            return NVM_Transaction_Infos
            {
                .Id = this->id_in_bound,
                .Stream_id = this->Stream_id,
                .Issue_time = this->Issue_time,
                .STAT_execution_time = this->STAT_execution_time,
                .STAT_transfer_time = this->STAT_transfer_time,
                .Submit_time = this->Submit_time,
                .number_of_transaction_in_same_bound = this->number_of_transaction_in_same_bound,
                .bound_id = this->bound_id,
                .init_state = this->init_state
            };
        }

        inline void set_transaction_submit_time_now()
        {
            Submit_time = Simulator->Time();
        }

        inline void set_bound_id(ino64_t id)
        {
            bound_id = id;
        }

        inline void set_number_of_transaction_in_same_bound(ino64_t num)
        {
            number_of_transaction_in_same_bound = num;
        }
	};
}

#endif //!NVM_TRANSACTION_H
