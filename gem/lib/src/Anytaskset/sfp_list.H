#ifndef _SFP_LIST_H_
#define _SFP_LIST_H_

#define TStampListSize 24

template<typename T>
class TList
{

public:

  typedef char Iterator;
  typedef struct {
    T time;
    Iterator next;
  } Element;
  
  TList()
  {
    head = -1;
    for(int i=0; i < TStampListSize; i++)
    {
      impl[i].next = -1;
    }
  }

  inline Iterator begin()
  {
    return head;
  }

  inline Iterator is_end(Iterator i)
  {
    return i == -1;
  }

  inline Iterator next(Iterator i)
  {
    return impl[(int)i].next;
  }

  inline void erase(Iterator b, Iterator e)
  {
    impl[(int)b].next = impl[(int)e].next;
  }

  inline T& get(Iterator e)
  {
    return impl[(int)e].time;
  }

  inline T& set_at(Iterator e, const T& time)
  {
    return impl[(int)e].time = time;
  }

  inline void set_front(Iterator e)
  {
    if ( head == e ) return;
    impl[(int)e].next = head;
    head = e;
  }

  inline void remove_rest(Iterator e)
  {
    impl[(int)e].next = -1;
  }

private:

  Element impl[TStampListSize];
  Iterator head;

};

#endif
