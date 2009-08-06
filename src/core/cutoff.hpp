namespace nanos {

class cutoff;

//abstract classes for info about cutoff and cutoff checking
class cutoff_info
{
    friend class cutoff;
};



class cutoff
{
public:
    virtual void init() = 0;
    virtual bool cutoff_pred(cutoff_info *); 
    virtual ~cutoff() = 0;
};

}