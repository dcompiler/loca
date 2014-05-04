#ifndef SFP_TASKMGR_H
#define SFP_TASKMGR_H

#include <stack>
#include "common.H"
#include "rdtsc.H"

#define MAX_TOKENS 26

typedef unsigned short TToken;

struct TTaskDesc
{
  TTaskDesc(int _taskid) : taskid(_taskid)
  {
    end_time = start_time = SFP_RDTSC();
  }
   
  int taskid;
  TToken token;
  TStamp start_time;
  TStamp end_time;
};

class TTokenManager
{

private:

  /**
   * Copy Constructor
   * \param [in] other The object from which to copy construct
   */
  TTokenManager (const TTokenManager& other);

  /**
   * operator=
   * \param [in] other The object from which to copy construct
   */
  void operator= (const TTokenManager& other);

public:
  /**
   * Constructor
   * This is the only means of constructing this object
   */
  TTokenManager() {}

  inline bool get_token(TTaskDesc* td, const TToken parent)
  {

    for(unsigned int i=0;i<MAX_TOKENS;i++)
    {
      if(!task_stack[i].empty())
      {
        current_task_map[i] = td;
        task_stack[i].push(td);
        td->token = (TToken)i;
        return true;
      }
    }

    current_task_map[parent] = td;
    task_stack[parent].push(td);
    td->token = parent;
    return false;
  }

  inline bool release_token(TToken token)
  {
    task_stack[token].pop();
    if(task_stack[token].empty())
    {
      current_task_map[token] = 0;
      return true;
    }
    current_task_map[token] = task_stack[token].top();
    return false;
  }

  inline TTaskDesc* taskid_to_taskdesc(int taskid)
  {
    return taskdesc_map[taskid];
  }

  inline TToken taskid_to_token(int taskid)
  {
    return taskdesc_map[taskid]->token;
  }

  inline TTaskDesc* token_to_task(TToken token)
  {
    return current_task_map[token];
  }

  inline void set_end_time(TToken token, TStamp ts)
  {
    current_task_map[token]->end_time = ts;
  }

  inline TTaskDesc* get_task_descriptor(int taskid)
  {
    if ( taskdesc_map.find(taskid) != taskdesc_map.end() )
      return taskdesc_map[taskid];

    TTaskDesc* td = new TTaskDesc(taskid);
    taskdesc_map[taskid] = td;
    return td;
  }

private:

  TTaskDesc* current_task_map[MAX_TOKENS];
  std::map<int, TTaskDesc*> taskdesc_map;
  std::stack<TTaskDesc*> task_stack[MAX_TOKENS];

};

#endif
